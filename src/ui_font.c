#include "ui_font.h"
#include <stddef.h>
#include <stdio.h>
#include <string.h>

static Font g_font;          // body: dense, small, has to stay legible
static Font g_display;       // display: titles, headers, zone names
static bool g_fontLoaded = false;
static bool g_displayLoaded = false;

// Loads the first candidate that exists, at `rasterSize`. Shared by both
// faces so the body and display fonts can't end up on different paths.
static bool LoadFirst(Font *out, const char **candidates, int count, int rasterSize) {
    for (int i = 0; i < count; i++) {
        if (!candidates[i] || !FileExists(candidates[i])) continue;
        Font f = LoadFontEx(candidates[i], rasterSize, NULL, 0);
        if (f.texture.id == 0) continue;
        // Bilinear so downscaled text stays smooth instead of chunky
        // nearest-neighbor.
        SetTextureFilter(f.texture, TEXTURE_FILTER_BILINEAR);
        *out = f;
        return true;
    }
    return false;
}

void UIFont_Init(void) {
    // Rasterize the atlas to match the monitor: UI text is drawn at up
    // to ~5x the 800px-reference sizes on a 4K display, and upscaling a
    // fixed 48px atlas there turns soft. Scale the raster size with the
    // monitor (clamped - a 95-glyph atlas at 160px is still tiny).
    int monitorH = GetMonitorHeight(GetCurrentMonitor());
    if (monitorH <= 0) monitorH = (int)UI_REFERENCE_HEIGHT;
    int rasterSize = (int)(48.0f * ((float)monitorH / UI_REFERENCE_HEIGHT));
    if (rasterSize < 48) rasterSize = 48;
    if (rasterSize > 160) rasterSize = 160;

    // The bundled font ships in assets/ next to the executable (CMake
    // copies it post-build), so text looks the same on every machine.
    // System fonts are only a fallback for running a bare binary.
    // The display face carries the fantasy - GW1 sets its titles and
    // window headers in a Roman capital serif, and Cinzel is that. It is
    // NOT used for body text: at 10-12px a high-contrast serif in all
    // caps stops being readable, which is what the body face is for.
    char displayBundled[512], displayRepo[512];
    snprintf(displayBundled, sizeof(displayBundled), "%sassets/fonts/Cinzel-SemiBold.ttf",
             GetApplicationDirectory());
    snprintf(displayRepo, sizeof(displayRepo), "assets/fonts/Cinzel-SemiBold.ttf");
    const char *displayCandidates[] = { displayBundled, displayRepo };
    g_displayLoaded = LoadFirst(&g_display, displayCandidates, 2, rasterSize);

    char bundled[512], bodyRepo[512], bundledDejaVu[512];
    // Alegreya Sans first: a humanist sans that sits with the serif
    // instead of fighting it, and stays clean down to a 10px list row.
    // DejaVu stays behind it as the fallback it always was.
    snprintf(bundled, sizeof(bundled), "%sassets/fonts/AlegreyaSans-Medium.ttf",
             GetApplicationDirectory()); // includes the trailing separator
    snprintf(bodyRepo, sizeof(bodyRepo), "assets/fonts/AlegreyaSans-Medium.ttf");
    snprintf(bundledDejaVu, sizeof(bundledDejaVu), "%sassets/fonts/DejaVuSans.ttf",
             GetApplicationDirectory());

    const char *candidates[] = {
        bundled,
        bodyRepo,
        bundledDejaVu,
        "assets/fonts/DejaVuSans.ttf", // running from the repo root
        // Linux
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/TTF/DejaVuSans.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
        // macOS
        "/System/Library/Fonts/Supplemental/Arial.ttf",
        "/System/Library/Fonts/Supplemental/Helvetica.ttf",
        "/Library/Fonts/Arial.ttf",
        // Windows
        "C:/Windows/Fonts/segoeui.ttf",
        "C:/Windows/Fonts/arial.ttf",
    };

    g_fontLoaded = LoadFirst(&g_font, candidates,
                             (int)(sizeof(candidates) / sizeof(candidates[0])), rasterSize);
    // No font found anywhere - raylib's built-in font still works, just
    // blockier. UIText falls back automatically. Same for the display
    // face, which falls back to the body font rather than to nothing.
}

void UIText(const char *text, int x, int y, int size, Color color) {
    if (g_fontLoaded) {
        DrawTextEx(g_font, text, (Vector2){ (float)x, (float)y }, (float)size, size * 0.04f, color);
    } else {
        DrawText(text, x, y, size, color);
    }
}

void UITextDisplay(const char *text, int x, int y, int size, Color color) {
    if (g_displayLoaded) {
        // A little tracking: Roman capitals want air between them, and
        // it's what makes a title read as carved rather than typed.
        DrawTextEx(g_display, text, (Vector2){ (float)x, (float)y }, (float)size,
                   size * 0.08f, color);
        return;
    }
    UIText(text, x, y, size, color);
}

int UITextDisplayWidth(const char *text, int size) {
    if (g_displayLoaded) {
        return (int)MeasureTextEx(g_display, text, (float)size, size * 0.08f).x;
    }
    return UITextWidth(text, size);
}

int UITextWidth(const char *text, int size) {
    if (g_fontLoaded) {
        return (int)MeasureTextEx(g_font, text, (float)size, size * 0.04f).x;
    }
    return MeasureText(text, size);
}

int UITextWrapped(const char *text, int x, int y, int size, int maxWidth,
                  int lineGap, Color color, bool draw) {
    if (!text || !text[0] || maxWidth <= 0) return 0;
    int adv = size + lineGap;
    int lines = 0;
    char line[512] = "";   // the line being assembled
    const char *p = text;
    while (*p) {
        if (*p == '\n') {  // hard break: emit the line and reset
            if (draw) UIText(line, x, y + lines * adv, size, color);
            lines++; line[0] = '\0'; p++;
            continue;
        }
        while (*p == ' ') p++;               // collapse runs of spaces
        char word[256];
        int wl = 0;
        while (*p && *p != ' ' && *p != '\n' && wl < (int)sizeof(word) - 1) word[wl++] = *p++;
        word[wl] = '\0';
        if (wl == 0) continue;
        char cand[512];
        if (line[0] == '\0') snprintf(cand, sizeof(cand), "%s", word);
        else snprintf(cand, sizeof(cand), "%s %s", line, word);
        // Keep the word if it fits, or if the line is empty (a single word
        // wider than the box has nowhere else to go).
        if (line[0] == '\0' || UITextWidth(cand, size) <= maxWidth) {
            snprintf(line, sizeof(line), "%s", cand);
        } else {
            if (draw) UIText(line, x, y + lines * adv, size, color);
            lines++;
            snprintf(line, sizeof(line), "%s", word);
        }
    }
    if (line[0] != '\0') {
        if (draw) UIText(line, x, y + lines * adv, size, color);
        lines++;
    }
    return lines;
}

float UI_Scale(int screenHeight) {
    float scale = (float)screenHeight / UI_REFERENCE_HEIGHT;
    if (scale < UI_MIN_SCALE) scale = UI_MIN_SCALE;
    if (scale > UI_MAX_SCALE) scale = UI_MAX_SCALE;
    return scale;
}

int UI_ScaledFontSize(int screenHeight, int baseSize) {
    return (int)(baseSize * UI_Scale(screenHeight));
}
