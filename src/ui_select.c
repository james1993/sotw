#include "ui_select.h"
#include "audio.h"
#include "character.h"
#include "raylib.h"
#include "save.h"
#include "ui_cursor.h"
#include "ui_font.h"
#include "ui_nav.h"
#include "ui_theme.h"
#include <stdio.h>
#include <string.h>

static SaveSlotInfo g_roster[SAVE_SLOT_COUNT];
// Which slot is waiting on "are you sure", -1 = none. Deleting a
// character is the one irreversible thing this screen does, so it never
// happens on a single click.
static int g_confirmDelete = -1;

static void RereadRoster(void) {
    for (int i = 0; i < SAVE_SLOT_COUNT; i++) {
        if (!Save_ReadSlotInfo(i, &g_roster[i])) {
            memset(&g_roster[i], 0, sizeof(g_roster[i]));
        }
    }
}

void UI_SelectReset(void) {
    RereadRoster();
    g_confirmDelete = -1;
    UINav_Clear();
}

// "Ascalonian (Mo/E)" - the same shorthand the nameplate uses, so a
// character reads the same here as in the world.
static void FormatSubtitle(const SaveSlotInfo *info, char *out, size_t outSize) {
    // PROF_NONE means "not earned yet", and the world also stores the
    // primary twice for a character without one - both have to read as
    // a single profession, not as "W/" with nothing after the slash.
    bool hasSecondary = (info->secondary != PROF_NONE) &&
                        (info->secondary != info->primary);
    if (hasSecondary) {
        snprintf(out, outSize, "%s/%s   Level %d",
                 Character_ProfessionAbbrev(info->primary),
                 Character_ProfessionAbbrev(info->secondary), info->level);
    } else {
        snprintf(out, outSize, "%s   Level %d",
                 Character_ProfessionAbbrev(info->primary), info->level);
    }
}

SelectAction UI_DrawSelectScreen(int screenWidth, int screenHeight, float dt) {
    (void)dt;
    float scale = UI_Scale(screenHeight);
    ClearBackground((Color){ 14, 14, 19, 255 });
    UINav_Begin();

    int titleFont = UI_FontSize(scale, UI_TEXT_XL);
    int nameFont  = UI_FontSize(scale, UI_TEXT_LG);
    int font      = UI_FontSize(scale, UI_TEXT_MD);
    int small     = UI_FontSize(scale, UI_TEXT_SM);
    int tiny      = UI_FontSize(scale, UI_TEXT_XS);

    UI_TextDisplayShadowCentered("CHOOSE YOUR CHARACTER", screenWidth / 2,
                                 (int)(screenHeight * 0.07f), titleFont, UI_GOLD);

    SelectAction action = SELECT_NONE;
    Vector2 mouse = UI_PointerPos();
    bool click = UI_PointerClicked();

    // Two rows of three. Six in a column would run off a 720p screen,
    // and six in a row would leave each card too narrow to name.
    int cols = 3, rows = 2;
    int cardW = (int)(300 * scale);
    int cardH = (int)(150 * scale);
    int gapX = UI_SP(scale, 5), gapY = UI_SP(scale, 5);
    int gridW = cols * cardW + (cols - 1) * gapX;
    int gridH = rows * cardH + (rows - 1) * gapY;
    int gx = (screenWidth - gridW) / 2;
    int gy = (int)(screenHeight * 0.20f);
    (void)gridH;

    for (int i = 0; i < SAVE_SLOT_COUNT; i++) {
        int col = i % cols, row = i / cols;
        Rectangle card = { (float)(gx + col * (cardW + gapX)),
                           (float)(gy + row * (cardH + gapY)),
                           (float)cardW, (float)cardH };
        const SaveSlotInfo *info = &g_roster[i];
        bool hovered = CheckCollisionPointRec(mouse, card);
        bool focused = UINav_Item(card);

        UI_ThemePanel(card, scale, 0);
        if (hovered || focused) {
            DrawRectangleRec((Rectangle){ card.x + 2, card.y + 2, card.width - 4, card.height - 4 },
                             (Color){ 44, 48, 64, 140 });
        }
        if (focused) {
            DrawRectangleLinesEx((Rectangle){ card.x - 3, card.y - 3,
                                              card.width + 6, card.height + 6 }, 2.0f, UI_GOLD);
        }

        int tx = (int)card.x + UI_SP(scale, 4);
        int ty = (int)card.y + UI_SP(scale, 4);

        if (!info->used) {
            UI_TextShadow("Empty", tx, ty, nameFont, UI_TEXT_MUTED);
            UI_TextShadow("Create a new character", tx, ty + nameFont + UI_SP(scale, 2),
                          small, UI_TEXT_SECOND);
            char slotLabel[24];
            snprintf(slotLabel, sizeof(slotLabel), "Slot %d", i + 1);
            UI_TextShadow(slotLabel, tx, (int)(card.y + card.height) - tiny - UI_SP(scale, 3),
                          tiny, UI_TEXT_MUTED);

            if ((hovered && click) || (focused && UINav_Confirm())) {
                Audio_Play(SFX_UI_CONFIRM);
                Save_SelectSlot(i);
                action = SELECT_CREATE;
            }
            continue;
        }

        UI_TextShadow(info->name, tx, ty, nameFont, UI_TEXT_PRIMARY);
        ty += nameFont + UI_SP(scale, 2);

        char sub[64];
        FormatSubtitle(info, sub, sizeof(sub));
        UI_TextShadow(sub, tx, ty, small, UI_TEXT_SECOND);
        ty += small + UI_SP(scale, 2);

        if (info->reforged) {
            UI_TextShadow("Reforged Mode", tx, ty, tiny, (Color){ 208, 168, 96, 255 });
            ty += tiny + UI_SP(scale, 1);
        }
        if (info->searingSurvived) {
            // A finished character. GW1 moves them on to post-Searing;
            // this prototype stops at the Searing, so the run is over
            // and the card says so rather than pretending otherwise.
            UI_TextShadow("Survived the Searing", tx, ty, tiny, (Color){ 224, 132, 96, 255 });
        }

        char slotLabel[24];
        snprintf(slotLabel, sizeof(slotLabel), "Slot %d", i + 1);
        UI_TextShadow(slotLabel, tx, (int)(card.y + card.height) - tiny - UI_SP(scale, 3),
                      tiny, UI_TEXT_MUTED);

        // Delete, tucked bottom-right of the card and confirmed in place.
        int delW = (int)(96 * scale), delH = (int)(22 * scale);
        Rectangle del = { card.x + card.width - delW - UI_SP(scale, 4),
                          card.y + card.height - delH - UI_SP(scale, 4),
                          (float)delW, (float)delH };
        bool delHover = CheckCollisionPointRec(mouse, del);
        bool confirming = (g_confirmDelete == i);
        DrawRectangleRec(del, confirming ? (Color){ 122, 52, 48, 255 }
                            : delHover ? (Color){ 82, 46, 44, 255 } : (Color){ 40, 34, 34, 255 });
        DrawRectangleLinesEx(del, 1.0f, confirming ? (Color){ 224, 132, 112, 255 }
                                                   : (Color){ 82, 62, 58, 255 });
        const char *delLabel = confirming ? "Really?" : "Delete";
        int dw = UITextWidth(delLabel, tiny);
        UI_TextShadow(delLabel, (int)(del.x + (del.width - dw) / 2),
                      (int)(del.y + (del.height - tiny) / 2), tiny,
                      confirming ? (Color){ 255, 226, 214, 255 } : UI_TEXT_SECOND);

        if (delHover && click) {
            if (confirming) {
                Save_DeleteSlot(i);
                RereadRoster();
                g_confirmDelete = -1;
                Audio_Play(SFX_UI_CLOSE);
            } else {
                g_confirmDelete = i;
                Audio_Play(SFX_UI_CLICK);
            }
            continue; // this click was the delete button's, not the card's
        }

        if ((hovered && click && !delHover) || (focused && UINav_Confirm())) {
            Audio_Play(SFX_UI_CONFIRM);
            Save_SelectSlot(i);
            action = SELECT_PLAY;
        }
    }

    // --- Back ---
    {
        int btnW = (int)(200 * scale), btnH = (int)(44 * scale);
        Rectangle back = { (float)((screenWidth - btnW) / 2),
                           (float)(gy + rows * (cardH + gapY) + UI_SP(scale, 6)),
                           (float)btnW, (float)btnH };
        bool focused = UINav_Item(back);
        if (UI_Button(back, "Back", font, true, focused) ||
            (focused && UINav_Confirm())) {
            action = SELECT_BACK;
        }
        if (focused) {
            DrawRectangleLinesEx((Rectangle){ back.x - 3, back.y - 3,
                                              back.width + 6, back.height + 6 }, 2.0f, UI_GOLD);
        }
    }

    UI_TextShadowCentered("D-pad or arrows to choose - Enter or (A) confirms",
                          screenWidth / 2, screenHeight - (int)(40 * scale), tiny,
                          UI_TEXT_MUTED);

    UINav_End();

    if (IsKeyPressed(KEY_ESCAPE) ||
        (IsGamepadAvailable(0) && IsGamepadButtonPressed(0, GAMEPAD_BUTTON_RIGHT_FACE_RIGHT))) {
        action = SELECT_BACK;
    }
    return action;
}
