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
#include "ui_tooltip.h"
#include "titles.h"
#include "builds.h"
#include "collectors.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

#define PLAYER_INDEX 0
#define DIALOG_WALKAWAY_DISTANCE 130.0f

static bool g_invOpen = false;
static bool g_skillsOpen = false;
static int g_dialogNpc = -1;   // entity index, -1 = closed
// Which offered quest the quest-giver dialogue is reading in detail, or
// -1 for the list of quest titles. GW1 shows a quest's description on its
// own page with Accept/Decline before you take it.
static int g_dialogQuest = -1;
static bool g_shopOpen = false;
static bool g_craftOpen = false;
static bool g_trainerOpen = false;
static bool g_professionOpen = false;
static bool g_titlesOpen = false;
static bool g_collectorOpen = false;

static Rectangle g_invRect, g_dialogRect, g_shopRect,
                 g_craftRect, g_skillsRect, g_trainerRect, g_professionRect,
                 g_titlesRect, g_collectorRect;

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

// Which template slot's name is being typed into, -1 = none. While one
// is armed the panel hotkeys are suppressed, or typing "L" would close
// the window you are typing in.
static int g_editingBuild = -1;

// The merchant's stock: fixed-power items at fixed prices, in GW1's
// spirit where the merchant sells the basics and rarity is cosmetic.
typedef struct { Item item; int price; } ShopEntry;
// Designated initialisers, because Item grew a slot and a two-handed
// flag: a positional literal would silently mean something else.
static const ShopEntry g_shopStock[] = {
    { { .kind = ITEM_WEAPON, .name = "Long Sword", .dmgMin = 15, .dmgMax = 22,
        .range = 28.0f, .attackInterval = 1.33f, .count = 1 }, 80 },
    { { .kind = ITEM_WEAPON, .name = "War Hammer", .dmgMin = 19, .dmgMax = 35,
        .range = 30.0f, .attackInterval = 1.75f, .count = 1, .twoHanded = true }, 120 },
    { { .kind = ITEM_WEAPON, .name = "Fire Staff", .dmgMin = 11, .dmgMax = 22,
        .range = 220.0f, .attackInterval = 1.75f, .count = 1, .twoHanded = true }, 100 },
    // An offhand and a bag, so the two new slots have somewhere to come
    // from without waiting on a drop table.
    { { .kind = ITEM_OFFHAND, .name = "Ascalon Shield", .armor = 8, .count = 1 }, 140 },
    { { .kind = ITEM_OFFHAND, .name = "Bone Focus", .armor = 3, .count = 1 }, 90 },
    { { .kind = ITEM_BAG, .name = "Belt Pouch", .count = 5 }, 100 },
    { { .kind = ITEM_BAG, .name = "Large Bag", .count = 10 }, 250 },
    // GW1's two workhorse kits, at GW1's merchant price point.
    { { .kind = ITEM_KIT_SALVAGE, .name = "Salvage Kit", .count = 25 }, 100 },
    { { .kind = ITEM_KIT_ID, .name = "Identification Kit", .count = 25 }, 100 },
};
#define SHOP_STOCK_COUNT (int)(sizeof(g_shopStock) / sizeof(g_shopStock[0]))

// The Armorer's recipes: gold + Charr Carvings in, armor out - GW1's
// craft-only armor economy. No armor ever drops or sits in a shop.
typedef struct { Item item; int gold; int hides; } CraftEntry;
#define CRAFT_LIST_COUNT 5   // one pre-Searing set, five pieces
#define CRAFT_MATERIAL "Charr Carving"

// The pre-Searing "Ascalon armor" set each profession crafts. These are
// GW1's actual pre-Searing set names; the post-Searing sets (and their
// far higher armour) come later.
static const char *ArmorSetName(Profession p) {
    switch (p) {
        case PROF_WARRIOR:      return "Ringmail";
        case PROF_RANGER:       return "Rawhide";
        case PROF_MONK:         return "Roughspun";
        case PROF_NECROMANCER:  return "Initiate's";
        case PROF_MESMER:       return "Dilettante's";
        case PROF_ELEMENTALIST: return "Apprentice's";
        default:                return "Ascalon";
    }
}

// Pre-Searing armour is deliberately low. A warrior's Ringmail sits
// around AL 40, a ranger's Rawhide near 30, and every caster's cloth
// lower still - nothing close to the AL 60/70/80 ceilings that only open
// up after the Searing.
static int ArmorRatingFor(Profession p) {
    switch (p) {
        case PROF_WARRIOR: return 40;
        case PROF_RANGER:  return 30;
        default:           return 24; // every caster
    }
}

// GW1's core armour-piece nouns, split martial vs cloth so a warrior gets
// a Hauberk and Gauntlets while a caster gets a Robe and Gloves.
static const char *ArmorPieceNoun(Profession p, EquipSlot slot) {
    bool warrior = (p == PROF_WARRIOR);
    bool ranger  = (p == PROF_RANGER);
    switch (slot) {
        case EQUIP_HEAD:  return warrior ? "Helm"      : ranger ? "Mask"  : "Cowl";
        case EQUIP_CHEST: return warrior ? "Hauberk"   : ranger ? "Vest"  : "Robe";
        case EQUIP_ARMS:  return warrior ? "Gauntlets" : "Gloves";
        case EQUIP_LEGS:  return "Leggings";
        case EQUIP_FEET:  return ranger || warrior ? "Boots" : "Shoes";
        default:          return "Piece";
    }
}

// One set, five pieces. GW1's armourer sells a SET a piece at a time,
// which is the whole reason armour is a collection: you buy the chest
// first because it protects the most, and finish the set when you can.
static void BuildCraftList(Profession p, CraftEntry out[CRAFT_LIST_COUNT]) {
    // The chest costs more and armours the most; the extremities are cheap.
    static const int pieceGold[5]  = { 25, 45, 20, 30, 20 };
    static const int pieceHides[5] = { 1, 2, 1, 1, 1 };
    int al = ArmorRatingFor(p);
    const char *set = ArmorSetName(p);
    int n = 0;
    for (int piece = EQUIP_HEAD; piece <= EQUIP_FEET && n < CRAFT_LIST_COUNT; piece++, n++) {
        memset(&out[n], 0, sizeof(out[n]));
        out[n].item.kind = ITEM_ARMOR;
        out[n].item.armor = al;
        out[n].item.count = 1;
        out[n].item.slot = (EquipSlot)piece;
        snprintf(out[n].item.name, sizeof(out[n].item.name), "%.12s %s (AL %d)",
                 set, ArmorPieceNoun(p, (EquipSlot)piece), al);
        out[n].gold = pieceGold[n];
        out[n].hides = pieceHides[n];
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
    g_dialogQuest = -1; // always open on the list of quests, not a detail
    g_shopOpen = false;
    g_craftOpen = false;
    // GW1 opens a merchant's trade window and an armourer's craft window
    // the moment you talk to them - the wares are the conversation, so
    // there's no "Browse wares" step in between. Closing the window drops
    // back to the one-line dialogue behind it.
    const Entity *npc = Entity_Get(entityIndex);
    if (npc && npc->kind == ENT_NPC) {
        if (npc->npcRole == NPC_MERCHANT) g_shopOpen = true;
        else if (npc->npcRole == NPC_CRAFTER) g_craftOpen = true;
    }
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
    g_collectorOpen = false;
}

bool UI_IsInventoryOpen(void) { return g_invOpen; }
// Attributes live on the build screen now; the old separate window is
// gone, so "are attributes open" is the same question as "is the build
// screen open". Kept as its own name because input.c asks it while
// deciding whether a menu owns the pad.
bool UI_IsAttributesOpen(void) { return g_skillsOpen; }
// Gear and bags are one screen now, so "is equipment open" and "is
// inventory open" are the same question. Both names survive because
// input.c and the pause hub each ask in their own words.
bool UI_IsEquipmentOpen(void) { return g_invOpen; }
bool UI_IsSkillsOpen(void) { return g_skillsOpen; }
bool UI_IsTitlesOpen(void) { return g_titlesOpen; }

// The pad's route to the build editor - see UI_ToggleBags for the same
// idea applied to inventory and gear.
void UI_ToggleSkills(void) {
    g_skillsOpen = !g_skillsOpen;
    g_armedBarSlot = -1;
}

void UI_OpenPanel(PanelId panel) {
    switch (panel) {
        case PANEL_SKILLS:     g_skillsOpen = true; g_armedBarSlot = -1; break;
        case PANEL_EQUIPMENT:  g_invOpen = true; break; // merged into the character screen
        case PANEL_INVENTORY:  g_invOpen = true; break;
        case PANEL_ATTRIBUTES: g_skillsOpen = true; break; // merged into the build screen
        case PANEL_TITLES:     g_titlesOpen = true; break;
    }
}

// The pad's Y button opens "the bags": inventory + equipment together,
// since on a controller you almost always want both at once.
void UI_ToggleBags(void) {
    g_invOpen = !g_invOpen;
}

void UI_ClosePanels(void) {
    g_invOpen = false;
    g_skillsOpen = false;
    g_titlesOpen = false;
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

// ---------------------------------------------------------------------
// The character screen: what you wear and what you carry, together.
//
// GW1 dresses you in five armour pieces plus a weapon and an offhand,
// and armour is a collection game precisely because you upgrade a set a
// piece at a time. Splitting "equipment" and "inventory" into two
// windows made that invisible - you could never see the hole in a set
// next to the piece in your bag that would fill it.
static void DrawInventory(Entity *player, int screenHeight) {
    float scale = UI_Scale(screenHeight);
    int screenWidth = GetScreenWidth();
    int font = (int)(12 * scale);
    int small = (int)(10 * scale);
    int pad = (int)(10 * scale);
    int slotH = (int)(28 * scale);
    int cell = (int)(38 * scale);
    int cellGap = (int)(4 * scale);

    int cols = 5;
    int capacity = Items_Capacity();
    int rows = (capacity + cols - 1) / cols;

    int equipW = (int)(250 * scale);
    int gridW = cols * cell + (cols - 1) * cellGap;
    int w = pad * 3 + equipW + gridW;

    int equipH = small + (int)(4 * scale) + EQUIP_SLOT_COUNT * slotH;
    int gridH = small + (int)(4 * scale) + rows * (cell + cellGap);
    int bodyH = equipH > gridH ? equipH : gridH;
    int h = pad * 2 + font + pad + bodyH + (int)(8 * scale) + font + pad;

    g_invRect = (Rectangle){ (float)(screenWidth - w) / 2.0f, (float)(100 * scale),
                             (float)w, (float)h };
    UIHit_Claim(g_invRect);
    UI_ThemePanel(g_invRect, scale, pad + font + pad / 2);

    char title[96];
    snprintf(title, sizeof(title), "Character   AL %d   %d/%d carried   %d gold",
             player->armor, g_inventoryCount, capacity, g_gold);
    if (PanelHeader(g_invRect, title, "I", NULL, font, pad, scale)) {
        g_invOpen = false;
        g_armedKit = -1;
        return;
    }

    bool click = UI_PointerClicked();
    Vector2 mouse = UI_PointerPos();

    int x = (int)g_invRect.x + pad;
    int y = (int)g_invRect.y + pad + font + pad;

    // --- Left: the paper doll ---
    UIText("EQUIPPED", x, y, small, UI_GOLD_DIM);
    int ey = y + small + (int)(4 * scale);

    for (int slot = 0; slot < EQUIP_SLOT_COUNT; slot++) {
        Rectangle row = { (float)x, (float)ey, (float)equipW, (float)(slotH - 3) };
        bool hovered = CheckCollisionPointRec(mouse, row);
        int idx = g_equipped[slot];
        bool filled = (idx >= 0 && idx < g_inventoryCount);

        DrawRectangleRec(row, hovered ? UI_SURFACE_HOVER : UI_SURFACE_RAISE);
        DrawRectangleLinesEx(row, 1.0f, filled ? UI_GOLD_DIM : (Color){ 54, 52, 48, 255 });

        UIText(Items_SlotName((EquipSlot)slot), (int)row.x + (int)(6 * scale),
               (int)row.y + (slotH - 3 - small) / 2, small, UI_TEXT_MUTED);

        char label[64];
        if (filled) {
            const Item *it = &g_inventory[idx];
            if (it->kind == ITEM_ARMOR)        snprintf(label, sizeof(label), "%.24s", it->name);
            else if (it->kind == ITEM_OFFHAND) snprintf(label, sizeof(label), "%.20s +%d AL", it->name, it->armor);
            else if (it->kind == ITEM_BAG)     snprintf(label, sizeof(label), "%.18s +%d slots", it->name, it->count);
            else                               snprintf(label, sizeof(label), "%.24s", Items_DisplayName(it));
        } else {
            // A two-hander is the reason the offhand is empty, and
            // saying so beats an empty box the player thinks is a bug.
            int wep = g_equipped[EQUIP_WEAPON];
            bool blocked = slot == EQUIP_OFFHAND && wep >= 0 && wep < g_inventoryCount &&
                           g_inventory[wep].twoHanded;
            snprintf(label, sizeof(label), "%s", blocked ? "- both hands used -" : "-");
        }
        int lw = UITextWidth(label, small);
        Color labelCol = UI_TEXT_MUTED;
        if (filled) {
            const Item *it = &g_inventory[idx];
            labelCol = (it->kind == ITEM_WEAPON) ? Items_RarityColor(it) : UI_TEXT_PRIMARY;
        }
        UIText(label, (int)(row.x + row.width) - lw - (int)(6 * scale),
               (int)row.y + (slotH - 3 - small) / 2, small, labelCol);

        if (hovered && click && filled) {
            Items_Unequip(player, (EquipSlot)slot);
            Audio_Play(SFX_UI_CLICK);
            Save_Write();
        }
        ey += slotH;
    }

    // --- Right: the bag grid ---
    int gx = x + equipW + pad;
    UIText("BAGS", gx, y, small, UI_GOLD_DIM);
    int gy = y + small + (int)(4 * scale);

    for (int i = 0; i < capacity; i++) {
        int col = i % cols, row = i / cols;
        Rectangle r = { (float)(gx + col * (cell + cellGap)),
                        (float)(gy + row * (cell + cellGap)),
                        (float)cell, (float)cell };
        bool has = i < g_inventoryCount;
        bool hovered = CheckCollisionPointRec(mouse, r);

        DrawRectangleRec(r, has ? (Color){ 34, 36, 46, 255 } : (Color){ 22, 23, 29, 255 });
        DrawRectangleLinesEx(r, 1.0f, (Color){ 52, 50, 46, 255 });
        if (!has) { continue; }

        const Item *it = &g_inventory[i];
        bool worn = Items_IsEquipped(i);
        bool armed = (g_armedKit == i);

        // A letter tile rather than an icon: the item art doesn't exist,
        // and a consistent glyph per kind still scans at a glance.
        const char *glyph = it->kind == ITEM_WEAPON ? "W"
                          : it->kind == ITEM_ARMOR ? "A"
                          : it->kind == ITEM_OFFHAND ? "O"
                          : it->kind == ITEM_BAG ? "B"
                          : it->kind == ITEM_MATERIAL ? "M" : "K";
        Color tile = it->kind == ITEM_WEAPON ? (Color){ 120, 92, 60, 255 }
                   : it->kind == ITEM_ARMOR ? (Color){ 78, 96, 120, 255 }
                   : it->kind == ITEM_OFFHAND ? (Color){ 96, 84, 124, 255 }
                   : it->kind == ITEM_BAG ? (Color){ 104, 86, 56, 255 }
                   : it->kind == ITEM_MATERIAL ? (Color){ 88, 106, 76, 255 }
                                               : (Color){ 100, 100, 108, 255 };
        if (it->unidentified) tile = (Color){ 82, 70, 96, 255 };
        DrawRectangleRec((Rectangle){ r.x + 3, r.y + 3, r.width - 6, r.height - 6 }, tile);
        UI_TextShadowCentered(glyph, (int)(r.x + r.width / 2), (int)(r.y + (cell - font) / 2 - 3),
                              font, RAYWHITE);

        if (it->count > 1) {
            char cnt[16];
            snprintf(cnt, sizeof(cnt), "%d", it->count);
            int cw = UITextWidth(cnt, small);
            UI_TextShadow(cnt, (int)(r.x + r.width) - cw - 3, (int)(r.y + r.height) - small - 2,
                          small, (Color){ 236, 230, 214, 255 });
        }
        if (worn) DrawRectangleLinesEx(r, 2.0f, UI_GOLD);
        if (armed) DrawRectangleLinesEx(r, 2.0f, SKYBLUE);
        if (hovered) DrawRectangleLinesEx(r, 2.0f, (Color){ 210, 200, 176, 200 });

        // Hover name, drawn under the grid so it can't cover a cell.
        if (hovered) {
            char info[80];
            if (it->kind == ITEM_WEAPON && !it->unidentified) {
                snprintf(info, sizeof(info), "%s  %d-%d%s", it->name, it->dmgMin, it->dmgMax,
                         it->twoHanded ? "  (two-handed)" : "");
            } else if (it->kind == ITEM_ARMOR) {
                snprintf(info, sizeof(info), "%s  %s  AL %d", it->name,
                         Items_SlotName(it->slot), it->armor);
            } else {
                snprintf(info, sizeof(info), "%s", Items_DisplayName(it));
            }
            Color nameCol = (it->kind == ITEM_WEAPON && !it->unidentified)
                            ? Items_RarityColor(it) : UI_TEXT_PRIMARY;
            UIText(info, x, (int)(g_invRect.y + g_invRect.height) - pad - font, font, nameCol);
        }

        if (hovered && click) {
            if (g_armedKit >= 0 && g_armedKit != i) {
                int next = Items_UseKitOn(g_armedKit, i);
                g_armedKit = (next >= 0) ? next : -1;
                Audio_Play(next == -2 ? SFX_UI_DENY : SFX_UI_CONFIRM);
                // A rune slotted into a worn piece has to take effect now.
                Items_RecomputeEquipped(player);
                Save_Write();
            } else if (it->kind == ITEM_KIT_ID || it->kind == ITEM_KIT_SALVAGE ||
                       it->kind == ITEM_RUNE || it->kind == ITEM_INSIGNIA) {
                g_armedKit = armed ? -1 : i;
                Audio_Play(SFX_UI_CLICK);
            } else if (it->kind == ITEM_DYE) {
                // Dye paints the whole outfit and is spent doing so.
                player->dyeColor = it->dyeColor;
                player->dyed = true;
                Items_RemoveFromInventory(i);
                Audio_Play(SFX_UI_CONFIRM);
                Save_Write();
            } else if (worn) {
                for (int s2 = 0; s2 < EQUIP_SLOT_COUNT; s2++) {
                    if (g_equipped[s2] == i) Items_Unequip(player, (EquipSlot)s2);
                }
                Audio_Play(SFX_UI_CLICK);
                Save_Write();
            } else {
                bool ok = Items_Equip(player, i);
                Audio_Play(ok ? SFX_UI_CONFIRM : SFX_UI_DENY);
                if (ok) Save_Write();
            }
        }
    }

    // Footer hint, in the strip the hover name shares.
    if (!CheckCollisionPointRec(mouse, g_invRect) || g_armedKit >= 0) {
        const char *hint = g_armedKit >= 0
            ? "Kit armed - click the item to use it on."
            : "Click a bag slot to equip; click an equipped row to take it off.";
        UIText(hint, x, (int)(g_invRect.y + g_invRect.height) - pad - font, font,
               g_armedKit >= 0 ? SKYBLUE : UI_TEXT_MUTED);
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
    int attrRowH = (int)(26 * scale);
    int tmplRowH = (int)(26 * scale);

    bool canEdit = (World_GetMode() == MODE_OUTPOST);

    // Count both columns so the window hugs the taller of them.
    int known = 0;
    for (int i = 0; i < g_skillCount; i++) {
        if (Skillbook_IsUnlocked(i) &&
            Skillbook_IsUsableBy(i, player->primaryProfession, player->secondaryProfession)) known++;
    }
    int accessible = 0;
    for (int a = 0; a < ATTR_COUNT; a++) {
        if (Attribute_Accessible((AttributeKind)a, player->primaryProfession,
                                 player->secondaryProfession)) accessible++;
    }
    int listRows = known > 0 ? known : 1;

    int barW = SKILL_BAR_SIZE * slot + (SKILL_BAR_SIZE - 1) * gap;
    int w = (int)(740 * scale);
    if (w < barW + pad * 2) w = barW + pad * 2;

    int attrColW = (int)(300 * scale);
    int skillColW = w - attrColW - pad * 3;

    int colRows = (accessible > listRows) ? accessible : listRows;
    int columnsH = small + (int)(4 * scale) + colRows * rowH;
    int templatesH = small + (int)(4 * scale) + BUILD_SLOT_COUNT * tmplRowH;
    int h = pad * 2 + font + pad + slot + (int)(8 * scale) + small + (int)(8 * scale)
          + columnsH + (int)(10 * scale) + templatesH + pad;

    g_skillsRect = (Rectangle){ (float)(screenWidth - w) / 2.0f, (float)(70 * scale),
                                (float)w, (float)h };
    UIHit_Claim(g_skillsRect);
    UI_ThemePanel(g_skillsRect, scale, pad + font + pad / 2);

    char title[112];
    snprintf(title, sizeof(title), "Skills & Attributes   %d known   %d skill point%s   %d attribute point%s",
             Skillbook_UnlockedCount(), g_skillPoints, g_skillPoints == 1 ? "" : "s",
             player->attributePoints, player->attributePoints == 1 ? "" : "s");
    if (PanelHeader(g_skillsRect, title, "L", NULL, font, pad, scale)) {
        g_skillsOpen = false;
        g_armedBarSlot = -1;
        g_editingBuild = -1;
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

        if (id >= 0 && CheckCollisionPointRec(mouse, r)) UITooltip_Request(id, r);

        if (hovered && click) {
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
        hint = "Skills and attributes can only be changed in an outpost.";
        hintColor = (Color){ 214, 150, 128, 255 };
    } else if (g_armedBarSlot >= 0) {
        hint = "Pick a skill on the right, or click the slot again to clear it.";
        hintColor = UI_GOLD;
    } else {
        hint = "Click a slot, then a skill, to build your bar.";
        hintColor = (Color){ 150, 146, 134, 255 };
    }
    UIText(hint, x, y, small, hintColor);
    y += small + (int)(8 * scale);

    int colTop = y;
    int attrX = x;
    int skillX = x + attrColW + pad;

    // --- Left column: attributes ---
    // On the same screen as the bar because they are one decision. GW1
    // puts them together for the same reason: a skill's numbers come
    // from a rank, and choosing either in isolation is guesswork.
    UIText("ATTRIBUTES", attrX, y, small, UI_GOLD_DIM);
    int ay = y + small + (int)(4 * scale);
    int btn = (int)(20 * scale);

    for (int a = 0; a < ATTR_COUNT; a++) {
        if (!Attribute_Accessible((AttributeKind)a, player->primaryProfession,
                                  player->secondaryProfession)) continue;

        int rank = player->attributeRank[a];
        char label[64];
        snprintf(label, sizeof(label), "%s", g_attributeNames[a]);
        UIText(label, attrX, ay, font, g_attributeIsPrimary[a] ? UI_GOLD : RAYWHITE);

        Rectangle plus  = { (float)(attrX + attrColW - btn), (float)ay - 2, (float)btn, (float)btn };
        Rectangle minus = { plus.x - btn - 6, (float)ay - 2, (float)btn, (float)btn };

        int upCost = (rank < ATTRIBUTE_RANK_CAP)
            ? g_attrCumulativeCost[rank + 1] - g_attrCumulativeCost[rank] : 0;
        bool canUp = canEdit && rank < ATTRIBUTE_RANK_CAP && player->attributePoints >= upCost;
        bool canDown = canEdit && rank > 0;

        // Rank, then the cost of the next one - the two numbers you need
        // to decide, side by side.
        char rankStr[24];
        if (rank < ATTRIBUTE_RANK_CAP) snprintf(rankStr, sizeof(rankStr), "%d   (+%d)", rank, upCost);
        else snprintf(rankStr, sizeof(rankStr), "%d   max", rank);
        int rw = UITextWidth(rankStr, small);
        UIText(rankStr, (int)minus.x - rw - (int)(8 * scale), ay + (font - small) / 2, small,
               rank > 0 ? UI_TEXT_SECOND : UI_TEXT_MUTED);

        if (click && !canUp && CheckCollisionPointRec(mouse, plus)) Audio_Play(SFX_UI_DENY);
        DrawRectangleRec(plus, canUp ? (Color){ 60, 100, 60, 255 } : (Color){ 45, 45, 45, 255 });
        DrawRectangleLinesEx(plus, 1, LIGHTGRAY);
        UIText("+", (int)plus.x + btn / 3, (int)plus.y + 2, font, RAYWHITE);

        DrawRectangleRec(minus, canDown ? (Color){ 100, 60, 60, 255 } : (Color){ 45, 45, 45, 255 });
        DrawRectangleLinesEx(minus, 1, LIGHTGRAY);
        UIText("-", (int)minus.x + btn / 3, (int)minus.y + 2, font, RAYWHITE);

        if (click && canUp && CheckCollisionPointRec(mouse, plus)) {
            Audio_Play(SFX_UI_CLICK);
            player->attributeRank[a]++;
            player->attributePoints -= upCost;
            Entity_RecomputeAttributeStats(player);
            Save_Write();
        }
        if (click && canDown && CheckCollisionPointRec(mouse, minus)) {
            Audio_Play(SFX_UI_CLICK);
            int refund = g_attrCumulativeCost[rank] - g_attrCumulativeCost[rank - 1];
            player->attributeRank[a]--;
            player->attributePoints += refund;
            Entity_RecomputeAttributeStats(player);
            Save_Write();
        }
        ay += attrRowH;
    }

    // --- Right column: everything you've learned ---
    UIText("SKILLS", skillX, colTop, small, UI_GOLD_DIM);
    int sy = colTop + small + (int)(4 * scale);

    if (Skillbook_UnlockedCount() == 0) {
        UIText("(none yet - try a quest giver)", skillX, sy, font, GRAY);
    }

    for (int i = 0; i < g_skillCount; i++) {
        if (!Skillbook_IsUnlocked(i)) continue;
        if (!Skillbook_IsUsableBy(i, player->primaryProfession, player->secondaryProfession)) continue;

        const Skill *s = &g_skillDB[i];

        int onBar = -1;
        for (int b = 0; b < SKILL_BAR_SIZE; b++) {
            if (player->skillBar[b] == i) onBar = b;
        }

        Rectangle row = { (float)skillX - 4, (float)sy - 2, (float)skillColW + 8, (float)rowH };
        bool selectable = canEdit && g_armedBarSlot >= 0 && onBar < 0;
        bool hovered = CheckCollisionPointRec(mouse, row);
        if (hovered && selectable) DrawRectangleRec(row, (Color){ 60, 60, 80, 255 });

        Rectangle icon = { (float)skillX, (float)sy - 1, (float)(rowH - 4), (float)(rowH - 4) };
        UI_DrawSkillIcon(i, icon);

        char label[96];
        snprintf(label, sizeof(label), "%.31s", s->name);
        Color c = s->isElite ? UI_GOLD : (onBar >= 0 ? SKYBLUE : RAYWHITE);
        UIText(label, skillX + (int)icon.width + (int)(8 * scale), sy, font, c);

        char right[40];
        if (onBar >= 0) snprintf(right, sizeof(right), "slot %d", onBar + 1);
        else snprintf(right, sizeof(right), "%.20s", g_attributeNames[s->attribute]);
        int rw = UITextWidth(right, small);
        UIText(right, skillX + skillColW - rw, sy + (font - small) / 2, small,
               onBar >= 0 ? SKYBLUE : (Color){ 150, 146, 134, 255 });

        if (hovered) UITooltip_Request(i, row);

        if (hovered && click && selectable) {
            player->skillBar[g_armedBarSlot] = i;
            player->skillRecharge[g_armedBarSlot] = 0.0f;
            g_armedBarSlot = -1;
            Save_Write();
        }
        sy += rowH;
    }

    // --- Templates: GW1's save/load for a whole build ---
    // A bar and an attribute spread are one thing, so they are saved as
    // one thing. Loading half a build is worse than loading none.
    int ty = colTop + columnsH + (int)(10 * scale);
    DrawLineEx((Vector2){ g_skillsRect.x + pad, (float)ty - (int)(5 * scale) },
               (Vector2){ g_skillsRect.x + g_skillsRect.width - pad, (float)ty - (int)(5 * scale) },
               1.0f, UI_GOLD_DIM);
    UIText("TEMPLATES", x, ty, small, UI_GOLD_DIM);
    ty += small + (int)(4 * scale);

    int nameW = (int)(240 * scale);
    int actW = (int)(60 * scale);
    for (int i = 0; i < BUILD_SLOT_COUNT; i++) {
        BuildTemplate *b = Builds_Slot(i);
        bool editing = (g_editingBuild == i);

        Rectangle nameBox = { (float)x, (float)ty, (float)nameW, (float)(tmplRowH - 4) };
        bool nameHover = CheckCollisionPointRec(mouse, nameBox);
        DrawRectangleRec(nameBox, editing ? (Color){ 46, 50, 68, 255 }
                                : nameHover ? UI_SURFACE_HOVER : UI_SURFACE_RAISE);
        DrawRectangleLinesEx(nameBox, 1.0f, editing ? UI_GOLD : UI_GOLD_DIM);

        char shown[BUILD_NAME_LEN + 8];
        if (b->name[0]) snprintf(shown, sizeof(shown), "%s%s", b->name, editing ? "_" : "");
        else snprintf(shown, sizeof(shown), "%s", editing ? "_" : (b->used ? "(unnamed)" : "empty"));
        UIText(shown, (int)nameBox.x + (int)(6 * scale),
               (int)nameBox.y + (tmplRowH - 4 - font) / 2, font,
               b->used || editing ? UI_TEXT_PRIMARY : UI_TEXT_MUTED);

        if (nameHover && click) {
            g_editingBuild = editing ? -1 : i;
            Audio_Play(SFX_UI_CLICK);
        }

        // Save / Load / Clear.
        struct { const char *label; int kind; bool enabled; } acts[3] = {
            { "Save",  0, canEdit },
            { "Load",  1, canEdit && b->used },
            { "Clear", 2, b->used },
        };
        for (int k = 0; k < 3; k++) {
            Rectangle btnR = { nameBox.x + nameBox.width + (float)pad + (float)(k * (actW + 6)),
                               (float)ty, (float)actW, (float)(tmplRowH - 4) };
            bool bh = CheckCollisionPointRec(mouse, btnR);
            DrawRectangleRec(btnR, !acts[k].enabled ? (Color){ 30, 31, 38, 255 }
                                 : bh ? UI_SURFACE_HOVER : UI_SURFACE_RAISE);
            DrawRectangleLinesEx(btnR, 1.0f, acts[k].enabled ? UI_GOLD_DIM : (Color){ 58, 56, 52, 255 });
            int lw = UITextWidth(acts[k].label, small);
            UIText(acts[k].label, (int)(btnR.x + (btnR.width - lw) / 2),
                   (int)(btnR.y + (tmplRowH - 4 - small) / 2), small,
                   acts[k].enabled ? RAYWHITE : UI_TEXT_MUTED);

            if (bh && click) {
                if (!acts[k].enabled) {
                    Audio_Play(SFX_UI_DENY);
                } else if (acts[k].kind == 0) {
                    Builds_SaveFrom(i, player);
                    Audio_Play(SFX_UI_CONFIRM);
                    UI_Notify("Build saved");
                    Save_Write();
                } else if (acts[k].kind == 1) {
                    Builds_LoadInto(i, player);
                    g_armedBarSlot = -1;
                    Audio_Play(SFX_UI_CONFIRM);
                    UI_Notify("Build loaded");
                    Save_Write();
                } else {
                    Builds_Clear(i);
                    if (g_editingBuild == i) g_editingBuild = -1;
                    Audio_Play(SFX_UI_CLICK);
                    Save_Write();
                }
            }
        }
        ty += tmplRowH;
    }

    // Typing into the armed name field. Handled last so a click that
    // just armed a different field doesn't also eat this frame's keys.
    if (g_editingBuild >= 0) {
        BuildTemplate *b = Builds_Slot(g_editingBuild);
        if (b) {
            int c = GetCharPressed();
            while (c > 0) {
                int len = (int)strlen(b->name);
                // '|' and '=' are the save file's own delimiters, so a name
                // containing one would come back mangled on load.
                if (c >= 32 && c <= 126 && c != '|' && c != '=' &&
                    len < BUILD_NAME_LEN - 1) {
                    b->name[len] = (char)c;
                    b->name[len + 1] = '\0';
                }
                c = GetCharPressed();
            }
            if (IsKeyPressed(KEY_BACKSPACE)) {
                int len = (int)strlen(b->name);
                if (len > 0) b->name[len - 1] = '\0';
            }
            if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER)) {
                g_editingBuild = -1;
                Save_Write();
            }
        }
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
        if (Skillbook_IsUnlocked(i) || g_skillDB[i].isElite || !g_skillDB[i].preSearing) continue;
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

    char sub[128];
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
        if (Skillbook_IsUnlocked(i) || g_skillDB[i].isElite || !g_skillDB[i].preSearing) continue;
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

        // The trainer's whole job is helping you decide, so the same
        // tooltip the skills panel uses shows up here - on hover, and on
        // pad focus, since a controller has no pointer to hover with.
        if (hovered || focused) UITooltip_Request(i, row);

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


// GW1's title screen, with the one track pre-Searing has. A title is
// worn, not just collected, so the panel's real job is the toggle: the
// point of Legendary Defender of Ascalon is that everyone can see it.

// The collector's trade: one offer, taken or not. Deliberately not a
// shop - GW1's collectors have exactly one thing they want and one
// thing they give, and that flatness is what makes them read as people
// standing in a field rather than as vendors.
static void DrawCollector(Entity *player, int screenWidth, int screenHeight) {
    const Entity *npc = Entity_Get(g_dialogNpc);
    const CollectorOffer *offer = npc ? Collectors_OfferFor(npc->name) : NULL;
    if (!offer) { g_collectorOpen = false; return; }

    float scale = UI_Scale(screenHeight);
    int font = (int)(12 * scale);
    int small = (int)(10 * scale);
    int pad = (int)(10 * scale);
    int w = (int)(430 * scale);
    int h = pad * 4 + font * 4 + (int)(40 * scale);

    g_collectorRect = (Rectangle){ (float)(screenWidth - w) / 2.0f, (float)(110 * scale),
                                   (float)w, (float)h };
    UIHit_Claim(g_collectorRect);
    UI_ThemePanel(g_collectorRect, scale, pad + font + pad / 2);

    if (PanelHeader(g_collectorRect, "Collector", NULL, NULL, font, pad, scale)) {
        g_collectorOpen = false;
        return;
    }

    int x = (int)g_collectorRect.x + pad;
    int y = (int)g_collectorRect.y + pad + font + pad;

    int have = Items_CountMaterial(offer->material);
    bool enough = have >= offer->count;
    bool room = g_inventoryCount < Items_Capacity();

    char want[96];
    snprintf(want, sizeof(want), "Wants:  %d x %s   (you have %d)",
             offer->count, offer->material, have);
    UIText(want, x, y, font, enough ? UI_POSITIVE : UI_TEXT_PRIMARY);
    y += font + (int)(6 * scale);

    char gives[96];
    if (offer->reward.kind == ITEM_ARMOR) {
        snprintf(gives, sizeof(gives), "Gives:  %s", offer->reward.name);
    } else if (offer->reward.kind == ITEM_OFFHAND) {
        snprintf(gives, sizeof(gives), "Gives:  %s  (+%d AL)", offer->reward.name, offer->reward.armor);
    } else {
        snprintf(gives, sizeof(gives), "Gives:  %s  %d-%d", offer->reward.name,
                 offer->reward.dmgMin, offer->reward.dmgMax);
    }
    UIText(gives, x, y, font, UI_GOLD);
    y += font + (int)(6 * scale);

    UIText("Collectors ask for trophies, never gold.", x, y, small, UI_TEXT_MUTED);
    y += small + (int)(8 * scale);

    bool canTrade = enough && room;
    Rectangle btn = { (float)x, (float)y, g_collectorRect.width - pad * 2, (float)(30 * scale) };
    bool focused = UIFocus_Item();
    const char *label = !enough ? "Not enough yet"
                      : !room ? "No room in your bags"
                              : "Make the trade";
    if (focused) DrawRectangleLinesEx((Rectangle){ btn.x - 2, btn.y - 2,
                                                   btn.width + 4, btn.height + 4 }, 2.0f, UI_GOLD);
    if ((UI_Button(btn, label, font, canTrade, canTrade || focused) ||
         (focused && canTrade && UIFocus_Confirm())) && canTrade) {
        if (Items_ConsumeMaterial(offer->material, offer->count)) {
            Items_AddToInventory(offer->reward);
            Audio_Play(SFX_UI_CONFIRM);
            char msg[96];
            snprintf(msg, sizeof(msg), "Received:  %.40s", offer->reward.name);
            UI_Notify(msg);
            Save_Write();
        }
    }
    (void)player;
}

static void DrawTitles(Entity *player, int screenWidth, int screenHeight) {
    float scale = UI_Scale(screenHeight);
    int font = (int)(12 * scale);
    int small = (int)(10 * scale);
    int pad = (int)(10 * scale);
    int rowH = (int)(64 * scale);
    int w = (int)(440 * scale);
    int h = pad * 3 + font + TITLE_COUNT * rowH + pad;

    g_titlesRect = (Rectangle){ (float)(screenWidth - w) / 2.0f, (float)(120 * scale),
                                (float)w, (float)h };
    UIHit_Claim(g_titlesRect);
    UI_ThemePanel(g_titlesRect, scale, pad + font + pad / 2);

    if (PanelHeader(g_titlesRect, "Titles", "T", NULL, font, pad, scale)) {
        g_titlesOpen = false;
        return;
    }

    int x = (int)g_titlesRect.x + pad;
    int y = (int)g_titlesRect.y + pad + font + pad;

    bool click = UI_PointerClicked();
    Vector2 mouse = UI_PointerPos();

    for (int i = 0; i < TITLE_COUNT; i++) {
        TitleId id = (TitleId)i;
        bool revealed = Titles_IsRevealed(id, player);
        bool earned = Titles_IsEarned(id, player);
        bool worn = (Titles_Displayed() == i);

        Rectangle row = { g_titlesRect.x + 4, (float)y - 2, g_titlesRect.width - 8, (float)rowH - 6 };
        bool hovered = CheckCollisionPointRec(mouse, row);
        bool focused = UIFocus_Item();
        if (hovered || focused) DrawRectangleRec(row, (Color){ 46, 48, 64, 160 });
        if (focused) DrawRectangleLinesEx(row, 1.5f, UI_GOLD);

        if (!revealed) {
            // GW1 doesn't list this track until level 12. Saying a title
            // exists is more useful than an empty bar you can't move.
            UIText("? ? ?", x, y, font, UI_TEXT_MUTED);
            char note[96];
            snprintf(note, sizeof(note),
                     "A title is spoken of in Ascalon. It reveals itself at level %d.",
                     TITLE_LDOA_REVEAL);
            UIText(note, x, y + font + (int)(6 * scale), small, UI_TEXT_MUTED);
            y += rowH;
            continue;
        }

        UIText(Titles_Name(id), x, y, font, earned ? UI_GOLD : UI_TEXT_PRIMARY);

        // Progress bar, in levels - the requirement is written in levels
        // and that is the number the player is watching climb.
        int barY = y + font + (int)(6 * scale);
        int barW = (int)(g_titlesRect.width) - pad * 2 - (int)(96 * scale);
        UI_ThemeBar((Rectangle){ (float)x, (float)barY, (float)barW, (float)(10 * scale) },
                    Titles_Progress(id, player),
                    earned ? UI_GOLD : (Color){ 90, 120, 170, 255 }, NULL, small);

        char status[64];
        if (earned) snprintf(status, sizeof(status), "Earned");
        else snprintf(status, sizeof(status), "Level %d of %d", player->level, TITLE_LDOA_LEVEL);
        UIText(status, x, barY + (int)(14 * scale), small,
               earned ? UI_POSITIVE : UI_TEXT_SECOND);

        // The display toggle.
        int btnW = (int)(84 * scale), btnH = (int)(24 * scale);
        Rectangle btn = { g_titlesRect.x + g_titlesRect.width - pad - btnW,
                          (float)y + (float)(8 * scale), (float)btnW, (float)btnH };
        bool btnHover = CheckCollisionPointRec(mouse, btn);
        DrawRectangleRec(btn, !earned ? (Color){ 30, 31, 38, 255 }
                            : worn ? (Color){ 92, 78, 40, 255 }
                            : btnHover ? UI_SURFACE_HOVER : UI_SURFACE_RAISE);
        DrawRectangleLinesEx(btn, 1.0f, earned ? UI_GOLD_DIM : (Color){ 60, 58, 54, 255 });
        const char *label = !earned ? "locked" : worn ? "Displayed" : "Display";
        int lw = UITextWidth(label, small);
        UIText(label, (int)(btn.x + (btn.width - lw) / 2),
               (int)(btn.y + (btn.height - small) / 2), small,
               earned ? RAYWHITE : UI_TEXT_MUTED);

        if (earned && ((btnHover && click) || (focused && UIFocus_Confirm()))) {
            Titles_SetDisplayed(worn ? -1 : i);
            Audio_Play(SFX_UI_CONFIRM);
            Save_Write();
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
    // Left/right switches tabs, up/down walks the rows. The tabs used to
    // be the first two entries of the same focus list, which meant
    // reaching the top item took a step DOWN past both of them - not how
    // a tabbed window reads on a pad.
    {
        int hstep = UIFocus_Horizontal();
        if (hstep != 0) {
            int next = (g_shopTab + hstep) & 1;
            if (next != g_shopTab) {
                g_shopTab = next;
                UIFocus_Clear(); // the list underneath just changed entirely
                Audio_Play(SFX_UI_CLICK);
            }
        }

        float tabW = (g_shopRect.width - 2 * pad) / 2.0f;
        const char *names[2] = { "Buy", "Sell" };
        for (int t = 0; t < 2; t++) {
            Rectangle tab = { g_shopRect.x + pad + t * tabW, (float)y, tabW, (float)tabH };
            bool active = (g_shopTab == t);
            bool hovered = CheckCollisionPointRec(mouse, tab);
            DrawRectangleRec(tab, active ? (Color){ 60, 66, 90, 255 }
                            : hovered ? (Color){ 45, 50, 70, 255 }
                                      : (Color){ 32, 35, 48, 255 });
            DrawRectangleLinesEx(tab, 1.0f, UI_GOLD_DIM);
            if (active) {
                // Gold underline marks the live tab.
                DrawRectangle((int)tab.x, (int)(tab.y + tab.height - 3), (int)tab.width, 3, UI_GOLD);
            }
            int tw = UITextWidth(names[t], font);
            UIText(names[t], (int)(tab.x + (tab.width - tw) / 2),
                   (int)(tab.y + (tab.height - font) / 2), font, active ? RAYWHITE : LIGHTGRAY);
            if (hovered && click) {
                if (g_shopTab != t) { Audio_Play(SFX_UI_CLICK); UIFocus_Clear(); }
                g_shopTab = t;
            }
        }
        // A pad has no pointer, so name the binding rather than leaving
        // the player to discover it.
        if (IsGamepadAvailable(0)) {
            const char *hint = "D-pad left/right switches tab";
            int hw = UITextWidth(hint, (int)(10 * scale));
            UIText(hint, (int)(g_shopRect.x + g_shopRect.width) - pad - hw,
                   y + tabH + (int)(2 * scale), (int)(10 * scale), UI_TEXT_MUTED);
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
            bool equipped = Items_IsEquipped(i);
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


// The equipment screen: what's in each slot, with the numbers that
// matter, plus a character summary - so "what do I have equipped
// where" is never a mystery. E toggles it; the pad's Y opens it with
// the inventory.

// The Armorer's crafting window: recipes take gold AND Charr Carvings,
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

// One reply in the dialog: a label, whether it can be taken, and what
// taking it does. Modelling the replies as a list is what lets a giver
// offer several quests at once - GW1 shows each as its own line.
typedef enum {
    DOPT_NONE = 0,   // a disabled line that names a requirement
    DOPT_ACCEPT, DOPT_TURNIN,
    DOPT_VIEW_QUEST, // open a quest's description page
    DOPT_BACK,       // back to the list of quest titles
    DOPT_SHOP, DOPT_CRAFT, DOPT_COLLECTOR, DOPT_TRAINER, DOPT_PROFESSION,
    DOPT_HIRE_THOM
} DlgOptKind;

typedef struct {
    char label[224];
    bool enabled;
    DlgOptKind kind;
    int param;       // quest index for ACCEPT / TURNIN
} DlgOpt;

#define MAX_DLG_OPTS 12

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

    // --- Decide what the NPC says and which replies to offer --------
    const char *body = "";
    char bodyBuf[512];
    DlgOpt opts[MAX_DLG_OPTS];
    int nopt = 0;

    switch (npc->npcRole) {
        case NPC_QUEST_GIVER: {
            // Each giver only deals in their own quest book, and GW1 lists
            // every one of that book's ready and available quests as its
            // own reply rather than a single next-quest button.
            int ready[QUEST_COUNT], offer[QUEST_COUNT];
            int nready = Quests_ReadyToTurnInListFor(npc->name, ready, QUEST_COUNT);
            int noffer = Quests_OfferableListFor(npc->name, offer, QUEST_COUNT);

            // Detail page: reading one offered quest before deciding. Only
            // valid while that quest is still on this giver's offer list.
            if (g_dialogQuest >= 0) {
                bool valid = false;
                for (int i = 0; i < noffer; i++) if (offer[i] == g_dialogQuest) valid = true;
                if (valid) {
                    Quest *q = &g_quests[g_dialogQuest];
                    char rewards[96];
                    QuestRewardSummary(q, player, rewards, sizeof(rewards));
                    snprintf(bodyBuf, sizeof(bodyBuf), "%s\n\nObjective: %s\n\nReward: %s",
                             q->offerText, q->objective, rewards);
                    body = bodyBuf;
                    snprintf(opts[0].label, sizeof(opts[0].label), "Accept: %.40s", q->name);
                    opts[0].enabled = true; opts[0].kind = DOPT_ACCEPT; opts[0].param = g_dialogQuest;
                    snprintf(opts[1].label, sizeof(opts[1].label), "Decline");
                    opts[1].enabled = true; opts[1].kind = DOPT_BACK;
                    nopt = 2;
                    break;
                }
                g_dialogQuest = -1; // the quest went away - fall back to the list
            }

            for (int i = 0; i < nready && nopt < MAX_DLG_OPTS; i++) {
                Quest *q = &g_quests[ready[i]];
                bool bagFull = q->rewardItem && g_inventoryCount >= MAX_INVENTORY;
                char rewards[96];
                QuestRewardSummary(q, player, rewards, sizeof(rewards));
                snprintf(opts[nopt].label, sizeof(opts[nopt].label),
                         "Turn in: %.48s (%.60s)", q->name, rewards);
                opts[nopt].enabled = !bagFull;
                opts[nopt].kind = DOPT_TURNIN;
                opts[nopt].param = ready[i];
                nopt++;
            }
            // Offers open a description page rather than accepting outright.
            for (int i = 0; i < noffer && nopt < MAX_DLG_OPTS; i++) {
                Quest *q = &g_quests[offer[i]];
                snprintf(opts[nopt].label, sizeof(opts[nopt].label), "%.60s", q->name);
                opts[nopt].enabled = true;
                opts[nopt].kind = DOPT_VIEW_QUEST;
                opts[nopt].param = offer[i];
                nopt++;
            }
            if (nready == 1 && noffer == 0) {
                bool bagFull = g_quests[ready[0]].rewardItem && g_inventoryCount >= MAX_INVENTORY;
                body = bagFull ? "Your bags are full - make room for your reward."
                               : "You've done it! Ascalon thanks you.";
            } else if (noffer > 0) {
                // The titles are the replies; the description waits on the
                // page behind each one, so the greeting just points at them.
                body = "I've work that needs doing. Take a look.";
            } else if (nready > 0) {
                body = "You've done it! Ascalon thanks you.";
            } else {
                bool anyActive = false;
                for (int i = 0; i < QUEST_COUNT; i++) {
                    if (g_quests[i].state == QUEST_ACTIVE &&
                        strcmp(g_quests[i].giverName, npc->name) == 0) anyActive = true;
                }
                body = anyActive ? "Your task awaits. Good hunting."
                                 : "Nothing more for now, friend.";
            }
            break;
        }
        case NPC_MERCHANT:
            body = "Weapons - fair prices, no haggling. Armor? See Dunda.";
            snprintf(opts[0].label, sizeof(opts[0].label), "%s",
                     g_shopOpen ? "Close shop" : "Browse wares");
            opts[0].enabled = true; opts[0].kind = DOPT_SHOP; nopt = 1;
            break;
        case NPC_CRAFTER:
            body = "Bring me Charr hides and coin - I'll fit you properly.";
            snprintf(opts[0].label, sizeof(opts[0].label), "%s",
                     g_craftOpen ? "Close crafting" : "Craft armor");
            opts[0].enabled = true; opts[0].kind = DOPT_CRAFT; nopt = 1;
            break;
        case NPC_COLLECTOR: {
            const CollectorOffer *offer = Collectors_OfferFor(npc->name);
            if (!offer) { body = "I've nothing to trade just now."; break; }
            body = offer->flavour;
            snprintf(opts[0].label, sizeof(opts[0].label), "%s",
                     g_collectorOpen ? "Step away" : "Look at the trade");
            opts[0].enabled = true; opts[0].kind = DOPT_COLLECTOR; nopt = 1;
            break;
        }
        case NPC_SKILL_TRAINER:
            body = "Skills are earned, not given. Points and coin, and I'll teach.";
            snprintf(opts[0].label, sizeof(opts[0].label), "%s",
                     g_trainerOpen ? "Close training" : "Learn skills");
            opts[0].enabled = true; opts[0].kind = DOPT_TRAINER; nopt = 1;
            break;
        case NPC_PROFESSION_CHANGER: {
            bool hasSecondary = (g_character.secondary != PROF_NONE);
            // Whichever gate you don't meet is said out loud, with the
            // requirement named - an NPC that just refuses teaches nothing.
            if (!hasSecondary) {
                if (SecondaryGrantAllowed()) {
                    body = "You've bled for Ascalon. Choose a second calling.";
                    snprintf(opts[0].label, sizeof(opts[0].label), "Choose a second profession");
                    opts[0].enabled = true; opts[0].kind = DOPT_PROFESSION;
                } else {
                    body = "Prove yourself first. Osric has work - finish it.";
                    snprintf(opts[0].label, sizeof(opts[0].label), "Requires: %s", SECONDARY_QUEST);
                    opts[0].enabled = false; opts[0].kind = DOPT_NONE;
                }
            } else if (SecondaryChangeAllowed(player)) {
                body = "Second thoughts? I can unmake the choice.";
                snprintf(opts[0].label, sizeof(opts[0].label), "%s",
                         g_professionOpen ? "Never mind" : "Change my second profession");
                opts[0].enabled = true; opts[0].kind = DOPT_PROFESSION;
            } else {
                body = "A calling isn't a coat. Live with it a while longer.";
                snprintf(opts[0].label, sizeof(opts[0].label),
                         "Requires: level %d (you are %d)", SECONDARY_CHANGE_LEVEL, player->level);
                opts[0].enabled = false; opts[0].kind = DOPT_NONE;
            }
            nopt = 1;
            break;
        }
        case NPC_HENCHMAN:
            body = "Need another axe... er, sword? I work for loot shares.";
            snprintf(opts[0].label, sizeof(opts[0].label), "Hire Little Thom (free)");
            opts[0].enabled = true; opts[0].kind = DOPT_HIRE_THOM; nopt = 1;
            break;
        default:
            break;
    }

    // --- Size the panel to the wrapped text and the reply list ------
    int maxw = w - 2 * pad;
    int lineGap = (int)(3 * scale);
    int nameAdv = font + (int)(8 * scale);
    int bodyLines = UITextWrapped(body, 0, 0, font, maxw, lineGap, LIGHTGRAY, false);
    int bodyH = bodyLines * (font + lineGap);
    int optGap = (int)(6 * scale);
    int optsH = nopt > 0 ? nopt * btnH + (nopt - 1) * optGap : 0;
    int gapBodyOpts = nopt > 0 ? (int)(8 * scale) : 0;
    int h = pad + nameAdv + bodyH + gapBodyOpts + optsH + pad;

    // Anchored by its bottom edge so it grows upward as replies pile up,
    // never off the bottom of the screen and never over the skill bar.
    float bottomY = (float)screenHeight - 96.0f * scale;
    float top = bottomY - (float)h;
    if (top < 10.0f * scale) top = 10.0f * scale;
    g_dialogRect = (Rectangle){ (float)(screenWidth - w) / 2.0f, top, (float)w, (float)h };
    UIHit_Claim(g_dialogRect);
    UI_ThemePanel(g_dialogRect, scale, pad + font + 6);

    int x = (int)g_dialogRect.x + pad;
    int y = (int)g_dialogRect.y + pad;
    UIText(npc->name, x, y, font, (Color){ 130, 220, 130, 255 });
    y += nameAdv;
    UITextWrapped(body, x, y, font, maxw, lineGap, LIGHTGRAY, true);
    y += bodyH + gapBodyOpts;

    // Draw every reply (so focus registration stays stable), remember
    // which was taken, and act on it after the list is laid out.
    int chosen = -1;
    for (int i = 0; i < nopt; i++) {
        Rectangle b = { (float)x, (float)y, (float)maxw, (float)btnH };
        if (DialogButton(b, opts[i].label, font, opts[i].enabled)) chosen = i;
        y += btnH + optGap;
    }
    if (chosen >= 0) {
        switch (opts[chosen].kind) {
            case DOPT_VIEW_QUEST: g_dialogQuest = opts[chosen].param; UIFocus_Clear(); break;
            case DOPT_ACCEPT:  Quests_Accept(opts[chosen].param);
                               g_dialogQuest = -1; UIFocus_Clear(); break; // back to the list
            case DOPT_BACK:    g_dialogQuest = -1; UIFocus_Clear(); break;
            case DOPT_TURNIN:  Quests_TurnIn(player, opts[chosen].param); break;
            case DOPT_SHOP:    g_shopOpen = !g_shopOpen; UIFocus_Clear(); break;
            case DOPT_CRAFT:   g_craftOpen = !g_craftOpen; UIFocus_Clear(); break;
            case DOPT_COLLECTOR: g_collectorOpen = !g_collectorOpen; UIFocus_Clear(); break;
            case DOPT_TRAINER: g_trainerOpen = !g_trainerOpen; UIFocus_Clear(); break;
            case DOPT_PROFESSION: g_professionOpen = !g_professionOpen; UIFocus_Clear(); break;
            case DOPT_HIRE_THOM:
                World_SetThomHired(true);
                // Convert the standing NPC into a fighting party member on
                // the spot; zone loads keep him from then on, from the same
                // stat setup so the two copies can't drift.
                npc->kind = ENT_HERO;
                npc->npcRole = NPC_NONE;
                World_SetupThomStats(npc);
                g_dialogNpc = -1;
                break;
            case DOPT_NONE: break;
        }
    }
}

void UI_PanelsUpdateAndDraw(int screenWidth, int screenHeight) {
    Entity *player = Entity_Get(PLAYER_INDEX);
    if (!player) return;

    if (g_editingBuild < 0) {
    if (IsKeyPressed(KEY_I)) g_invOpen = !g_invOpen;
    // K used to open a separate attributes window. Attributes are now
    // part of the build screen, so both keys land on the same place.
    if (IsKeyPressed(KEY_K)) g_skillsOpen = !g_skillsOpen;
    if (IsKeyPressed(KEY_E)) g_invOpen = !g_invOpen; // gear and bags are one screen
    if (IsKeyPressed(KEY_T)) g_titlesOpen = !g_titlesOpen;
    if (IsKeyPressed(KEY_L)) {
        g_skillsOpen = !g_skillsOpen;
        g_armedBarSlot = -1;
    }
    }
    // Focus navigation covers the NPC dialog and the windows it opens -
    // the parts of the game that are a list of choices. Only run while
    // one is up, or the arrow keys would be swallowed during play.
    bool focusList = (g_dialogNpc >= 0) || g_titlesOpen;
    if (focusList) UIFocus_Begin();

    if (g_invOpen) DrawInventory(player, screenHeight);
    if (g_skillsOpen) DrawSkillsPanel(player, screenWidth, screenHeight);
    if (g_titlesOpen) DrawTitles(player, screenWidth, screenHeight);
    if (g_dialogNpc >= 0) DrawNpcDialog(player, screenWidth, screenHeight);
    if (g_shopOpen && g_dialogNpc >= 0) DrawShop(screenWidth, screenHeight);
    if (g_craftOpen && g_dialogNpc >= 0) DrawCraft(player, screenWidth, screenHeight);
    if (g_trainerOpen && g_dialogNpc >= 0) DrawTrainer(player, screenWidth, screenHeight);
    if (g_collectorOpen && g_dialogNpc >= 0) DrawCollector(player, screenWidth, screenHeight);
    if (g_professionOpen && g_dialogNpc >= 0) DrawProfessionPanel(player, screenWidth, screenHeight);

    if (focusList) {
        UIFocus_End();
        // Back closes whatever is deepest, so a pad can always retreat
        // without needing the mouse to find a close box.
        if (UIFocus_Cancel()) {
            if (g_shopOpen || g_craftOpen || g_trainerOpen || g_professionOpen ||
                g_collectorOpen) {
                g_shopOpen = g_craftOpen = g_trainerOpen = g_professionOpen = false;
                g_collectorOpen = false;
                UIFocus_Clear();
                Audio_Play(SFX_UI_CLOSE);
            } else if (g_dialogQuest >= 0) {
                g_dialogQuest = -1; // a quest page backs out to the list first
                UIFocus_Clear();
                Audio_Play(SFX_UI_CLOSE);
            } else {
                UI_CloseNpcDialog();
            }
        }
    }
}
