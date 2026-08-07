#include "ui_panels.h"
#include "ui_hit.h"
#include "ui_cursor.h"
#include "save.h"
#include "entity.h"
#include "items.h"
#include "attributes.h"
#include "quests.h"
#include "world.h"
#include "skill.h"
#include "skillbook.h"
#include "character.h"
#include "ui_font.h"
#include "ui_theme.h"
#include "ui_hints.h"
#include "audio.h"
#include "ui_focus.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

#define PLAYER_INDEX 0
#define DIALOG_WALKAWAY_DISTANCE 130.0f

static bool g_invOpen = false;
static bool g_attrOpen = false;
static bool g_equipOpen = false;
static bool g_skillsOpen = false;
static int g_dialogNpc = -1;   // entity index, -1 = closed
static bool g_shopOpen = false;
static bool g_craftOpen = false;
static bool g_trainerOpen = false;
static bool g_professionOpen = false;

static Rectangle g_invRect, g_attrRect, g_dialogRect, g_shopRect, g_equipRect,
                 g_craftRect, g_skillsRect, g_trainerRect, g_professionRect;

// --- Prophecies pacing for the second profession ---------------------
//
// GW1 does two separate things here, a campaign apart, and the gap
// between them IS the design: you are handed a secondary early, once
// you've played the primary enough to have an opinion, and you cannot
// re-choose it until much later. A build you can rewrite on a whim
// isn't a build, it's a menu.
//
// Sir Tydus's "A Second Profession" is the canon gate: he sends you off
// to find a trainer, and only then will one teach you. Re-choosing is
// held back to level 10, standing in for the campaign's worth of
// distance GW1 puts between the two.
#define SECONDARY_QUEST "A Second Profession"
#define SECONDARY_CHANGE_LEVEL 10

static bool SecondaryGrantAllowed(void) {
    return Quests_IsDoneByName(SECONDARY_QUEST);
}

static bool SecondaryChangeAllowed(const Entity *player) {
    return player->level >= SECONDARY_CHANGE_LEVEL;
}

// Which bar slot the skills panel is editing, -1 = none. Click a slot to
// arm it, then click a known skill to drop it in - two clicks, works
// identically with the mouse and with the pad's menu cursor.
static int g_armedBarSlot = -1;

// The merchant's stock: fixed-power items at fixed prices, in GW1's
// spirit where the merchant sells the basics and rarity is cosmetic.
typedef struct { Item item; int price; } ShopEntry;
static const ShopEntry g_shopStock[] = {
    { { ITEM_WEAPON, "Long Sword",  15, 22, 28.0f,  1.33f, 0, 1, false }, 80 },
    { { ITEM_WEAPON, "War Hammer",  19, 35, 30.0f,  1.75f, 0, 1, false }, 120 },
    { { ITEM_WEAPON, "Fire Staff",  11, 22, 220.0f, 1.75f, 0, 1, false }, 100 },
    // GW1's two workhorse kits, at GW1's merchant price point.
    { { ITEM_KIT_SALVAGE, "Salvage Kit", 0, 0, 0, 0, 0, 25, false }, 100 },
    { { ITEM_KIT_ID, "Identification Kit", 0, 0, 0, 0, 0, 25, false }, 100 },
};
#define SHOP_STOCK_COUNT (int)(sizeof(g_shopStock) / sizeof(g_shopStock[0]))

// The Armorer's recipes: gold + Charr Hides in, armor out - GW1's
// craft-only armor economy. No armor ever drops or sits in a shop.
typedef struct { Item item; int gold; int hides; } CraftEntry;
#define CRAFT_LIST_COUNT 2
#define CRAFT_MATERIAL "Charr Hide"

// GW1 armor is per-profession, and the ceiling differs: a Warrior tops
// out at AL 80, a Ranger at 70, every caster at 60. The armorer stocks
// two tiers above whatever that profession started in, so the gap
// between a front-liner and a caster stays the gap GW1 designed.
static int ArmorBaseFor(Profession p) {
    switch (p) {
        case PROF_WARRIOR: return 40;
        case PROF_RANGER:  return 35;
        default:           return 30;
    }
}

static void BuildCraftList(Profession p, CraftEntry out[CRAFT_LIST_COUNT]) {
    static const int gold[CRAFT_LIST_COUNT]  = { 100, 250 };
    static const int hides[CRAFT_LIST_COUNT] = { 3, 6 };
    int base = ArmorBaseFor(p);
    for (int i = 0; i < CRAFT_LIST_COUNT; i++) {
        int al = base + 15 * (i + 1);
        memset(&out[i], 0, sizeof(out[i]));
        out[i].item.kind = ITEM_ARMOR;
        out[i].item.armor = al;
        out[i].item.count = 1;
        snprintf(out[i].item.name, sizeof(out[i].item.name), "%s Armour (AL %d)",
                 Character_ProfessionName(p), al);
        out[i].gold = gold[i];
        out[i].hides = hides[i];
    }
}

// GW1's kit flow: click a kit to arm it, then click the item to use it
// on. -1 = no kit armed. Cleared when panels close or the kit is spent.
static int g_armedKit = -1;

void UI_OpenNpcDialog(int entityIndex) {
    if (g_dialogNpc != entityIndex) {
        Audio_Play(SFX_UI_OPEN);
        UIFocus_Clear(); // a new conversation starts at its first reply
    }
    g_dialogNpc = entityIndex;
    g_shopOpen = false;
    g_craftOpen = false;
}

bool UI_IsNpcDialogOpen(void) {
    return g_dialogNpc >= 0;
}

void UI_CloseNpcDialog(void) {
    if (g_dialogNpc >= 0) Audio_Play(SFX_UI_CLOSE);
    UIFocus_Clear();
    g_dialogNpc = -1;
    g_shopOpen = false;
    g_craftOpen = false;
    g_trainerOpen = false;
    g_professionOpen = false;
}

bool UI_IsInventoryOpen(void) { return g_invOpen; }
bool UI_IsAttributesOpen(void) { return g_attrOpen; }
bool UI_IsEquipmentOpen(void) { return g_equipOpen; }
bool UI_IsSkillsOpen(void) { return g_skillsOpen; }

// The pad's route to the build editor - see UI_ToggleBags for the same
// idea applied to inventory and gear.
void UI_ToggleSkills(void) {
    g_skillsOpen = !g_skillsOpen;
    g_armedBarSlot = -1;
}

void UI_OpenPanel(PanelId panel) {
    switch (panel) {
        case PANEL_SKILLS:     g_skillsOpen = true; g_armedBarSlot = -1; break;
        case PANEL_EQUIPMENT:  g_equipOpen = true; break;
        case PANEL_INVENTORY:  g_invOpen = true; break;
        case PANEL_ATTRIBUTES: g_attrOpen = true; break;
    }
}

// The pad's Y button opens "the bags": inventory + equipment together,
// since on a controller you almost always want both at once.
void UI_ToggleBags(void) {
    bool anyOpen = g_invOpen || g_equipOpen;
    g_invOpen = !anyOpen;
    g_equipOpen = !anyOpen;
}

void UI_ClosePanels(void) {
    g_invOpen = false;
    g_attrOpen = false;
    g_equipOpen = false;
    g_skillsOpen = false;
    g_armedKit = -1;
    g_armedBarSlot = -1;
}

// Shared window header: the title in gold, the key that toggles this
// window as a badge beside it, and a close box on the right. Every
// panel used to be a bare rectangle with a title string, which left the
// player guessing both how it opened and how to get rid of it. Returns
// true when the close box was clicked this frame.
static bool PanelHeader(Rectangle rect, const char *title, const char *hotkey,
                        const char *padkey, int font, int pad, float scale) {
    int x = (int)rect.x + pad;
    int y = (int)rect.y + pad;
    // Window headers get the display face, body rows don't - that split
    // is what keeps a serif from turning a dense list into mush.
    UITextDisplay(title, x, y, font, UI_GOLD);

    // Binding badge: pad glyph when a controller is present, key otherwise.
    const char *badge = (IsGamepadAvailable(0) && padkey) ? padkey : hotkey;
    if (badge) {
        int tw = UITextDisplayWidth(title, font);
        UI_KeyBadge(badge, x + tw + (int)(8 * scale), y - (int)(3 * scale),
                    (int)(font * 0.85f), true);
    }

    int box = (int)(16 * scale);
    Rectangle close = { rect.x + rect.width - pad - box, rect.y + pad - (int)(2 * scale),
                        (float)box, (float)box };
    bool hovered = CheckCollisionPointRec(UI_PointerPos(), close);
    DrawRectangleRounded(close, 0.3f, 6,
                         hovered ? (Color){ 132, 58, 58, 255 } : (Color){ 52, 44, 48, 235 });
    DrawRectangleRoundedLines(close, 0.3f, 6, hovered ? (Color){ 235, 180, 180, 255 } : UI_GOLD_DIM);
    // A drawn X, not a glyph - the bundled font's lowercase x sat
    // off-center in the box at small sizes.
    float inset = box * 0.3f;
    Color mark = hovered ? RAYWHITE : (Color){ 208, 200, 186, 255 };
    DrawLineEx((Vector2){ close.x + inset, close.y + inset },
               (Vector2){ close.x + box - inset, close.y + box - inset }, 1.8f, mark);
    DrawLineEx((Vector2){ close.x + box - inset, close.y + inset },
               (Vector2){ close.x + inset, close.y + box - inset }, 1.8f, mark);

    bool closing = hovered && UI_PointerClicked();
    if (closing) Audio_Play(SFX_UI_CLOSE);
    return closing;
}

static void DrawInventory(Entity *player, int screenHeight) {
    float scale = UI_Scale(screenHeight);
    int rowH = (int)(24 * scale);
    int font = (int)(12 * scale);
    int pad = (int)(10 * scale);
    int w = (int)(300 * scale);
    int h = pad * 3 + font + (g_inventoryCount + 1) * rowH;

    // Below the party window, which now owns the upper-left.
    g_invRect = (Rectangle){ (float)(20 * scale), (float)(310 * scale), (float)w, (float)h };
    UIHit_Claim(g_invRect);
    UI_ThemePanel(g_invRect, scale, pad + font + pad / 2);

    int x = (int)g_invRect.x + pad;
    int y = (int)g_invRect.y + pad;
    char title[64];
    bool armed = (g_armedKit >= 0 && g_armedKit < g_inventoryCount);
    if (armed) {
        snprintf(title, sizeof(title), "%s - pick a target item",
                 g_inventory[g_armedKit].kind == ITEM_KIT_ID ? "Identify" : "Salvage");
    } else {
        snprintf(title, sizeof(title), "Inventory   %d gold", g_gold);
    }
    if (PanelHeader(g_invRect, title, "I", "Y", font, pad, scale)) {
        g_invOpen = false;
        g_armedKit = -1;
        return;
    }
    y += font + pad;

    if (g_armedKit >= g_inventoryCount) g_armedKit = -1; // kit vanished

    bool click = UI_PointerClicked();
    Vector2 mouse = UI_PointerPos();

    for (int i = 0; i < g_inventoryCount; i++) {
        Item *it = &g_inventory[i];
        Rectangle row = { g_invRect.x + 4, (float)y - 2, g_invRect.width - 8, (float)rowH };
        bool hovered = CheckCollisionPointRec(mouse, row);
        bool equipped = (i == g_equippedWeapon || i == g_equippedArmor);

        if (hovered) DrawRectangleRec(row, (Color){ 60, 60, 80, 255 });

        char label[64];
        Color rowColor;
        if (it->kind == ITEM_WEAPON && it->unidentified) {
            // Masked until an Identification Kit reveals it, like GW1.
            snprintf(label, sizeof(label), "%s", Items_DisplayName(it));
            rowColor = (Color){ 190, 150, 220, 255 };
        } else if (it->kind == ITEM_WEAPON) {
            snprintf(label, sizeof(label), "%s %d-%d%s", it->name, it->dmgMin, it->dmgMax,
                     equipped ? "  [equipped]" : "");
            rowColor = equipped ? SKYBLUE : RAYWHITE;
        } else if (it->kind == ITEM_MATERIAL) {
            snprintf(label, sizeof(label), "%s x%d", it->name, it->count);
            rowColor = (Color){ 200, 180, 140, 255 };
        } else if (it->kind == ITEM_KIT_SALVAGE || it->kind == ITEM_KIT_ID) {
            snprintf(label, sizeof(label), "%s [%d uses]%s", it->name, it->count,
                     (i == g_armedKit) ? "  <armed>" : "");
            rowColor = (i == g_armedKit) ? GOLD : (Color){ 160, 200, 210, 255 };
        } else {
            snprintf(label, sizeof(label), "%s%s", it->name, equipped ? "  [equipped]" : "");
            rowColor = equipped ? SKYBLUE : RAYWHITE;
        }
        UIText(label, x, y, font, rowColor);

        if (hovered && click) {
            if (it->kind == ITEM_KIT_SALVAGE || it->kind == ITEM_KIT_ID) {
                // Arm/disarm the kit; the next item click applies it.
                g_armedKit = (g_armedKit == i) ? -1 : i;
            } else if (g_armedKit >= 0) {
                g_armedKit = Items_UseKitOn(g_armedKit, i);
                if (g_armedKit == -2) g_armedKit = -1; // invalid target: disarm
                Save_Write();
                break; // indices may have shifted - relayout next frame
            } else if (it->kind == ITEM_WEAPON) {
                Items_EquipWeapon(player, i); // rejects unidentified itself
            } else if (it->kind == ITEM_ARMOR) {
                Items_EquipArmor(player, i);
            }
        }
        y += rowH;
    }

    if (g_inventoryCount == 0) {
        UIText("(empty - kill something)", x, y, font, GRAY);
    }
}

// ---------------------------------------------------------------------
// The skills panel: your eight-slot bar on top, everything you've
// learned underneath. This is where a build actually gets made, so it's
// a first-class window rather than a corner of the attributes screen.
//
// GW1 only lets you rearrange skills in an outpost - you commit to a
// build before you leave town and live with it out in the field. That
// rule is what gives build-crafting its weight, so it's enforced here
// with a visible explanation rather than a silently dead click.
static void DrawSkillsPanel(Entity *player, int screenWidth, int screenHeight) {
    float scale = UI_Scale(screenHeight);
    int font = (int)(12 * scale);
    int small = (int)(10 * scale);
    int pad = (int)(10 * scale);
    int slot = (int)(46 * scale);
    int gap = (int)(5 * scale);
    int rowH = (int)(26 * scale);
    int w = SKILL_BAR_SIZE * slot + (SKILL_BAR_SIZE - 1) * gap + pad * 2;

    bool canEdit = (World_GetMode() == MODE_OUTPOST);

    // Count what the character owns so the window hugs the list.
    int known = 0;
    for (int i = 0; i < g_skillCount; i++) {
        if (Skillbook_IsUnlocked(i) &&
            Skillbook_IsUsableBy(i, player->primaryProfession, player->secondaryProfession)) known++;
    }
    if (known == 0) known = 1; // room for the "nothing yet" line

    int h = pad * 3 + font + slot + (int)(14 * scale) + small + known * rowH + pad;

    g_skillsRect = (Rectangle){ (float)(screenWidth - w) / 2.0f, (float)(110 * scale),
                                (float)w, (float)h };
    UIHit_Claim(g_skillsRect);
    UI_ThemePanel(g_skillsRect, scale, pad + font + pad / 2);

    char title[80];
    snprintf(title, sizeof(title), "Skills   %d known   %d skill point%s",
             Skillbook_UnlockedCount(), g_skillPoints, g_skillPoints == 1 ? "" : "s");
    if (PanelHeader(g_skillsRect, title, "L", NULL, font, pad, scale)) {
        g_skillsOpen = false;
        g_armedBarSlot = -1;
        return;
    }

    int x = (int)g_skillsRect.x + pad;
    int y = (int)g_skillsRect.y + pad + font + pad;

    bool click = UI_PointerClicked();
    Vector2 mouse = UI_PointerPos();

    // --- The bar itself: eight slots, empty ones plainly empty ---
    for (int i = 0; i < SKILL_BAR_SIZE; i++) {
        Rectangle r = { (float)(x + i * (slot + gap)), (float)y, (float)slot, (float)slot };
        int id = player->skillBar[i];
        bool hovered = canEdit && CheckCollisionPointRec(mouse, r);

        if (id >= 0) {
            UI_DrawSkillIcon(id, r);
        } else {
            DrawRectangleRec(r, (Color){ 20, 20, 25, 255 });
            DrawRectangleLinesEx(r, 2, (Color){ 10, 10, 14, 255 });
            DrawRectangleLinesEx((Rectangle){ r.x + 2, r.y + 2, r.width - 4, r.height - 4 },
                                 1, (Color){ 62, 60, 54, 255 });
            UI_TextShadowCentered("+", (int)(r.x + r.width / 2),
                                  (int)(r.y + r.height / 2 - small), small,
                                  (Color){ 110, 106, 96, 255 });
        }

        if (i == g_armedBarSlot) {
            DrawRectangleLinesEx((Rectangle){ r.x - 2, r.y - 2, r.width + 4, r.height + 4 },
                                 2, UI_GOLD);
        } else if (hovered) {
            DrawRectangleLinesEx(r, 2, (Color){ 200, 190, 160, 180 });
        }

        char num[4];
        snprintf(num, sizeof(num), "%d", i + 1);
        UI_TextShadow(num, (int)r.x + 3, (int)(r.y + r.height) - small - 2, small,
                      (Color){ 190, 184, 168, 220 });

        if (hovered && click) {
            // Clicking the armed slot again empties it; otherwise arm it.
            if (g_armedBarSlot == i) {
                player->skillBar[i] = -1;
                player->skillRecharge[i] = 0.0f;
                g_armedBarSlot = -1;
                Save_Write();
            } else {
                g_armedBarSlot = i;
            }
        }
    }
    y += slot + (int)(8 * scale);

    const char *hint;
    Color hintColor;
    if (!canEdit) {
        hint = "Skills can only be changed in an outpost.";
        hintColor = (Color){ 214, 150, 128, 255 };
    } else if (g_armedBarSlot >= 0) {
        hint = "Pick a skill below, or click the slot again to clear it.";
        hintColor = UI_GOLD;
    } else {
        hint = "Click a slot, then a skill, to build your bar.";
        hintColor = (Color){ 150, 146, 134, 255 };
    }
    UIText(hint, x, y, small, hintColor);
    y += small + (int)(6 * scale);

    // --- Everything you've learned ---
    if (Skillbook_UnlockedCount() == 0) {
        UIText("(no skills yet - try a quest giver or a trainer)", x, y, font, GRAY);
        return;
    }

    for (int i = 0; i < g_skillCount; i++) {
        if (!Skillbook_IsUnlocked(i)) continue;
        if (!Skillbook_IsUsableBy(i, player->primaryProfession, player->secondaryProfession)) continue;

        const Skill *s = &g_skillDB[i];

        // Already on the bar? Show it as such rather than letting the
        // player silently slot the same skill twice.
        int onBar = -1;
        for (int b = 0; b < SKILL_BAR_SIZE; b++) {
            if (player->skillBar[b] == i) onBar = b;
        }

        Rectangle row = { g_skillsRect.x + 4, (float)y - 2, g_skillsRect.width - 8, (float)rowH };
        bool selectable = canEdit && g_armedBarSlot >= 0 && onBar < 0;
        bool hovered = CheckCollisionPointRec(mouse, row);
        if (hovered && selectable) DrawRectangleRec(row, (Color){ 60, 60, 80, 255 });

        // A small icon makes the list scan the same way the bar does.
        Rectangle icon = { (float)x, (float)y - 1, (float)(rowH - 4), (float)(rowH - 4) };
        UI_DrawSkillIcon(i, icon);

        char label[96];
        if (s->energyCost > 0) {
            snprintf(label, sizeof(label), "%.31s   %dE   %.0fs recharge",
                     s->name, s->energyCost, (double)s->recharge);
        } else if (s->adrenalineCost > 0) {
            snprintf(label, sizeof(label), "%.31s   %d adrenaline", s->name, s->adrenalineCost);
        } else {
            snprintf(label, sizeof(label), "%.31s   free   %.0fs recharge", s->name, (double)s->recharge);
        }
        Color c = s->isElite ? UI_GOLD : (onBar >= 0 ? SKYBLUE : RAYWHITE);
        int textX = x + (int)icon.width + (int)(8 * scale);
        UIText(label, textX, y, font, c);

        // Right-aligned status: which slot it occupies, or its line.
        char right[40];
        if (onBar >= 0) snprintf(right, sizeof(right), "slot %d", onBar + 1);
        else snprintf(right, sizeof(right), "%.31s", g_attributeNames[s->attribute]);
        int rw = UITextWidth(right, small);
        UIText(right, (int)(g_skillsRect.x + g_skillsRect.width) - pad - rw,
               y + (font - small) / 2, small,
               onBar >= 0 ? SKYBLUE : (Color){ 150, 146, 134, 255 });

        if (hovered && click && selectable) {
            player->skillBar[g_armedBarSlot] = i;
            player->skillRecharge[g_armedBarSlot] = 0.0f;
            g_armedBarSlot = -1;
            Save_Write();
        }
        y += rowH;
    }
}

// The skill trainer's stock: every non-elite skill your professions can
// use that you don't already know. Elites are absent by design - those
// come off bosses.
static void DrawTrainer(Entity *player, int screenWidth, int screenHeight) {
    float scale = UI_Scale(screenHeight);
    int font = (int)(12 * scale);
    int small = (int)(10 * scale);
    int pad = (int)(10 * scale);
    int rowH = (int)(30 * scale);
    int w = (int)(440 * scale);

    int forSale = 0;
    for (int i = 0; i < g_skillCount; i++) {
        if (Skillbook_IsUnlocked(i) || g_skillDB[i].isElite) continue;
        if (Skillbook_IsUsableBy(i, player->primaryProfession, player->secondaryProfession)) forSale++;
    }
    int rows = forSale > 0 ? forSale : 1;
    int h = pad * 3 + font + small + (int)(6 * scale) + rows * rowH + pad;

    g_trainerRect = (Rectangle){ (float)(screenWidth - w) / 2.0f, (float)(90 * scale),
                                 (float)w, (float)h };
    UIHit_Claim(g_trainerRect);
    UI_ThemePanel(g_trainerRect, scale, pad + font + pad / 2);

    char title[96];
    snprintf(title, sizeof(title), "Skill Trainer   %d skill point%s   %dg",
             g_skillPoints, g_skillPoints == 1 ? "" : "s", g_gold);
    if (PanelHeader(g_trainerRect, title, NULL, NULL, font, pad, scale)) {
        g_trainerOpen = false;
        return;
    }

    int x = (int)g_trainerRect.x + pad;
    int y = (int)g_trainerRect.y + pad + font + pad;

    char sub[96];
    snprintf(sub, sizeof(sub), "Each skill costs 1 skill point and %d gold. Elites must be captured.",
             Skillbook_TrainerGoldCost());
    UIText(sub, x, y, small, (Color){ 150, 146, 134, 255 });
    y += small + (int)(6 * scale);

    bool click = UI_PointerClicked();
    Vector2 mouse = UI_PointerPos();

    if (forSale == 0) {
        UIText("You've learned everything I can teach.", x, y, font, GRAY);
        return;
    }

    for (int i = 0; i < g_skillCount; i++) {
        if (Skillbook_IsUnlocked(i) || g_skillDB[i].isElite) continue;
        if (!Skillbook_IsUsableBy(i, player->primaryProfession, player->secondaryProfession)) continue;

        const Skill *s = &g_skillDB[i];
        bool affordable = Skillbook_CanBuy(i, player->primaryProfession, player->secondaryProfession);

        Rectangle row = { g_trainerRect.x + 4, (float)y - 2, g_trainerRect.width - 8, (float)rowH };
        bool focused = UIFocus_Item();
        bool hovered = CheckCollisionPointRec(mouse, row);
        if ((hovered || focused) && affordable) DrawRectangleRec(row, (Color){ 60, 60, 80, 255 });
        if (focused) DrawRectangleLinesEx(row, 1.5f, UI_GOLD);

        Rectangle icon = { (float)x, (float)y - 1, (float)(rowH - 6), (float)(rowH - 6) };
        UI_DrawSkillIcon(i, icon);

        char label[112];
        snprintf(label, sizeof(label), "%.31s   %.31s", s->name, g_attributeNames[s->attribute]);
        UIText(label, x + (int)icon.width + (int)(8 * scale), y, font,
               affordable ? RAYWHITE : GRAY);

        const char *why = affordable ? "buy"
                        : (g_skillPoints < 1) ? "needs a skill point" : "not enough gold";
        int rw = UITextWidth(why, small);
        UIText(why, (int)(g_trainerRect.x + g_trainerRect.width) - pad - rw,
               y + (font - small) / 2, small,
               affordable ? (Color){ 140, 220, 150, 255 } : (Color){ 170, 120, 110, 255 });

        if (((hovered && click) || (focused && UIFocus_Confirm())) && affordable) {
            if (Skillbook_Buy(i, player->primaryProfession, player->secondaryProfession)) {
                Audio_Play(SFX_UI_CONFIRM);
                char msg[96];
                snprintf(msg, sizeof(msg), "Skill learned:  %.31s", s->name);
                UI_Notify(msg);
                Save_Write();
            }
            break; // the list just changed - relayout next frame
        }
        y += rowH;
    }
}

static void DrawAttributes(Entity *player, int screenWidth, int screenHeight) {
    float scale = UI_Scale(screenHeight);
    int rowH = (int)(28 * scale);
    int font = (int)(12 * scale);
    int pad = (int)(10 * scale);
    int w = (int)(360 * scale);

    int accessible = 0;
    for (int a = 0; a < ATTR_COUNT; a++) {
        if (Attribute_Accessible((AttributeKind)a, player->primaryProfession, player->secondaryProfession)) accessible++;
    }
    int h = pad * 3 + font + (accessible + 1) * rowH;

    g_attrRect = (Rectangle){ (float)(screenWidth - w) / 2.0f, (float)(140 * scale), (float)w, (float)h };
    UIHit_Claim(g_attrRect);
    UI_ThemePanel(g_attrRect, scale, pad + font + pad / 2);

    int x = (int)g_attrRect.x + pad;
    int y = (int)g_attrRect.y + pad;
    char title[64];
    snprintf(title, sizeof(title), "Attributes   %d free", player->attributePoints);
    if (PanelHeader(g_attrRect, title, "K", NULL, font, pad, scale)) {
        g_attrOpen = false;
        return;
    }
    y += font + pad;

    bool click = UI_PointerClicked();
    Vector2 mouse = UI_PointerPos();
    int btn = (int)(20 * scale);

    for (int a = 0; a < ATTR_COUNT; a++) {
        if (!Attribute_Accessible((AttributeKind)a, player->primaryProfession, player->secondaryProfession)) continue;

        int rank = player->attributeRank[a];
        char label[64];
        snprintf(label, sizeof(label), "%s: %d", g_attributeNames[a], rank);
        UIText(label, x, y, font, g_attributeIsPrimary[a] ? GOLD : RAYWHITE);

        // "+" button: costs the GW1 incremental amount for the next rank.
        Rectangle plus = { g_attrRect.x + g_attrRect.width - pad - btn, (float)y - 2, (float)btn, (float)btn };
        Rectangle minus = { plus.x - btn - 6, (float)y - 2, (float)btn, (float)btn };

        int upCost = (rank < ATTRIBUTE_RANK_CAP)
            ? g_attrCumulativeCost[rank + 1] - g_attrCumulativeCost[rank] : 0;

        bool canUp = rank < ATTRIBUTE_RANK_CAP && player->attributePoints >= upCost;
        if (click && !canUp && CheckCollisionPointRec(mouse, plus)) Audio_Play(SFX_UI_DENY);
        DrawRectangleRec(plus, canUp ? (Color){ 60, 100, 60, 255 } : (Color){ 45, 45, 45, 255 });
        DrawRectangleLinesEx(plus, 1, LIGHTGRAY);
        UIText("+", (int)plus.x + btn / 3, (int)plus.y + 2, font, RAYWHITE);

        bool canDown = rank > 0;
        DrawRectangleRec(minus, canDown ? (Color){ 100, 60, 60, 255 } : (Color){ 45, 45, 45, 255 });
        DrawRectangleLinesEx(minus, 1, LIGHTGRAY);
        UIText("-", (int)minus.x + btn / 3, (int)minus.y + 2, font, RAYWHITE);

        if (rank < ATTRIBUTE_RANK_CAP) {
            char cost[24];
            snprintf(cost, sizeof(cost), "next: %d", upCost);
            UIText(cost, (int)minus.x - (int)(70 * scale), y, font, GRAY);
        }

        if (click && canUp && CheckCollisionPointRec(mouse, plus)) {
            Audio_Play(SFX_UI_CLICK);
            player->attributeRank[a]++;
            player->attributePoints -= upCost;
            // Energy Storage IS maximum energy, so the pool has to move
            // the instant the rank does - that's the feedback that makes
            // the primary attribute legible.
            Entity_RecomputeAttributeStats(player);
        }
        if (click && canDown && CheckCollisionPointRec(mouse, minus)) {
            Audio_Play(SFX_UI_CLICK);
            int refund = g_attrCumulativeCost[rank] - g_attrCumulativeCost[rank - 1];
            player->attributeRank[a]--;
            player->attributePoints += refund;
            Entity_RecomputeAttributeStats(player);
        }
        y += rowH;
    }
}

// "300 XP, 150g, Bane Signet" - the full payout on one line. The skill
// especially has to be named up front: it's the reward that changes what
// your character can do, and a player deciding whether a quest is worth
// the trip needs to see it before they commit, not after.
static void QuestRewardSummary(const Quest *q, const Entity *player, char *out, int outSize) {
    int n = snprintf(out, outSize, "%d XP, %dg", q->rewardXP, q->rewardGold);
    if (n < 0 || n >= outSize) return;
    if (q->rewardItem) {
        n += snprintf(out + n, outSize - n, ", %.31s", q->rewardItem->name);
        if (n < 0 || n >= outSize) return;
    }
    int taught = Quests_ResolveRewardSkill(q, player);
    if (taught >= 0 && taught < g_skillCount) {
        snprintf(out + n, outSize - n, ", %.31s", g_skillDB[taught].name);
    }
}

// One clickable dialog button; returns true when clicked this frame
// (or, for the dialog's single action button, confirmed on the pad).
static bool DialogButton(Rectangle rect, const char *label, int font, bool enabled) {
    // Registered even when disabled: a reply you can't take yet is still
    // a line the player steps past, and skipping it would make the
    // highlight jump unpredictably as requirements are met.
    bool focused = UIFocus_Item();

    Vector2 mouse = UI_PointerPos();
    bool hovered = enabled && CheckCollisionPointRec(mouse, rect);
    DrawRectangleRec(rect, !enabled ? (Color){ 40, 40, 40, 255 }
                     : (hovered || focused) ? (Color){ 80, 90, 120, 255 }
                                            : (Color){ 55, 60, 80, 255 });
    DrawRectangleLinesEx(rect, focused ? 2.0f : 1.0f,
                         !enabled ? (Color){ 70, 70, 70, 255 }
                                  : (focused ? UI_GOLD : UI_GOLD_DIM));
    UIText(label, (int)rect.x + 8, (int)rect.y + ((int)rect.height - font) / 2, font,
           enabled ? RAYWHITE : GRAY);

    bool picked = (hovered && UI_PointerClicked()) ||
                  (focused && enabled && UIFocus_Confirm());
    if (picked) Audio_Play(SFX_UI_CLICK);
    if (focused && !enabled && UIFocus_Confirm()) Audio_Play(SFX_UI_DENY);
    return picked;
}

// Which merchant tab is showing: GW1's merchant window splits trade
// into a Buy tab (the merchant's stock) and a Sell tab (your bags).
static int g_shopTab = 0; // 0 = Buy, 1 = Sell

static void DrawShop(int screenWidth, int screenHeight) {
    float scale = UI_Scale(screenHeight);
    int rowH = (int)(26 * scale);
    int font = (int)(12 * scale);
    int pad = (int)(10 * scale);
    int w = (int)(380 * scale);
    int tabH = (int)(30 * scale);
    int rows = (g_shopTab == 0) ? SHOP_STOCK_COUNT
                                : (g_inventoryCount > 0 ? g_inventoryCount : 1);
    int h = pad * 4 + font + tabH + (rows + 1) * rowH;

    g_shopRect = (Rectangle){ (float)(screenWidth - w) / 2.0f, (float)(90 * scale), (float)w, (float)h };
    UIHit_Claim(g_shopRect);
    UI_ThemePanel(g_shopRect, scale, pad + font + pad / 2);

    int x = (int)g_shopRect.x + pad;
    int y = (int)g_shopRect.y + pad;
    char title[48];
    snprintf(title, sizeof(title), "Merchant   %d gold", g_gold);
    if (PanelHeader(g_shopRect, title, NULL, NULL, font, pad, scale)) {
        g_shopOpen = false;
        return;
    }
    y += font + pad;

    bool click = UI_PointerClicked();
    Vector2 mouse = UI_PointerPos();

    // --- Tabs: Buy | Sell ---
    {
        float tabW = (g_shopRect.width - 2 * pad) / 2.0f;
        const char *names[2] = { "Buy", "Sell" };
        for (int t = 0; t < 2; t++) {
            Rectangle tab = { g_shopRect.x + pad + t * tabW, (float)y, tabW, (float)tabH };
            bool active = (g_shopTab == t);
            // Registered unconditionally and BEFORE any short-circuiting
            // test: the focus index is positional, so an item that skips
            // registration on some frames shifts every item after it.
            bool focused = UIFocus_Item();
            bool hovered = CheckCollisionPointRec(mouse, tab);
            DrawRectangleRec(tab, active ? (Color){ 60, 66, 90, 255 }
                            : (hovered || focused) ? (Color){ 45, 50, 70, 255 }
                                                   : (Color){ 32, 35, 48, 255 });
            DrawRectangleLinesEx(tab, focused ? 2.0f : 1.0f, focused ? UI_GOLD : UI_GOLD_DIM);
            if (active) {
                // Gold underline marks the live tab.
                DrawRectangle((int)tab.x, (int)(tab.y + tab.height - 3), (int)tab.width, 3, UI_GOLD);
            }
            int tw = UITextWidth(names[t], font);
            UIText(names[t], (int)(tab.x + (tab.width - tw) / 2),
                   (int)(tab.y + (tab.height - font) / 2), font, active ? RAYWHITE : LIGHTGRAY);
            if ((hovered && click) || (focused && UIFocus_Confirm())) {
                if (g_shopTab != t) Audio_Play(SFX_UI_CLICK);
                g_shopTab = t;
            }
        }
        y += tabH + pad;
    }

    if (g_shopTab == 0) {
        // --- Buy: the merchant's stock ---
        for (int i = 0; i < SHOP_STOCK_COUNT; i++) {
            const ShopEntry *entry = &g_shopStock[i];
            Rectangle row = { g_shopRect.x + 4, (float)y - 2, g_shopRect.width - 8, (float)rowH };
            bool canAfford = g_gold >= entry->price && g_inventoryCount < MAX_INVENTORY;
            bool focused = UIFocus_Item();
            bool hovered = CheckCollisionPointRec(mouse, row);
            if ((hovered || focused) && canAfford) DrawRectangleRec(row, (Color){ 60, 60, 80, 255 });
            if (focused) DrawRectangleLinesEx(row, 1.5f, UI_GOLD);

            char label[80];
            if (entry->item.kind == ITEM_WEAPON) {
                snprintf(label, sizeof(label), "%s %d-%d  (%dg)", entry->item.name,
                         entry->item.dmgMin, entry->item.dmgMax, entry->price);
            } else if (entry->item.kind == ITEM_KIT_SALVAGE || entry->item.kind == ITEM_KIT_ID) {
                snprintf(label, sizeof(label), "%s [%d uses]  (%dg)", entry->item.name,
                         entry->item.count, entry->price);
            } else {
                snprintf(label, sizeof(label), "%s  (%dg)", entry->item.name, entry->price);
            }
            UIText(label, x, y, font, canAfford ? RAYWHITE : GRAY);

            if (((hovered && click) || (focused && UIFocus_Confirm())) && canAfford) {
                g_gold -= entry->price;
                Items_AddToInventory(entry->item);
                Audio_Play(SFX_UI_CONFIRM);
            }
            y += rowH;
        }
    } else {
        // --- Sell: your inventory (equipped items can't be sold) ---
        if (g_inventoryCount == 0) {
            UIText("(nothing to sell)", x, y, font, GRAY);
        }
        for (int i = 0; i < g_inventoryCount; i++) {
            Item *it = &g_inventory[i];
            bool equipped = (i == g_equippedWeapon || i == g_equippedArmor);
            Rectangle row = { g_shopRect.x + 4, (float)y - 2, g_shopRect.width - 8, (float)rowH };
            bool focused = UIFocus_Item();
            bool hovered = !equipped && CheckCollisionPointRec(mouse, row);
            if (hovered || (focused && !equipped)) DrawRectangleRec(row, (Color){ 80, 60, 60, 255 });
            if (focused) DrawRectangleLinesEx(row, 1.5f, UI_GOLD);

            char label[80];
            if (it->kind == ITEM_MATERIAL) {
                snprintf(label, sizeof(label), "%s x%d  (+%dg each)", it->name, it->count,
                         Items_SellValue(it));
            } else if (it->kind == ITEM_KIT_SALVAGE || it->kind == ITEM_KIT_ID) {
                snprintf(label, sizeof(label), "%s [%d uses]  (+%dg)", it->name, it->count,
                         Items_SellValue(it));
            } else {
                snprintf(label, sizeof(label), "%s  (+%dg)%s", Items_DisplayName(it),
                         Items_SellValue(it), equipped ? "  [equipped]" : "");
            }
            UIText(label, x, y, font, equipped ? GRAY : RAYWHITE);

            if ((hovered && click) || (focused && !equipped && UIFocus_Confirm())) {
                Audio_Play(SFX_UI_CONFIRM);
                int value = Items_SellValue(it);
                if (it->kind == ITEM_MATERIAL) {
                    // One hide per click, so a stack isn't dumped by accident.
                    if (Items_ConsumeMaterial(it->name, 1)) g_gold += value;
                } else if (Items_RemoveFromInventory(i)) {
                    g_gold += value;
                }
                break; // indices shifted - redo layout next frame
            }
            y += rowH;
        }
    }
}

// Clicking an equipment slot cycles to the next compatible item in the
// inventory - quick swapping without hunting through the bag list.
static void CycleEquip(Entity *player, ItemKind kind) {
    int cur = (kind == ITEM_WEAPON) ? g_equippedWeapon : g_equippedArmor;
    int n = g_inventoryCount;
    for (int step = 1; step <= n; step++) {
        int i = (cur < 0) ? (step - 1) : (cur + step) % n;
        if (i < 0 || i >= n) continue;
        if (g_inventory[i].kind != kind) continue;
        if (kind == ITEM_WEAPON && g_inventory[i].unidentified) continue;
        if (i == cur) break; // only one option - nothing to cycle to
        if (kind == ITEM_WEAPON) Items_EquipWeapon(player, i);
        else Items_EquipArmor(player, i);
        break;
    }
}

// The equipment screen: what's in each slot, with the numbers that
// matter, plus a character summary - so "what do I have equipped
// where" is never a mystery. E toggles it; the pad's Y opens it with
// the inventory.
static void DrawEquipment(Entity *player, int screenHeight) {
    float scale = UI_Scale(screenHeight);
    int rowH = (int)(44 * scale);
    int font = (int)(12 * scale);
    int small = (int)(10 * scale);
    int pad = (int)(10 * scale);
    int w = (int)(300 * scale);
    int h = pad * 3 + font + rowH * 2 + font + (int)(8 * scale);

    // Above the inventory, left side.
    g_equipRect = (Rectangle){ (float)(20 * scale), (float)(140 * scale), (float)w, (float)h };
    UIHit_Claim(g_equipRect);
    UI_ThemePanel(g_equipRect, scale, pad + font + pad / 2);

    int x = (int)g_equipRect.x + pad;
    int y = (int)g_equipRect.y + pad;
    if (PanelHeader(g_equipRect, "Equipment", "E", "Y", font, pad, scale)) {
        g_equipOpen = false;
        return;
    }
    y += font + pad;

    bool click = UI_PointerClicked();
    Vector2 mouse = UI_PointerPos();

    const char *slotNames[2] = { "Weapon", "Armor" };
    for (int slot = 0; slot < 2; slot++) {
        ItemKind kind = (slot == 0) ? ITEM_WEAPON : ITEM_ARMOR;
        int idx = (slot == 0) ? g_equippedWeapon : g_equippedArmor;

        Rectangle row = { g_equipRect.x + 4, (float)y - 2, g_equipRect.width - 8, (float)rowH - 4 };
        bool hovered = CheckCollisionPointRec(mouse, row);
        if (hovered) DrawRectangleRec(row, (Color){ 60, 60, 80, 255 });
        DrawRectangleLinesEx(row, 1, (Color){ 90, 90, 110, 255 });

        UIText(slotNames[slot], x, y, small, LIGHTGRAY);
        char line[96];
        if (idx >= 0 && idx < g_inventoryCount) {
            const Item *it = &g_inventory[idx];
            if (kind == ITEM_WEAPON) {
                snprintf(line, sizeof(line), "%s   %d-%d dmg, %s, %.2fs",
                         it->name, it->dmgMin, it->dmgMax,
                         it->range > 60.0f ? "ranged" : "melee", it->attackInterval);
            } else {
                snprintf(line, sizeof(line), "%s   AL %d", it->name, it->armor);
            }
        } else {
            snprintf(line, sizeof(line), "(nothing equipped)");
        }
        UIText(line, x, y + small + 3, font, idx >= 0 ? SKYBLUE : GRAY);

        if (hovered && click) {
            Audio_Play(SFX_UI_CLICK);
            CycleEquip(player, kind);
        }
        y += rowH;
    }

    char summary[96];
    snprintf(summary, sizeof(summary), "Level %d   HP %d/%d   Energy %d/%d   AL %d",
             player->level, player->hp, player->maxHp,
             player->energy, player->maxEnergy, player->armor);
    UIText(summary, x, y + 4, small, LIGHTGRAY);
}

// The Armorer's crafting window: recipes take gold AND Charr Hides,
// GW1's armor-is-crafted-only economy.
static void DrawCraft(Entity *player, int screenWidth, int screenHeight) {
    CraftEntry craftList[CRAFT_LIST_COUNT];
    BuildCraftList(player->primaryProfession, craftList);

    float scale = UI_Scale(screenHeight);
    int rowH = (int)(30 * scale);
    int font = (int)(12 * scale);
    int pad = (int)(10 * scale);
    int w = (int)(420 * scale);
    int h = pad * 4 + font * 2 + (CRAFT_LIST_COUNT + 1) * rowH;

    g_craftRect = (Rectangle){ (float)(screenWidth - w) / 2.0f, (float)(90 * scale), (float)w, (float)h };
    UIHit_Claim(g_craftRect);
    UI_ThemePanel(g_craftRect, scale, pad + font + pad / 2);

    int x = (int)g_craftRect.x + pad;
    int y = (int)g_craftRect.y + pad;
    char title[80];
    snprintf(title, sizeof(title), "Armor Crafting   %dg, %d %s", g_gold,
             Items_CountMaterial(CRAFT_MATERIAL), CRAFT_MATERIAL);
    if (PanelHeader(g_craftRect, title, NULL, NULL, font, pad, scale)) {
        g_craftOpen = false;
        return;
    }
    y += font + pad;

    bool click = UI_PointerClicked();
    Vector2 mouse = UI_PointerPos();

    for (int i = 0; i < CRAFT_LIST_COUNT; i++) {
        const CraftEntry *entry = &craftList[i];
        Rectangle row = { g_craftRect.x + 4, (float)y - 2, g_craftRect.width - 8, (float)rowH };
        bool canCraft = g_gold >= entry->gold &&
                        Items_CountMaterial(CRAFT_MATERIAL) >= entry->hides &&
                        g_inventoryCount < MAX_INVENTORY;
        bool focused = UIFocus_Item();
        bool hovered = CheckCollisionPointRec(mouse, row);
        if ((hovered || focused) && canCraft) DrawRectangleRec(row, (Color){ 60, 60, 80, 255 });
        if (focused) DrawRectangleLinesEx(row, 1.5f, UI_GOLD);

        char label[112];
        snprintf(label, sizeof(label), "%s  -  %dg + %d %s", entry->item.name,
                 entry->gold, entry->hides, CRAFT_MATERIAL);
        UIText(label, x, y, font, canCraft ? RAYWHITE : GRAY);

        if (((hovered && click) || (focused && UIFocus_Confirm())) && canCraft) {
            Audio_Play(SFX_UI_CONFIRM);
            if (Items_AddToInventory(entry->item)) {
                g_gold -= entry->gold;
                Items_ConsumeMaterial(CRAFT_MATERIAL, entry->hides);
            }
        }
        y += rowH;
    }

    y += 4;
    UIText("Hides come from slain Charr - armor is never looted, only crafted.",
           x, y, (int)(10 * scale), GRAY);
}

// Picking a second profession. Every profession but your primary is
// offered; taking one opens its non-primary attribute lines and its
// half of the trainer's stock.
static void DrawProfessionPanel(Entity *player, int screenWidth, int screenHeight) {
    const Entity *trainer = Entity_Get(g_dialogNpc);
    if (trainer && trainer->npcRole != NPC_PROFESSION_CHANGER) trainer = NULL;
    float scale = UI_Scale(screenHeight);
    int font = (int)(12 * scale);
    int small = (int)(10 * scale);
    int pad = (int)(10 * scale);
    int rowH = (int)(34 * scale);
    int w = (int)(440 * scale);
    int rows = trainer ? 1 : (PROF_COUNT - 1);
    int h = pad * 3 + font + small + (int)(6 * scale) + rows * rowH + pad;

    g_professionRect = (Rectangle){ (float)(screenWidth - w) / 2.0f, (float)(90 * scale),
                                    (float)w, (float)h };
    UIHit_Claim(g_professionRect);
    UI_ThemePanel(g_professionRect, scale, pad + font + pad / 2);

    if (PanelHeader(g_professionRect, "Second Profession", NULL, NULL, font, pad, scale)) {
        g_professionOpen = false;
        return;
    }

    int x = (int)g_professionRect.x + pad;
    int y = (int)g_professionRect.y + pad + font + pad;

    UIText(trainer ? "Your primary never changes. This is the other half of your build."
                   : "Your primary never changes.",
           x, y, small, (Color){ 150, 146, 134, 255 });
    y += small + (int)(6 * scale);

    bool click = UI_PointerClicked();
    Vector2 mouse = UI_PointerPos();

    for (int p = 0; p < PROF_COUNT; p++) {
        if (p == (int)player->primaryProfession) continue;
        // Pre-Searing scatters the six trainers across six areas, and
        // each one teaches only their own calling. Which secondary you
        // can take is therefore a question of where you can get to.
        if (trainer && p != (int)trainer->teachesProfession) continue;
        bool current = (g_character.secondary == p);

        Rectangle row = { g_professionRect.x + 4, (float)y - 2,
                          g_professionRect.width - 8, (float)rowH };
        bool focused = UIFocus_Item();
        bool hovered = CheckCollisionPointRec(mouse, row);
        if ((hovered || focused) && !current) DrawRectangleRec(row, (Color){ 60, 60, 80, 255 });
        if (focused) DrawRectangleLinesEx(row, 1.5f, UI_GOLD);

        char label[96];
        snprintf(label, sizeof(label), "%s / %s", Character_ProfessionAbbrev(player->primaryProfession),
                 Character_ProfessionAbbrev(p));
        UIText(label, x, y, font, current ? UI_GOLD : RAYWHITE);
        UIText(Character_ProfessionName(p), x + (int)(64 * scale), y, font,
               current ? UI_GOLD : RAYWHITE);
        UIText(Character_ProfessionBlurb(p), x + (int)(64 * scale), y + font + 2, small,
               (Color){ 150, 146, 134, 255 });

        if (current) {
            const char *tag = "current";
            int tw = UITextWidth(tag, small);
            UIText(tag, (int)(g_professionRect.x + g_professionRect.width) - pad - tw,
                   y + (font - small) / 2, small, UI_GOLD);
        }

        if (((hovered && click) || (focused && UIFocus_Confirm())) && !current) {
            // Points sunk into the OLD secondary's lines come back.
            // GW1 refunds them, and it has to: otherwise changing costs
            // you a chunk of your character with no way to earn it back.
            int refunded = 0;
            for (int a = 0; a < ATTR_COUNT; a++) {
                if (player->attributeRank[a] <= 0) continue;
                if (Attribute_Accessible((AttributeKind)a, player->primaryProfession,
                                         (Profession)p)) continue;
                refunded += g_attrCumulativeCost[player->attributeRank[a]];
                player->attributeRank[a] = 0;
            }
            player->attributePoints += refunded;

            g_character.secondary = p;
            player->secondaryProfession = (Profession)p;
            Character_FormatTitle(&g_character, player->name, sizeof(player->name));
            Entity_RecomputeAttributeStats(player);

            // A skill on the bar you can no longer use has to come off,
            // or you'd keep casting something you don't have access to.
            for (int i = 0; i < SKILL_BAR_SIZE; i++) {
                int id = player->skillBar[i];
                if (id >= 0 && !Skillbook_IsUsableBy(id, player->primaryProfession,
                                                     player->secondaryProfession)) {
                    player->skillBar[i] = -1;
                }
            }

            Audio_Play(SFX_UI_CONFIRM);
            char msg[96];
            snprintf(msg, sizeof(msg), "You are now a %.20s / %.20s",
                     Character_ProfessionName(player->primaryProfession),
                     Character_ProfessionName(p));
            UI_Notify(msg);
            if (refunded > 0) UI_Notify("Attribute points refunded.");
            Save_Write();
            g_professionOpen = false;
            return;
        }
        y += rowH;
    }
}

static void DrawNpcDialog(Entity *player, int screenWidth, int screenHeight) {
    Entity *npc = Entity_Get(g_dialogNpc);
    if (!npc || npc->kind != ENT_NPC) { g_dialogNpc = -1; g_shopOpen = false; return; }

    // Walking away closes the conversation, like GW1.
    float dx = npc->pos.x - player->pos.x, dy = npc->pos.y - player->pos.y;
    if (sqrtf(dx * dx + dy * dy) > DIALOG_WALKAWAY_DISTANCE) {
        UI_CloseNpcDialog();
        return;
    }

    float scale = UI_Scale(screenHeight);
    int font = (int)(13 * scale);
    int pad = (int)(12 * scale);
    int btnH = (int)(30 * scale);
    int w = (int)(360 * scale);
    int h = pad * 3 + font * 2 + btnH + 8;

    g_dialogRect = (Rectangle){ (float)(screenWidth - w) / 2.0f,
                                (float)screenHeight - (float)(220 * scale), (float)w, (float)h };
    UIHit_Claim(g_dialogRect);
    UI_ThemePanel(g_dialogRect, scale, pad + font + 6);

    int x = (int)g_dialogRect.x + pad;
    int y = (int)g_dialogRect.y + pad;
    UIText(npc->name, x, y, font, (Color){ 130, 220, 130, 255 });
    y += font + 6;

    Rectangle btn = { (float)x, (float)y + font + 6, g_dialogRect.width - 2 * pad, (float)btnH };

    switch (npc->npcRole) {
        case NPC_QUEST_GIVER: {
            // Each giver only deals in their own quest book - Osric's
            // work stays in Ashford, Grast's on the Piken frontier.
            int ready = Quests_ReadyToTurnInIndexFor(npc->name);
            int offer = Quests_OfferableIndexFor(npc->name);
            if (ready >= 0) {
                Quest *q = &g_quests[ready];
                bool bagFull = q->rewardItem && g_inventoryCount >= MAX_INVENTORY;
                UIText(bagFull ? "Your bags are full - make room for your reward."
                               : "You've done it! Ascalon thanks you.", x, y, font, LIGHTGRAY);
                char rewards[96];
                QuestRewardSummary(q, player, rewards, sizeof(rewards));
                char label[224];
                snprintf(label, sizeof(label), "Turn in: %.63s (%.95s)", q->name, rewards);
                if (DialogButton(btn, label, font, !bagFull)) {
                    Quests_TurnIn(player, ready);
                }
            } else if (offer >= 0) {
                Quest *q = &g_quests[offer];
                UIText(q->offerText, x, y, font, LIGHTGRAY);
                char rewards[96];
                QuestRewardSummary(q, player, rewards, sizeof(rewards));
                char label[224];
                snprintf(label, sizeof(label), "Accept: %.63s (%.95s)", q->name, rewards);
                if (DialogButton(btn, label, font, true)) {
                    Quests_Accept(offer);
                }
            } else {
                bool anyActive = false;
                for (int i = 0; i < QUEST_COUNT; i++) {
                    if (g_quests[i].state == QUEST_ACTIVE &&
                        strcmp(g_quests[i].giverName, npc->name) == 0) anyActive = true;
                }
                UIText(anyActive ? "Your task awaits. Good hunting."
                                 : "Nothing more for now, friend.", x, y, font, LIGHTGRAY);
            }
            break;
        }
        case NPC_MERCHANT: {
            UIText("Weapons - fair prices, no haggling. Armor? See Dunda.", x, y, font, LIGHTGRAY);
            if (DialogButton(btn, g_shopOpen ? "Close shop" : "Browse wares", font, true)) {
                g_shopOpen = !g_shopOpen;
                UIFocus_Clear();
            }
            break;
        }
        case NPC_CRAFTER: {
            UIText("Bring me Charr hides and coin - I'll fit you properly.", x, y, font, LIGHTGRAY);
            if (DialogButton(btn, g_craftOpen ? "Close crafting" : "Craft armor", font, true)) {
                g_craftOpen = !g_craftOpen;
                UIFocus_Clear();
            }
            break;
        }
        case NPC_SKILL_TRAINER: {
            UIText("Skills are earned, not given. Points and coin, and I'll teach.",
                   x, y, font, LIGHTGRAY);
            if (DialogButton(btn, g_trainerOpen ? "Close training" : "Learn skills", font, true)) {
                g_trainerOpen = !g_trainerOpen;
                UIFocus_Clear();
            }
            break;
        }
        case NPC_PROFESSION_CHANGER: {
            bool hasSecondary = (g_character.secondary != PROF_NONE);
            // Two different gates depending on which half of the pacing
            // this NPC is. Whichever one you don't meet is said out
            // loud, with the requirement named - an NPC that just
            // refuses teaches the player nothing.
            if (!hasSecondary) {
                if (SecondaryGrantAllowed()) {
                    UIText("You've bled for Ascalon. Choose a second calling.",
                           x, y, font, LIGHTGRAY);
                    if (DialogButton(btn, "Choose a second profession", font, true)) {
                        g_professionOpen = !g_professionOpen;
                        UIFocus_Clear();
                    }
                } else {
                    UIText("Prove yourself first. Osric has work - finish it.",
                           x, y, font, LIGHTGRAY);
                    DialogButton(btn, "Requires: " SECONDARY_QUEST, font, false);
                }
            } else if (SecondaryChangeAllowed(player)) {
                UIText("Second thoughts? I can unmake the choice.", x, y, font, LIGHTGRAY);
                if (DialogButton(btn, g_professionOpen ? "Never mind" : "Change my second profession",
                                 font, true)) {
                    g_professionOpen = !g_professionOpen;
                    UIFocus_Clear();
                }
            } else {
                char why[96];
                snprintf(why, sizeof(why), "Requires: level %d (you are %d)",
                         SECONDARY_CHANGE_LEVEL, player->level);
                UIText("A calling isn't a coat. Live with it a while longer.",
                       x, y, font, LIGHTGRAY);
                DialogButton(btn, why, font, false);
            }
            break;
        }
        case NPC_HENCHMAN: {
            UIText("Need another axe... er, sword? I work for loot shares.", x, y, font, LIGHTGRAY);
            if (DialogButton(btn, "Hire Little Thom (free)", font, true)) {
                World_SetThomHired(true);
                // Convert the standing NPC into a fighting party member on
                // the spot; zone loads keep him from then on. Stats come
                // from the same setup zone loads use, so the two copies
                // of his stat block can't drift.
                npc->kind = ENT_HERO;
                npc->npcRole = NPC_NONE;
                World_SetupThomStats(npc);
                g_dialogNpc = -1;
            }
            break;
        }
        default:
            break;
    }
}

void UI_PanelsUpdateAndDraw(int screenWidth, int screenHeight) {
    Entity *player = Entity_Get(PLAYER_INDEX);
    if (!player) return;

    if (IsKeyPressed(KEY_I)) g_invOpen = !g_invOpen;
    if (IsKeyPressed(KEY_K)) g_attrOpen = !g_attrOpen;
    if (IsKeyPressed(KEY_E)) g_equipOpen = !g_equipOpen;
    if (IsKeyPressed(KEY_L)) {
        g_skillsOpen = !g_skillsOpen;
        g_armedBarSlot = -1;
    }
    // Focus navigation covers the NPC dialog and the windows it opens -
    // the parts of the game that are a list of choices. Only run while
    // one is up, or the arrow keys would be swallowed during play.
    bool focusList = (g_dialogNpc >= 0);
    if (focusList) UIFocus_Begin();

    if (g_invOpen) DrawInventory(player, screenHeight);
    if (g_equipOpen) DrawEquipment(player, screenHeight);
    if (g_attrOpen) DrawAttributes(player, screenWidth, screenHeight);
    if (g_skillsOpen) DrawSkillsPanel(player, screenWidth, screenHeight);
    if (g_dialogNpc >= 0) DrawNpcDialog(player, screenWidth, screenHeight);
    if (g_shopOpen && g_dialogNpc >= 0) DrawShop(screenWidth, screenHeight);
    if (g_craftOpen && g_dialogNpc >= 0) DrawCraft(player, screenWidth, screenHeight);
    if (g_trainerOpen && g_dialogNpc >= 0) DrawTrainer(player, screenWidth, screenHeight);
    if (g_professionOpen && g_dialogNpc >= 0) DrawProfessionPanel(player, screenWidth, screenHeight);

    if (focusList) {
        UIFocus_End();
        // Back closes whatever is deepest, so a pad can always retreat
        // without needing the mouse to find a close box.
        if (UIFocus_Cancel()) {
            if (g_shopOpen || g_craftOpen || g_trainerOpen || g_professionOpen) {
                g_shopOpen = g_craftOpen = g_trainerOpen = g_professionOpen = false;
                UIFocus_Clear();
                Audio_Play(SFX_UI_CLOSE);
            } else {
                UI_CloseNpcDialog();
            }
        }
    }
}
