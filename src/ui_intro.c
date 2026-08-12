#include "ui_intro.h"
#include "audio.h"
#include "raylib.h"
#include "ui_font.h"
#include "ui_nav.h"
#include "ui_theme.h"
#include <math.h>
#include <stddef.h>

// "The Last Day Dawns", verbatim from GW1 Prophecies. who == NULL is the
// unnamed narrator's voiceover; a name is spoken dialogue. The exchange
// shifts from the dawn outside to King Adelbern's hall, which the
// background follows.
typedef struct { const char *who; const char *text; float dur; } IntroLine;

static const IntroLine g_lines[] = {
    { NULL, "The last day dawns on the Kingdom of Ascalon.", 4.0f },
    { NULL, "It arrives with no fanfare, no tolling of alarms.", 4.0f },
    { NULL, "Those who will remember will speak fondly of the warm morning breeze.", 4.6f },
    { NULL, "People carry on with their daily lives, unaware that in a short while...", 4.6f },
    { NULL, "Everything they have ever known will come to an end.", 4.2f },
    { "King Adelbern",     "Scribe!", 2.0f },
    { "Ascalonian Scribe", "Yes, my lord.", 1.9f },
    { "King Adelbern",     "These Charr are relentless, but we shall hold the Wall at all costs.", 4.2f },
    { "King Adelbern",     "Take this message to Sir Tydus.", 2.8f },
    { "King Adelbern",     "Go forth and recruit the strongest, the smartest.", 3.4f },
    { "King Adelbern",     "Bring to me the bravest in all Ascalon.", 3.2f },
    { "King Adelbern",     "Find me the heroes who will lead our kingdom to glory.", 3.8f },
    { "Ascalonian Scribe", "As you command, my king.", 2.8f },
};
#define INTRO_LINE_COUNT (int)(sizeof(g_lines) / sizeof(g_lines[0]))

#define LINE_FADE 0.7f   // in and out per line
#define TITLE_HOLD 3.0f  // the card title before the first line

static float g_t = 0.0f;

void UI_IntroReset(void) {
    g_t = 0.0f;
    UINav_Clear();
}

// The Great Northern Wall as a crenellated black silhouette along the
// horizon - the one landmark this whole story turns on.
static void DrawWall(int screenWidth, int screenHeight, float scale) {
    float top = screenHeight * 0.68f;
    Color stone = { 14, 12, 16, 255 };
    DrawRectangle(0, (int)top, screenWidth, screenHeight - (int)top, stone);
    // Merlons along the top edge.
    float mw = 34.0f * scale, gap = 20.0f * scale, mh = 16.0f * scale;
    for (float x = -gap; x < screenWidth; x += mw + gap) {
        DrawRectangle((int)x, (int)(top - mh), (int)mw, (int)mh, stone);
    }
    // A couple of tower blocks to break the line.
    DrawRectangle((int)(screenWidth * 0.22f), (int)(top - 46 * scale), (int)(40 * scale),
                  (int)(46 * scale), stone);
    DrawRectangle((int)(screenWidth * 0.71f), (int)(top - 54 * scale), (int)(46 * scale),
                  (int)(54 * scale), stone);
}

bool UI_DrawIntro(int screenWidth, int screenHeight, float dt) {
    g_t += dt;
    float scale = UI_Scale(screenHeight);

    // Which line is showing, and how far into it. The title card holds
    // first, then each line runs for its own duration back to back.
    int idx = -1;              // -1 = title card
    float local = 0.0f, dur = TITLE_HOLD;
    float acc = TITLE_HOLD;
    if (g_t >= TITLE_HOLD) {
        float tt = g_t - TITLE_HOLD;
        idx = 0;
        while (idx < INTRO_LINE_COUNT && tt >= g_lines[idx].dur) {
            tt -= g_lines[idx].dur;
            acc += g_lines[idx].dur;
            idx++;
        }
        if (idx >= INTRO_LINE_COUNT) return true; // played out -> gameplay
        local = tt;
        dur = g_lines[idx].dur;
    }
    (void)acc;

    bool inHall = (idx >= 0 && g_lines[idx].who != NULL);

    // Sky: a dawn wash outside, deepening to torchlit gloom once we're in
    // the king's hall. The transition rides `hall` so it eases, not cuts.
    static float hallBlend = 0.0f;
    float target = inHall ? 1.0f : 0.0f;
    hallBlend += (target - hallBlend) * (dt * 3.0f);
    Color skyTop = { (unsigned char)(30 + 6 * (1 - hallBlend)),
                     (unsigned char)(24 + 10 * (1 - hallBlend)),
                     (unsigned char)(40 + 20 * (1 - hallBlend)), 255 };
    Color skyLow = { (unsigned char)(120 + 40 * (1 - hallBlend) - 40 * hallBlend),
                     (unsigned char)(70 + 20 * (1 - hallBlend) - 40 * hallBlend),
                     (unsigned char)(60 - 30 * hallBlend), 255 };
    DrawRectangleGradientV(0, 0, screenWidth, screenHeight, skyTop, skyLow);

    // A rising sun, low and warm, climbing slowly across the whole piece.
    float sunT = g_t / (TITLE_HOLD + 30.0f);
    if (sunT > 1.0f) sunT = 1.0f;
    float sunX = screenWidth * 0.5f;
    float sunY = screenHeight * (0.70f - 0.16f * sunT);
    Color sunGlow = Fade((Color){ 255, 180, 110, 255 }, 0.5f * (1.0f - hallBlend));
    DrawCircleGradient((int)sunX, (int)sunY, 220.0f * scale, sunGlow,
                       Fade(sunGlow, 0.0f));
    DrawCircleV((Vector2){ sunX, sunY }, 46.0f * scale,
                Fade((Color){ 255, 214, 170, 255 }, 0.9f * (1.0f - hallBlend)));

    DrawWall(screenWidth, screenHeight, scale);

    // Indoors, drop a warm vignette so the hall reads as enclosed.
    if (hallBlend > 0.02f) {
        DrawRectangle(0, 0, screenWidth, screenHeight,
                      Fade((Color){ 20, 10, 8, 255 }, 0.55f * hallBlend));
    }

    int titleFont = (int)(46 * scale);
    int nameFont  = UI_FontSize(scale, UI_TEXT_MD);
    int bodyFont  = UI_FontSize(scale, UI_TEXT_LG);
    int smallFont = UI_FontSize(scale, UI_TEXT_SM);

    if (idx < 0) {
        // Title card, fading in then holding.
        float a = g_t / 0.9f; if (a > 1.0f) a = 1.0f;
        UI_TextDisplayShadowCentered("THE LAST DAY DAWNS", screenWidth / 2,
                                     (int)(screenHeight * 0.40f), titleFont,
                                     Fade((Color){ 245, 224, 190, 255 }, a));
    } else {
        // Per-line fade in / hold / fade out.
        float a = 1.0f;
        if (local < LINE_FADE) a = local / LINE_FADE;
        else if (local > dur - LINE_FADE) a = (dur - local) / LINE_FADE;
        if (a < 0.0f) a = 0.0f;

        const IntroLine *ln = &g_lines[idx];
        int cy = (int)(screenHeight * 0.44f);
        if (ln->who) {
            UI_TextShadowCentered(ln->who, screenWidth / 2, cy, nameFont,
                                  Fade(UI_GOLD, a));
            cy += nameFont + UI_SP(scale, 6);
        }
        Color textCol = ln->who ? (Color){ 236, 224, 206, 255 }
                                 : (Color){ 214, 216, 232, 255 }; // narrator cooler
        UI_TextShadowCentered(ln->text, screenWidth / 2, cy, bodyFont, Fade(textCol, a));
    }

    // A steady skip prompt.
    float pulse = 0.5f + 0.4f * sinf(g_t * 3.0f);
    UI_TextShadowCentered("Press Enter, Space, or (A) to continue",
                          screenWidth / 2, (int)(screenHeight * 0.88f), smallFont,
                          Fade((Color){ 214, 202, 182, 255 }, pulse));

    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER) ||
        IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ESCAPE) ||
        (IsGamepadAvailable(0) &&
         (IsGamepadButtonPressed(0, GAMEPAD_BUTTON_RIGHT_FACE_DOWN) ||
          IsGamepadButtonPressed(0, GAMEPAD_BUTTON_MIDDLE_RIGHT)))) {
        Audio_Play(SFX_UI_CONFIRM);
        return true;
    }
    return false;
}
