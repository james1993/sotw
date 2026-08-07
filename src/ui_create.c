#include "ui_create.h"
#include "entity.h"
#include "sprite.h"
#include "skill.h"
#include "attributes.h"
#include "gwmath.h"
#include "ui_font.h"
#include "ui_theme.h"
#include "ui_nav.h"
#include "ui_cursor.h"
#include "audio.h"
#include "raylib.h"
#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

// The character being built. Committed into g_character on confirm, so
// backing out of the screen can't leave a half-made character behind.
static CharacterDef g_draft;
static bool g_nameFocused = false;
static float g_previewTime = 0.0f;

// A standalone entity used only to render the preview. Building a real
// Entity rather than a bespoke "draw a person" routine means the
// preview is literally the sprite the world will draw - it cannot drift
// out of sync with the game the way a mock-up would.
static Entity g_previewEntity;

void UI_CreateReset(void) {
    UINav_Clear(); // nothing highlighted until the player reaches for the D-pad
    g_draft = Character_Default();
    g_nameFocused = false;
    g_previewTime = 0.0f;
}

// All six Prophecies professions, in GW1's own listing order.
static const Profession g_professions[] = {
    PROF_WARRIOR, PROF_RANGER, PROF_MONK,
    PROF_NECROMANCER, PROF_MESMER, PROF_ELEMENTALIST
};
#define PROFESSION_COUNT (int)(sizeof(g_professions) / sizeof(g_professions[0]))

// What each profession starts with, mirroring World_Init's kit. Shown
// on the screen so the choice is informed rather than a guess at what
// the words mean.
//
// Energy is 20 across the board and that is not an oversight - it's
// GW1's rule. The Elementalist's deep pool comes from ranks in Energy
// Storage, so the starting kit's 3 ranks are what put them at 29.
static void ProfessionStats(Profession p, int *energy, int *armor, const char **weapon,
                            const char **skill) {
    *energy = GW_MaxEnergy(0);
    switch (p) {
        case PROF_WARRIOR:
            *armor = 40; *weapon = "Ascalon Sword"; *skill = "Gash"; break;
        case PROF_RANGER:
            *armor = 35; *weapon = "Ascalon Longbow"; *skill = "Power Shot"; break;
        case PROF_MONK:
            *armor = 30; *weapon = "Smiting Rod"; *skill = "Orison of Healing"; break;
        case PROF_NECROMANCER:
            *armor = 30; *weapon = "Bone Idol"; *skill = "Vampiric Gaze"; break;
        case PROF_MESMER:
            *armor = 30; *weapon = "Jeweled Wand"; *skill = "Ether Feast"; break;
        case PROF_ELEMENTALIST:
        default:
            *energy = GW_MaxEnergy(3); // 3 starting ranks of Energy Storage
            *armor = 30; *weapon = "Kindling Staff"; *skill = "Fire Bolt"; break;
    }
}

// One line on what this profession's PRIMARY attribute actually does.
// The primary is the permanent half of the choice - a secondary can be
// changed later, this can't - so it gets stated outright.
static const char *PrimaryEffect(Profession p) {
    switch (p) {
        case PROF_WARRIOR:      return "Ignores 1% of a foe's armour per rank.";
        case PROF_RANGER:       return "Attack skills cost 4% less energy per rank.";
        case PROF_MONK:         return "Your healing spells give +3.2 health per rank.";
        case PROF_NECROMANCER:  return "Regain energy when a creature dies nearby.";
        case PROF_MESMER:       return "Spells cast faster - rank 15 halves them.";
        case PROF_ELEMENTALIST: return "+3 maximum energy per rank.";
        default:                return "";
    }
}

// Rebuilds the preview entity from the draft. Cheap enough to redo
// every frame, and that keeps it honest.
static void SyncPreview(void) {
    memset(&g_previewEntity, 0, sizeof(g_previewEntity));
    g_previewEntity.alive = true;
    g_previewEntity.kind = ENT_HERO; // not the player slot: keeps sprite.c's
                                     // "is this g_entities[0]" checks false
    g_previewEntity.species = SPECIES_HUMAN;
    g_previewEntity.primaryProfession = g_draft.primary;
    g_previewEntity.secondaryProfession = g_draft.primary;
    g_previewEntity.sex = g_draft.sex;
    g_previewEntity.skinTone = g_draft.skinTone;
    g_previewEntity.hairColor = g_draft.hairColor;
    g_previewEntity.hairStyle = g_draft.hairStyle;
    g_previewEntity.facing = (Vector2){ 0.0f, 1.0f };
    g_previewEntity.castingSlot = -1;
    for (int i = 0; i < SKILL_BAR_SIZE; i++) g_previewEntity.skillBar[i] = -1;

    // Robe colour by profession, matching the starting armour each one
    // is issued (character.c owns the palette, so the preview and the
    // world can't disagree about what a Necromancer looks like).
    g_previewEntity.color = Character_ProfessionColor(g_draft.primary);
}

// Every interactive control on this screen goes through one of these
// three wrappers, so each one is registered with the spatial nav ring in
// draw order and draws its own focus ring. Registering unconditionally
// matters: nav identity is the registration index, so a control that
// skipped a frame would renumber every control after it.
static void NavRing(Rectangle r) {
    DrawRectangleLinesEx((Rectangle){ r.x - 3, r.y - 3, r.width + 6, r.height + 6 },
                         2.0f, UI_GOLD);
}

static bool NavButton(Rectangle r, const char *label, int font, bool enabled, bool selected) {
    bool focused = UINav_Item(r);
    bool fired = UI_Button(r, label, font, enabled, selected || focused);
    if (focused) NavRing(r);
    if (focused && enabled && UINav_Confirm()) {
        Audio_Play(SFX_UI_CLICK);
        return true;
    }
    return fired;
}

// A row of small swatches; returns the index clicked, or -1.
static int SwatchRow(int x, int y, int size, int gap, const Color *colors, int count,
                     int selected) {
    int clicked = -1;
    Vector2 mouse = UI_PointerPos();

    // The row is ONE nav stop, not one per swatch: left/right walks the
    // colours, up/down leaves the row. Six separate stops would mean
    // pressing Down five times to get past a palette.
    Rectangle rowRect = { (float)x, (float)y, (float)(count * size + (count - 1) * gap),
                          (float)size };
    if (UINav_Item(rowRect)) {
        NavRing(rowRect);
        int step = UINav_ClaimHorizontal();
        if (step != 0) clicked = (selected + step + count) % count;
    }

    for (int i = 0; i < count; i++) {
        Rectangle r = { (float)(x + i * (size + gap)), (float)y, (float)size, (float)size };
        DrawRectangleRec(r, colors[i]);
        bool hovered = CheckCollisionPointRec(mouse, r);
        DrawRectangleLinesEx(r, (i == selected) ? 3.0f : 1.0f,
                             (i == selected) ? UI_GOLD
                             : hovered ? (Color){ 220, 214, 196, 255 } : (Color){ 20, 20, 26, 255 });
        if (hovered && UI_PointerClicked()) {
            clicked = i;
            Audio_Play(SFX_UI_CLICK);
        }
    }
    return clicked;
}

// A labelled left/right stepper for options that aren't colours.
static int Stepper(int x, int y, int w, int h, const char *label, const char *value,
                   int font, int count, int current) {
    UI_TextShadow(label, x, y + (h - font) / 2, font, UI_TEXT_SECOND);
    int btn = h;
    Rectangle left = { (float)(x + w - btn * 2 - 90), (float)y, (float)btn, (float)h };
    Rectangle right = { (float)(x + w - btn), (float)y, (float)btn, (float)h };
    int result = current;

    // One stop for the whole stepper - left/right IS the control, so
    // focusing the two arrows separately would be busywork.
    Rectangle rowRect = { (float)x, (float)y, (float)w, (float)h };
    if (UINav_Item(rowRect)) {
        NavRing(rowRect);
        int step = UINav_ClaimHorizontal();
        if (step != 0) result = (current + step + count) % count;
    }

    if (UI_Button(left, "<", font, true, false)) result = (current - 1 + count) % count;
    if (UI_Button(right, ">", font, true, false)) result = (current + 1) % count;
    int vw = UITextWidth(value, font);
    UI_TextShadow(value, (int)(left.x + btn + (90 - vw) / 2), y + (h - font) / 2, font,
                  UI_TEXT_PRIMARY);
    return result;
}

CreateAction UI_DrawCreateScreen(int screenWidth, int screenHeight, float dt) {
    float scale = UI_Scale(screenHeight);
    g_previewTime += dt;
    SyncPreview();
    UINav_Begin();

    ClearBackground((Color){ 14, 14, 19, 255 });

    int titleFont = UI_FontSize(scale, UI_TEXT_XL);
    int headFont = UI_FontSize(scale, UI_TEXT_LG);
    int font = UI_FontSize(scale, UI_TEXT_MD);
    int small = UI_FontSize(scale, UI_TEXT_SM);
    int tiny = UI_FontSize(scale, UI_TEXT_XS);

    UI_TextDisplayShadowCentered("CREATE YOUR CHARACTER", screenWidth / 2,
                          (int)(screenHeight * 0.05f), titleFont, UI_GOLD);

    // Three columns: choose (left), see (middle), adjust (right). The
    // preview sits in the centre because it's the thing every other
    // control is talking about.
    int colW = (int)(screenWidth * 0.27f);
    int colY = (int)(screenHeight * 0.14f);
    int colH = (int)(screenHeight * 0.62f);
    int margin = (int)(screenWidth * 0.04f);

    Rectangle leftCol = { (float)margin, (float)colY, (float)colW, (float)colH };
    Rectangle rightCol = { (float)(screenWidth - margin - colW), (float)colY,
                           (float)colW, (float)colH };
    UI_ThemePanel(leftCol, scale, 0);
    UI_ThemePanel(rightCol, scale, 0);

    // ---------------- Left: profession ----------------
    {
        int x = (int)leftCol.x + UI_SP(scale, 4);
        int y = (int)leftCol.y + UI_SP(scale, 4);
        int innerW = colW - UI_SP(scale, 8);
        UI_TextDisplayShadow("Profession", x, y, headFont, UI_GOLD);
        y += headFont + UI_SP(scale, 3);

        // Six buttons in two columns. A single column of six would push
        // the stats below the fold at 720p, and the whole point of this
        // panel is that the numbers sit next to the choice.
        int btnH = (int)(32 * scale);
        int colGap = UI_SP(scale, 2);
        int btnW = (innerW - colGap) / 2;
        for (int i = 0; i < PROFESSION_COUNT; i++) {
            Profession p = g_professions[i];
            Rectangle r = { (float)(x + (i % 2) * (btnW + colGap)),
                            (float)(y + (i / 2) * (btnH + UI_SP(scale, 2))),
                            (float)btnW, (float)btnH };
            if (NavButton(r, Character_ProfessionName(p), small, true, g_draft.primary == p)) {
                g_draft.primary = p;
            }
        }
        y += ((PROFESSION_COUNT + 1) / 2) * (btnH + UI_SP(scale, 2)) + UI_SP(scale, 2);

        // The pitch, then the concrete numbers. A player who doesn't
        // know GW1 needs both: the sentence to understand the fantasy,
        // the stats to understand the trade.
        UI_TextShadow(Character_ProfessionBlurb(g_draft.primary), x, y, tiny, UI_TEXT_SECOND);
        y += tiny * 2 + UI_SP(scale, 3);

        int energy, armor;
        const char *weapon, *skill;
        ProfessionStats(g_draft.primary, &energy, &armor, &weapon, &skill);

        char line[96];
        snprintf(line, sizeof(line), "Energy    %d", energy);
        UI_TextShadow(line, x, y, small, UI_TEXT_PRIMARY); y += small + UI_SP(scale, 2);
        snprintf(line, sizeof(line), "Armour    AL %d", armor);
        UI_TextShadow(line, x, y, small, UI_TEXT_PRIMARY); y += small + UI_SP(scale, 2);
        snprintf(line, sizeof(line), "Weapon    %s", weapon);
        UI_TextShadow(line, x, y, small, UI_TEXT_PRIMARY); y += small + UI_SP(scale, 2);
        snprintf(line, sizeof(line), "Skill     %s", skill);
        UI_TextShadow(line, x, y, small, UI_TEXT_LINK); y += small + UI_SP(scale, 3);

        UI_TextShadow("Attributes", x, y, small, UI_GOLD_DIM);
        y += small + UI_SP(scale, 1);
        for (int a = 0; a < ATTR_COUNT; a++) {
            if (!Attribute_Accessible((AttributeKind)a, g_draft.primary, g_draft.primary)) continue;
            snprintf(line, sizeof(line), "%s%s", g_attributeNames[a],
                     g_attributeIsPrimary[a] ? "   (primary)" : "");
            UI_TextShadow(line, x + UI_SP(scale, 2), y, tiny,
                          g_attributeIsPrimary[a] ? UI_GOLD : UI_TEXT_SECOND);
            y += tiny + UI_SP(scale, 1);
        }

        // What the primary actually does. This is the line that makes
        // the choice permanent, so it goes last and it goes in gold.
        y += UI_SP(scale, 1);
        UI_TextShadow(PrimaryEffect(g_draft.primary), x + UI_SP(scale, 2), y, tiny, UI_GOLD_DIM);
    }

    // ---------------- Middle: live preview ----------------
    {
        float cx = screenWidth / 2.0f;
        float cy = colY + colH * 0.52f;

        // A plinth so the figure isn't floating in a void.
        DrawEllipse((int)cx, (int)(cy + 62 * scale), 74 * scale, 20 * scale,
                    (Color){ 22, 23, 30, 255 });
        DrawEllipse((int)cx, (int)(cy + 60 * scale), 70 * scale, 18 * scale,
                    (Color){ 34, 36, 46, 255 });

        // Draw the real sprite, scaled up. radius drives every
        // proportion in sprite.c, so this is a straight zoom of what the
        // world renders.
        g_previewEntity.radius = 42.0f * scale;
        g_previewEntity.pos = (Vector2){ cx, cy };
        // A slow idle sway, so the preview reads as a character rather
        // than a paper doll.
        g_previewEntity.animTime = g_previewTime * 1.2f;
        g_previewEntity.moveBlend = 0.18f;
        Sprite_DrawEntity(&g_previewEntity, g_previewTime);

        char title[64];
        Character_FormatTitle(&g_draft, title, sizeof(title));
        UI_TextShadowCentered(title, (int)cx, (int)(cy + 92 * scale), headFont,
                              UI_TEXT_PRIMARY);
        UI_TextShadowCentered("Your second profession is earned in Ascalon.",
                              (int)cx, (int)(cy + 92 * scale) + headFont + UI_SP(scale, 2),
                              tiny, UI_TEXT_MUTED);
    }

    // ---------------- Right: appearance ----------------
    {
        int x = (int)rightCol.x + UI_SP(scale, 4);
        int y = (int)rightCol.y + UI_SP(scale, 4);
        int innerW = colW - UI_SP(scale, 8);
        UI_TextDisplayShadow("Appearance", x, y, headFont, UI_GOLD);
        y += headFont + UI_SP(scale, 4);

        int rowH = (int)(30 * scale);
        static const char *sexNames[2] = { "Female", "Male" };
        g_draft.sex = Stepper(x, y, innerW, rowH, "Sex", sexNames[g_draft.sex ? 1 : 0],
                              font, 2, g_draft.sex);
        y += rowH + UI_SP(scale, 4);

        char buf[32];
        snprintf(buf, sizeof(buf), "Style %d", g_draft.hairStyle + 1);
        g_draft.hairStyle = Stepper(x, y, innerW, rowH, "Hair", buf, font,
                                    HAIR_STYLE_COUNT, g_draft.hairStyle);
        y += rowH + UI_SP(scale, 5);

        int sw = (int)(28 * scale);
        int gap = UI_SP(scale, 2);
        UI_TextShadow("Skin", x, y, small, UI_TEXT_SECOND);
        y += small + UI_SP(scale, 2);
        int picked = SwatchRow(x, y, sw, gap, g_skinTones, SKIN_TONE_COUNT, g_draft.skinTone);
        if (picked >= 0) g_draft.skinTone = picked;
        y += sw + UI_SP(scale, 5);

        UI_TextShadow("Hair colour", x, y, small, UI_TEXT_SECOND);
        y += small + UI_SP(scale, 2);
        picked = SwatchRow(x, y, sw, gap, g_hairColors, HAIR_COLOR_COUNT, g_draft.hairColor);
        if (picked >= 0) g_draft.hairColor = picked;
        y += sw + UI_SP(scale, 6);

        // Reforged Mode. Taken here and never again - that's how the
        // real thing works, and it's why it belongs on the creation
        // screen next to the other permanent decisions rather than in a
        // settings menu you could revisit.
        DrawRectangle(x, y, innerW, 1, UI_GOLD_DIM);
        y += UI_SP(scale, 3);
        UI_TextShadow("Reforged Mode", x, y, small, UI_GOLD);
        y += small + UI_SP(scale, 2);
        Rectangle toggle = { (float)x, (float)y, (float)innerW, (float)(30 * scale) };
        if (NavButton(toggle, g_draft.reforged ? "Reforged:  ON" : "Reforged:  off",
                      small, true, g_draft.reforged)) {
            g_draft.reforged = !g_draft.reforged;
        }
        y += (int)(30 * scale) + UI_SP(scale, 2);
        UI_TextShadow(g_draft.reforged
                          ? "Piken Square opens. More Charr in the north."
                          : "The county as it was. No Piken Square.",
                      x, y, tiny, UI_TEXT_SECOND);
        y += tiny + UI_SP(scale, 1);
        UI_TextShadow(g_draft.reforged
                          ? "Foes hit softer; +5% XP and gold."
                          : "",
                      x, y, tiny, UI_TEXT_MUTED);
    }

    // ---------------- Bottom: name + actions ----------------
    CreateAction action = CREATE_NONE;
    {
        int y = (int)(screenHeight * 0.80f);
        int fieldW = (int)(340 * scale);
        int fieldH = (int)(38 * scale);
        int fx = (screenWidth - fieldW) / 2;

        UI_TextShadowCentered("Name", screenWidth / 2, y - small - UI_SP(scale, 2), small,
                              UI_TEXT_SECOND);

        Rectangle field = { (float)fx, (float)y, (float)fieldW, (float)fieldH };
        bool hovered = CheckCollisionPointRec(UI_PointerPos(), field);
        if (UI_PointerClicked()) g_nameFocused = hovered;

        bool navField = UINav_Item(field);
        if (navField) {
            NavRing(field);
            // Confirm arms the field; confirm again (or Escape) releases
            // it. Without the release, Enter would type forever and the
            // pad could never leave the name box.
            if (UINav_Confirm()) g_nameFocused = !g_nameFocused;
        }

        DrawRectangleRec(field, (Color){ 20, 21, 28, 255 });
        DrawRectangleLinesEx(field, g_nameFocused ? 2.0f : 1.0f,
                             g_nameFocused ? UI_GOLD : UI_GOLD_DIM);

        // Typed input. Only printable ASCII, and a hard length cap, so
        // a name can't overflow the nameplate or the save line.
        if (g_nameFocused) {
            int c = GetCharPressed();
            while (c > 0) {
                int len = (int)strlen(g_draft.name);
                if (c >= 32 && c <= 126 && len < CHARACTER_NAME_MAX) {
                    g_draft.name[len] = (char)c;
                    g_draft.name[len + 1] = '\0';
                }
                c = GetCharPressed();
            }
            if (IsKeyPressed(KEY_BACKSPACE)) {
                int len = (int)strlen(g_draft.name);
                if (len > 0) g_draft.name[len - 1] = '\0';
            }
        }

        int tw = UITextWidth(g_draft.name, font);
        UI_TextShadow(g_draft.name, fx + UI_SP(scale, 3), y + (fieldH - font) / 2, font,
                      UI_TEXT_PRIMARY);
        // Blinking caret while focused.
        if (g_nameFocused && fmodf(g_previewTime, 1.0f) < 0.5f) {
            DrawRectangle(fx + UI_SP(scale, 3) + tw + 2, y + (fieldH - font) / 2,
                          (int)(2 * scale), font, UI_GOLD);
        }

        y += fieldH + UI_SP(scale, 5);
        int btnW = (int)(200 * scale);
        int btnH = (int)(44 * scale);
        int gap = UI_SP(scale, 4);
        int bx = (screenWidth - (btnW * 2 + gap)) / 2;

        if (NavButton((Rectangle){ (float)bx, (float)y, (float)btnW, (float)btnH },
                      "Back", font, true, false)) {
            action = CREATE_CANCEL;
        }
        // A nameless character is the one thing that isn't allowed.
        bool canCreate = (strlen(g_draft.name) > 0);
        if (NavButton((Rectangle){ (float)(bx + btnW + gap), (float)y, (float)btnW, (float)btnH },
                      "Enter Ascalon", font, canCreate, canCreate)) {
            action = CREATE_CONFIRM;
        }
        if (!canCreate) {
            UI_TextShadowCentered("Your character needs a name.", screenWidth / 2,
                                  y + btnH + UI_SP(scale, 2), tiny, UI_NEGATIVE);
        }
    }

    UINav_End();

    if (IsKeyPressed(KEY_ESCAPE) ||
        (IsGamepadAvailable(0) && IsGamepadButtonPressed(0, GAMEPAD_BUTTON_RIGHT_FACE_RIGHT))) {
        action = CREATE_CANCEL;
    }
    if (action == CREATE_CONFIRM) g_character = g_draft;
    return action;
}
