#include "ui_panels.h"
#include "ui_hit.h"
#include "ui_cursor.h"
#include "entity.h"
#include "items.h"
#include "attributes.h"
#include "quests.h"
#include "world.h"
#include "ui_font.h"
#include <math.h>
#include <stdio.h>

#define PLAYER_INDEX 0
#define DIALOG_WALKAWAY_DISTANCE 130.0f

static bool g_invOpen = false;
static bool g_attrOpen = false;
static int g_dialogNpc = -1;   // entity index, -1 = closed
static bool g_shopOpen = false;

static Rectangle g_invRect, g_attrRect, g_dialogRect, g_shopRect;

// The merchant's stock: fixed-power items at fixed prices, in GW1's
// spirit where the merchant sells the basics and rarity is cosmetic.
typedef struct { Item item; int price; } ShopEntry;
static const ShopEntry g_shopStock[] = {
    { { ITEM_WEAPON, "Long Sword",  15, 22, 28.0f,  1.33f, 0 }, 80 },
    { { ITEM_WEAPON, "War Hammer",  19, 35, 30.0f,  1.75f, 0 }, 120 },
    { { ITEM_WEAPON, "Fire Staff",  11, 22, 220.0f, 1.75f, 0 }, 100 },
    { { ITEM_ARMOR, "Monk Raiment (AL 45)", 0, 0, 0, 0, 45 }, 150 },
    { { ITEM_ARMOR, "Monk Raiment (AL 60)", 0, 0, 0, 0, 60 }, 300 },
};
#define SHOP_STOCK_COUNT (int)(sizeof(g_shopStock) / sizeof(g_shopStock[0]))

// Set on the frame a dialog opens so the very same X/Square press that
// opened it can't also "click" its button (input runs before drawing,
// IsGamepadButtonPressed stays true for the whole frame).
static bool g_dialogOpenedThisFrame = false;

void UI_OpenNpcDialog(int entityIndex) {
    g_dialogNpc = entityIndex;
    g_shopOpen = false;
    g_dialogOpenedThisFrame = true;
}

bool UI_IsNpcDialogOpen(void) {
    return g_dialogNpc >= 0;
}

void UI_CloseNpcDialog(void) {
    g_dialogNpc = -1;
    g_shopOpen = false;
}

bool UI_IsInventoryOpen(void) { return g_invOpen; }
bool UI_IsAttributesOpen(void) { return g_attrOpen; }

void UI_ToggleInventory(void) { g_invOpen = !g_invOpen; }

void UI_ClosePanels(void) {
    g_invOpen = false;
    g_attrOpen = false;
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
    DrawRectangleRec(g_invRect, (Color){ 20, 22, 30, 235 });
    DrawRectangleLinesEx(g_invRect, 2, (Color){ 120, 120, 140, 255 });

    int x = (int)g_invRect.x + pad;
    int y = (int)g_invRect.y + pad;
    char title[48];
    snprintf(title, sizeof(title), "Inventory        %d gold", g_gold);
    UIText(title, x, y, font, GOLD);
    y += font + pad;

    bool click = UI_PointerClicked();
    Vector2 mouse = UI_PointerPos();

    for (int i = 0; i < g_inventoryCount; i++) {
        Item *it = &g_inventory[i];
        Rectangle row = { g_invRect.x + 4, (float)y - 2, g_invRect.width - 8, (float)rowH };
        bool hovered = CheckCollisionPointRec(mouse, row);
        bool equipped = (i == g_equippedWeapon || i == g_equippedArmor);

        if (hovered) DrawRectangleRec(row, (Color){ 60, 60, 80, 255 });

        char label[64];
        if (it->kind == ITEM_WEAPON) {
            snprintf(label, sizeof(label), "%s %d-%d%s", it->name, it->dmgMin, it->dmgMax,
                     equipped ? "  [equipped]" : "");
        } else {
            snprintf(label, sizeof(label), "%s%s", it->name, equipped ? "  [equipped]" : "");
        }
        UIText(label, x, y, font, equipped ? SKYBLUE : RAYWHITE);

        if (hovered && click) {
            if (it->kind == ITEM_WEAPON) Items_EquipWeapon(player, i);
            else if (it->kind == ITEM_ARMOR) Items_EquipArmor(player, i);
        }
        y += rowH;
    }

    if (g_inventoryCount == 0) {
        UIText("(empty - kill something)", x, y, font, GRAY);
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
    DrawRectangleRec(g_attrRect, (Color){ 20, 22, 30, 235 });
    DrawRectangleLinesEx(g_attrRect, 2, (Color){ 120, 120, 140, 255 });

    int x = (int)g_attrRect.x + pad;
    int y = (int)g_attrRect.y + pad;
    char title[64];
    snprintf(title, sizeof(title), "Attributes      %d points free", player->attributePoints);
    UIText(title, x, y, font, GOLD);
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
            player->attributeRank[a]++;
            player->attributePoints -= upCost;
        }
        if (click && canDown && CheckCollisionPointRec(mouse, minus)) {
            int refund = g_attrCumulativeCost[rank] - g_attrCumulativeCost[rank - 1];
            player->attributeRank[a]--;
            player->attributePoints += refund;
        }
        y += rowH;
    }
}

// The gamepad talk button doubles as "advance the conversation" while
// a dialog is up - suppressed on the frame the dialog opened so one
// press can't both open and confirm.
static bool GamepadDialogConfirm(void) {
    return !g_dialogOpenedThisFrame && IsGamepadAvailable(0) &&
           IsGamepadButtonPressed(0, GAMEPAD_BUTTON_RIGHT_FACE_LEFT);
}

// One clickable dialog button; returns true when clicked this frame
// (or, for the dialog's single action button, confirmed on the pad).
static bool DialogButton(Rectangle rect, const char *label, int font, bool enabled) {
    Vector2 mouse = UI_PointerPos();
    bool hovered = enabled && CheckCollisionPointRec(mouse, rect);
    DrawRectangleRec(rect, !enabled ? (Color){ 40, 40, 40, 255 }
                     : hovered ? (Color){ 80, 90, 120, 255 } : (Color){ 55, 60, 80, 255 });
    DrawRectangleLinesEx(rect, 1, LIGHTGRAY);
    UIText(label, (int)rect.x + 8, (int)rect.y + ((int)rect.height - font) / 2, font,
           enabled ? RAYWHITE : GRAY);
    if (enabled && GamepadDialogConfirm()) return true;
    return hovered && UI_PointerClicked();
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
    DrawRectangleRec(g_shopRect, (Color){ 20, 22, 30, 240 });
    DrawRectangleLinesEx(g_shopRect, 2, (Color){ 120, 120, 140, 255 });

    int x = (int)g_shopRect.x + pad;
    int y = (int)g_shopRect.y + pad;
    char title[48];
    snprintf(title, sizeof(title), "Merchant        you have %d gold", g_gold);
    UIText(title, x, y, font, GOLD);
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
            bool hovered = CheckCollisionPointRec(mouse, tab);
            DrawRectangleRec(tab, active ? (Color){ 60, 66, 90, 255 }
                            : hovered ? (Color){ 45, 50, 70, 255 } : (Color){ 32, 35, 48, 255 });
            DrawRectangleLinesEx(tab, 1, (Color){ 120, 120, 140, 255 });
            if (active) {
                // Gold underline marks the live tab.
                DrawRectangle((int)tab.x, (int)(tab.y + tab.height - 3), (int)tab.width, 3,
                              (Color){ 200, 170, 90, 255 });
            }
            int tw = UITextWidth(names[t], font);
            UIText(names[t], (int)(tab.x + (tab.width - tw) / 2),
                   (int)(tab.y + (tab.height - font) / 2), font, active ? RAYWHITE : LIGHTGRAY);
            if (hovered && click) g_shopTab = t;
        }
        y += tabH + pad;
    }

    if (g_shopTab == 0) {
        // --- Buy: the merchant's stock ---
        for (int i = 0; i < SHOP_STOCK_COUNT; i++) {
            const ShopEntry *entry = &g_shopStock[i];
            Rectangle row = { g_shopRect.x + 4, (float)y - 2, g_shopRect.width - 8, (float)rowH };
            bool canAfford = g_gold >= entry->price && g_inventoryCount < MAX_INVENTORY;
            bool hovered = CheckCollisionPointRec(mouse, row);
            if (hovered && canAfford) DrawRectangleRec(row, (Color){ 60, 60, 80, 255 });

            char label[80];
            if (entry->item.kind == ITEM_WEAPON) {
                snprintf(label, sizeof(label), "%s %d-%d  (%dg)", entry->item.name,
                         entry->item.dmgMin, entry->item.dmgMax, entry->price);
            } else {
                snprintf(label, sizeof(label), "%s  (%dg)", entry->item.name, entry->price);
            }
            UIText(label, x, y, font, canAfford ? RAYWHITE : GRAY);

            if (hovered && click && canAfford) {
                g_gold -= entry->price;
                Items_AddToInventory(entry->item);
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
            bool hovered = !equipped && CheckCollisionPointRec(mouse, row);
            if (hovered) DrawRectangleRec(row, (Color){ 80, 60, 60, 255 });

            char label[80];
            snprintf(label, sizeof(label), "%s  (+%dg)%s", it->name, Items_SellValue(it),
                     equipped ? "  [equipped]" : "");
            UIText(label, x, y, font, equipped ? GRAY : RAYWHITE);

            if (hovered && click) {
                int value = Items_SellValue(it);
                if (Items_RemoveFromInventory(i)) {
                    g_gold += value;
                }
                break; // indices shifted - redo layout next frame
            }
            y += rowH;
        }
    }
}

static void DrawNpcDialog(Entity *player, int screenWidth, int screenHeight) {
    Entity *npc = Entity_Get(g_dialogNpc);
    if (!npc || npc->kind != ENT_NPC) { g_dialogNpc = -1; g_shopOpen = false; return; }

    // Walking away closes the conversation, like GW1.
    float dx = npc->pos.x - player->pos.x, dy = npc->pos.y - player->pos.y;
    if (sqrtf(dx * dx + dy * dy) > DIALOG_WALKAWAY_DISTANCE) {
        g_dialogNpc = -1;
        g_shopOpen = false;
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
    DrawRectangleRec(g_dialogRect, (Color){ 20, 22, 30, 240 });
    DrawRectangleLinesEx(g_dialogRect, 2, (Color){ 120, 120, 140, 255 });

    int x = (int)g_dialogRect.x + pad;
    int y = (int)g_dialogRect.y + pad;
    UIText(npc->name, x, y, font, (Color){ 130, 220, 130, 255 });
    y += font + 6;

    Rectangle btn = { (float)x, (float)y + font + 6, g_dialogRect.width - 2 * pad, (float)btnH };

    switch (npc->npcRole) {
        case NPC_QUEST_GIVER: {
            int ready = Quests_ReadyToTurnInIndex();
            int offer = Quests_OfferableIndex();
            if (ready >= 0) {
                UIText("You've done it! Ascalon thanks you.", x, y, font, LIGHTGRAY);
                char label[96];
                snprintf(label, sizeof(label), "Turn in: %s (%d XP, %dg)",
                         g_quests[ready].name, g_quests[ready].rewardXP, g_quests[ready].rewardGold);
                if (DialogButton(btn, label, font, true)) {
                    Quests_TurnIn(player, ready);
                }
            } else if (offer >= 0) {
                UIText(offer == 0 ? "The Charr prowl our plains. Will you thin them out?"
                                  : "With the Charr culled, we need eyes on the eastern ridge.",
                       x, y, font, LIGHTGRAY);
                char label[96];
                snprintf(label, sizeof(label), "Accept: %s (%d XP, %dg)",
                         g_quests[offer].name, g_quests[offer].rewardXP, g_quests[offer].rewardGold);
                if (DialogButton(btn, label, font, true)) {
                    Quests_Accept(offer);
                }
            } else {
                bool anyActive = false;
                for (int i = 0; i < QUEST_COUNT; i++) {
                    if (g_quests[i].state == QUEST_ACTIVE) anyActive = true;
                }
                UIText(anyActive ? "Your task awaits in the plains. Good hunting."
                                 : "Ashford is safer for your work, friend.", x, y, font, LIGHTGRAY);
            }
            break;
        }
        case NPC_MERCHANT: {
            UIText("Weapons, armor - fair prices, no haggling.", x, y, font, LIGHTGRAY);
            if (DialogButton(btn, g_shopOpen ? "Close shop" : "Browse wares", font, true)) {
                g_shopOpen = !g_shopOpen;
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
    if (IsKeyPressed(KEY_ESCAPE)) UI_CloseNpcDialog();

    if (g_invOpen) DrawInventory(player, screenHeight);
    if (g_attrOpen) DrawAttributes(player, screenWidth, screenHeight);
    if (g_dialogNpc >= 0) DrawNpcDialog(player, screenWidth, screenHeight);
    if (g_shopOpen && g_dialogNpc >= 0) DrawShop(screenWidth, screenHeight);

    // The open-frame guard only needs to cover the frame the dialog
    // appeared; from the next frame on the pad button confirms.
    g_dialogOpenedThisFrame = false;
}
