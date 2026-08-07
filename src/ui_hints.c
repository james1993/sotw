#include "ui_hints.h"
#include "world.h"
#include "ui_font.h"
#include "ui_theme.h"
#include "ui_hit.h"
#include "raylib.h"
#include <stddef.h>
#include <stdio.h>
#include <string.h>

// One row of the legend. Keyboard and pad labels are separate strings
// because they aren't the same shape - "I" vs "Y", "M" vs "Select".
typedef struct {
    const char *key;
    const char *pad;
    const char *what;
} HintEntry;

// A NULL pad label means "no controller binding" - the entry is simply
// dropped from the pad legend. On a controller every screen is reached
// through Start, so the panel hotkeys only appear for the keyboard.
static const HintEntry g_hints[] = {
    { "Space", "A",      "Attack" },
    { "Tab",   "R1",     "Next foe" },
    { "F",     "X",      "Talk" },
    { "L",     NULL,     "Skills" },
    { "I",     NULL,     "Bags" },
    { "E",     NULL,     "Gear" },
    { "K",     NULL,     "Attributes" },
    { "M",     "Select", "Map" },
    { "P",     "Start",  "Menu" },
};
#define HINT_COUNT (int)(sizeof(g_hints) / sizeof(g_hints[0]))

void UI_DrawControlHints(int screenWidth, int screenHeight) {
    (void)screenWidth;
    float scale = UI_Scale(screenHeight);
    bool pad = IsGamepadAvailable(0);

    int font = (int)(11 * scale);
    int gapAfterBadge = (int)(5 * scale);
    int gapBetween = (int)(14 * scale);
    int rowH = (int)(font * 1.5f) + (int)(6 * scale);

    // Measure first so the backing panel hugs the row exactly.
    int totalW = 0;
    for (int i = 0; i < HINT_COUNT; i++) {
        const char *label = pad ? g_hints[i].pad : g_hints[i].key;
        if (!label) continue; // no pad equivalent - skip rather than lie
        totalW += UI_KeyBadge(label, 0, 0, font, false);
        totalW += gapAfterBadge + UITextWidth(g_hints[i].what, font) + gapBetween;
    }
    if (totalW <= 0) return;
    totalW -= gapBetween;

    int padX = (int)(12 * scale);
    int x = (int)(14 * scale);
    int y = screenHeight - rowH - (int)(10 * scale);

    Rectangle panel = { (float)(x - padX), (float)(y - (int)(5 * scale)),
                        (float)(totalW + padX * 2), (float)(rowH + (int)(10 * scale)) };
    UIHit_Claim(panel);
    DrawRectangleRounded(panel, 0.35f, 8, (Color){ 14, 15, 20, 190 });
    DrawRectangleRoundedLines(panel, 0.35f, 8, (Color){ 90, 80, 55, 200 });

    int cursor = x;
    for (int i = 0; i < HINT_COUNT; i++) {
        const char *label = pad ? g_hints[i].pad : g_hints[i].key;
        if (!label) continue;
        int badgeW = UI_KeyBadge(label, cursor, y, font, true);
        cursor += badgeW + gapAfterBadge;
        UI_TextShadow(g_hints[i].what, cursor, y + (rowH - font) / 2 - (int)(2 * scale),
                      font, (Color){ 186, 180, 164, 255 });
        cursor += UITextWidth(g_hints[i].what, font) + gapBetween;
    }
}

// ---------------------------------------------------------------------
// Zone title card

static const char *g_shownZone = NULL;
static float g_titleTimer = 0.0f;

#define TITLE_HOLD 2.4f
#define TITLE_FADE 1.0f

void UI_ResetZoneTitle(void) {
    g_shownZone = NULL;
    g_titleTimer = 0.0f;
}

void UI_DrawZoneTitle(int screenWidth, int screenHeight, float dt) {
    const char *zone = World_GetZoneName();

    // Zone names are string literals from the zone table, but compare by
    // content rather than pointer so a rebuilt table can't skip a card.
    if (!g_shownZone || strcmp(zone, g_shownZone) != 0) {
        g_shownZone = zone;
        g_titleTimer = TITLE_HOLD + TITLE_FADE;
    }

    float scale = UI_Scale(screenHeight);

    // The small persistent zone label lives top-center always; the big
    // card only rides on top of it for a few seconds after arriving.
    int smallFont = (int)(16 * scale);
    UI_TextDisplayShadowCentered(zone, screenWidth / 2, (int)(14 * scale), smallFont,
                                 (Color){ 222, 210, 178, 235 });

    if (g_titleTimer <= 0.0f) return;
    g_titleTimer -= dt;

    float alpha = 1.0f;
    if (g_titleTimer < TITLE_FADE) alpha = g_titleTimer / TITLE_FADE;
    if (alpha < 0.0f) alpha = 0.0f;

    int bigFont = (int)(38 * scale);
    int tw = UITextDisplayWidth(zone, bigFont);
    int cx = screenWidth / 2;
    int y = (int)(screenHeight * 0.17f);

    // Gold rules either side of the name, GW1's area-announcement look.
    int ruleW = (int)(90 * scale);
    int ruleGap = (int)(22 * scale);
    int ruleY = y + bigFont / 2;
    Color rule = Fade(UI_GOLD, alpha * 0.8f);
    DrawRectangle(cx - tw / 2 - ruleGap - ruleW, ruleY, ruleW, (int)(2 * scale), rule);
    DrawRectangle(cx + tw / 2 + ruleGap, ruleY, ruleW, (int)(2 * scale), rule);

    UI_TextDisplayShadowCentered(zone, cx, y, bigFont, Fade((Color){ 240, 226, 186, 255 }, alpha));

    const char *sub = (World_GetMode() == MODE_OUTPOST) ? "Outpost" : "Explorable";
    int subFont = (int)(13 * scale);
    UI_TextShadowCentered(sub, cx, y + bigFont + (int)(6 * scale), subFont,
                          Fade((Color){ 186, 176, 150, 255 }, alpha * 0.9f));
}

// ---------------------------------------------------------------------
// Notification banners

#define MAX_NOTIFICATIONS 4
#define NOTIFY_LIFE 4.5f
#define NOTIFY_FADE 1.2f

typedef struct {
    char text[96];
    float life;
} Notification;

static Notification g_notes[MAX_NOTIFICATIONS];

void UI_ClearNotifications(void) {
    for (int i = 0; i < MAX_NOTIFICATIONS; i++) g_notes[i].life = 0.0f;
}

void UI_Notify(const char *message) {
    if (!message) return;
    // Newest at the top of the stack; the oldest falls off when full.
    for (int i = MAX_NOTIFICATIONS - 1; i > 0; i--) g_notes[i] = g_notes[i - 1];
    snprintf(g_notes[0].text, sizeof(g_notes[0].text), "%s", message);
    g_notes[0].life = NOTIFY_LIFE;
}

void UI_DrawNotifications(int screenWidth, int screenHeight, float dt) {
    float scale = UI_Scale(screenHeight);
    int font = (int)(17 * scale);
    int padX = (int)(16 * scale);
    int rowH = (int)(font * 1.5f) + (int)(12 * scale);
    int y = (int)(screenHeight * 0.30f);

    for (int i = 0; i < MAX_NOTIFICATIONS; i++) {
        if (g_notes[i].life <= 0.0f) continue;
        g_notes[i].life -= dt;

        float alpha = 1.0f;
        if (g_notes[i].life < NOTIFY_FADE) alpha = g_notes[i].life / NOTIFY_FADE;
        if (alpha < 0.0f) alpha = 0.0f;

        int tw = UITextWidth(g_notes[i].text, font);
        int w = tw + padX * 2;
        Rectangle box = { (screenWidth - w) / 2.0f, (float)y, (float)w, (float)rowH };
        DrawRectangleRounded(box, 0.3f, 8, Fade((Color){ 18, 19, 26, 240 }, alpha));
        DrawRectangleRoundedLines(box, 0.3f, 8, Fade(UI_GOLD, alpha));
        UI_TextShadowCentered(g_notes[i].text, screenWidth / 2, y + (rowH - font) / 2, font,
                              Fade((Color){ 244, 232, 198, 255 }, alpha));
        y += rowH + (int)(6 * scale);
    }
}
