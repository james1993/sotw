#include "ui_searing.h"
#include "audio.h"
#include "character.h"
#include "raylib.h"
#include "ui_font.h"
#include "ui_nav.h"
#include "ui_theme.h"
#include <math.h>
#include <stdio.h>

// The sequence runs on a clock rather than on input, so it lands the
// same way every time. Skippable, but not before the crystals fall -
// the whole point is the moment the sky opens.
#define SEARING_SKY      2.2f   // the sky curdles
#define SEARING_IMPACT   3.4f   // crystals land
#define SEARING_TITLE    4.6f   // the word
#define SEARING_TEXT     6.0f   // the epilogue
#define SEARING_PROMPT   8.0f   // and the way out

static float g_t = 0.0f;

void UI_SearingReset(void) {
    g_t = 0.0f;
    UINav_Clear();
}

// A falling crystal: a bright shard on a fixed track, so the same six
// fall in the same places every time this plays.
static void DrawCrystal(int i, float phase, int screenWidth, int screenHeight, float scale) {
    static const float xs[6] = { 0.14f, 0.31f, 0.47f, 0.62f, 0.78f, 0.91f };
    static const float delays[6] = { 0.00f, 0.22f, 0.09f, 0.35f, 0.17f, 0.28f };

    float t = phase - delays[i];
    if (t <= 0.0f) return;
    if (t > 1.0f) t = 1.0f;

    float x = xs[i] * (float)screenWidth;
    float y = -60.0f * scale + t * t * ((float)screenHeight * 0.78f + 60.0f * scale);
    float len = 60.0f * scale;

    // The trail first, so the head sits on top of it.
    DrawLineEx((Vector2){ x, y - len }, (Vector2){ x, y }, 5.0f * scale,
               (Color){ 240, 128, 72, 140 });
    DrawCircleV((Vector2){ x, y }, 9.0f * scale, (Color){ 255, 210, 150, 255 });
    DrawCircleV((Vector2){ x, y }, 16.0f * scale, (Color){ 255, 140, 70, 90 });

    // Impact bloom once it has landed.
    if (t >= 1.0f) {
        DrawCircleGradient((int)x, (int)y, 130.0f * scale,
                           (Color){ 255, 160, 80, 120 }, (Color){ 255, 90, 40, 0 });
    }
}

bool UI_DrawSearing(int screenWidth, int screenHeight, float dt) {
    g_t += dt;
    float scale = UI_Scale(screenHeight);

    // Sky: from Ascalon's cold dusk to the colour everything after this
    // is remembered in.
    float sky = g_t / SEARING_SKY;
    if (sky > 1.0f) sky = 1.0f;
    Color base = { (unsigned char)(18 + 92 * sky),
                   (unsigned char)(18 + 22 * sky),
                   (unsigned char)(24 - 10 * sky), 255 };
    ClearBackground(base);

    // Ground haze, so the frame reads as a horizon rather than a wash.
    DrawRectangleGradientV(0, (int)(screenHeight * 0.62f), screenWidth,
                           (int)(screenHeight * 0.38f),
                           (Color){ 0, 0, 0, 0 }, (Color){ 30, 8, 6, 220 });

    if (g_t > SEARING_SKY) {
        float phase = (g_t - SEARING_SKY) / (SEARING_IMPACT - SEARING_SKY);
        for (int i = 0; i < 6; i++) DrawCrystal(i, phase, screenWidth, screenHeight, scale);
    }

    // A hard white flash on impact, decaying fast.
    if (g_t > SEARING_IMPACT && g_t < SEARING_IMPACT + 0.5f) {
        float a = 1.0f - (g_t - SEARING_IMPACT) / 0.5f;
        DrawRectangle(0, 0, screenWidth, screenHeight, Fade(RAYWHITE, a * 0.85f));
    }

    int titleFont = (int)(64 * scale);
    int bodyFont  = UI_FontSize(scale, UI_TEXT_MD);
    int smallFont = UI_FontSize(scale, UI_TEXT_SM);

    if (g_t > SEARING_TITLE) {
        float a = (g_t - SEARING_TITLE) / 0.8f;
        if (a > 1.0f) a = 1.0f;
        // A slow tremor under the title while the ground is still moving.
        float shake = (g_t < SEARING_TITLE + 1.6f)
                      ? sinf(g_t * 26.0f) * 3.0f * scale * (1.0f - (g_t - SEARING_TITLE) / 1.6f)
                      : 0.0f;
        UI_TextDisplayShadowCentered("THE SEARING", screenWidth / 2 + (int)shake,
                                     (int)(screenHeight * 0.30f), titleFont,
                                     Fade((Color){ 255, 214, 170, 255 }, a));
    }

    if (g_t > SEARING_TEXT) {
        float a = (g_t - SEARING_TEXT) / 1.0f;
        if (a > 1.0f) a = 1.0f;
        static const char *lines[] = {
            "The Charr crystals fell while you stood in the Academy.",
            "Ascalon is burning, and the county you walked is gone.",
        };
        int y = (int)(screenHeight * 0.46f);
        for (int i = 0; i < 2; i++) {
            UI_TextShadowCentered(lines[i], screenWidth / 2, y, bodyFont,
                                  Fade((Color){ 236, 214, 194, 255 }, a));
            y += bodyFont + UI_SP(scale, 4);
        }

        char who[96];
        Character_FormatTitle(&g_character, who, sizeof(who));
        char closing[160];
        snprintf(closing, sizeof(closing), "%s survived it. That is where this prototype ends.", who);
        UI_TextShadowCentered(closing, screenWidth / 2, y + UI_SP(scale, 4), bodyFont,
                              Fade(UI_GOLD, a));
    }

    bool leave = false;
    if (g_t > SEARING_PROMPT) {
        float pulse = 0.55f + 0.45f * sinf(g_t * 3.0f);
        UI_TextShadowCentered("Press Enter, or (A), to return to your characters",
                              screenWidth / 2, (int)(screenHeight * 0.76f), smallFont,
                              Fade((Color){ 224, 210, 186, 255 }, pulse));

        if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER) ||
            IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_SPACE) ||
            (IsGamepadAvailable(0) &&
             (IsGamepadButtonPressed(0, GAMEPAD_BUTTON_RIGHT_FACE_DOWN) ||
              IsGamepadButtonPressed(0, GAMEPAD_BUTTON_MIDDLE_RIGHT)))) {
            Audio_Play(SFX_UI_CONFIRM);
            leave = true;
        }
    }
    return leave;
}
