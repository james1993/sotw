#include "ui_panels.h"
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

static float UIScale(int screenHeight) {
    float scale = (float)screenHeight / 800.0f;
    if (scale < 0.85f) scale = 0.85f;
    if (scale > 5.0f) scale = 5.0f;
    return scale;
}

bool UI_PointerOverPanels(Vector2 point) {
    if (g_invOpen && CheckCollisionPointRec(point, g_invRect)) return true;
    if (g_attrOpen && CheckCollisionPointRec(point, g_attrRect)) return true;
    if (g_dialogNpc >= 0 && CheckCollisionPointRec(point, g_dialogRect)) return true;
    if (g_shopOpen && CheckCollisionPointRec(point, g_shopRect)) return true;
    return false;
}

void UI_OpenNpcDialog(int entityIndex) {
    g_dialogNpc = entityIndex;
    g_shopOpen = false;
}

static void DrawInventory(Entity *player, int screenHeight) {
    float scale = UIScale(screenHeight);
    int rowH = (int)(24 * scale);
    int font = (int)(12 * scale);
    int pad = (int)(10 * scale);
    int w = (int)(300 * scale);
    int h = pad * 3 + font + (g_inventoryCount + 1) * rowH;

    g_invRect = (Rectangle){ (float)(20 * scale), (float)(120 * scale), (float)w, (float)h };
    DrawRectangleRec(g_invRect, (Color){ 20, 22, 30, 235 });
    DrawRectangleLinesEx(g_invRect, 2, (Color){ 120, 120, 140, 255 });

    int x = (int)g_invRect.x + pad;
    int y = (int)g_invRect.y + pad;
    char title[48];
    snprintf(title, sizeof(title), "Inventory        %d gold", g_gold);
    UIText(title, x, y, font, GOLD);
    y += font + pad;

    bool click = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    Vector2 mouse = GetMousePosition();

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
    float scale = UIScale(screenHeight);
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
    DrawRectangleRec(g_attrRect, (Color){ 20, 22, 30, 235 });
    DrawRectangleLinesEx(g_attrRect, 2, (Color){ 120, 120, 140, 255 });

    int x = (int)g_attrRect.x + pad;
    int y = (int)g_attrRect.y + pad;
    char title[64];
    snprintf(title, sizeof(title), "Attributes      %d points free", player->attributePoints);
    UIText(title, x, y, font, GOLD);
    y += font + pad;

    bool click = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    Vector2 mouse = GetMousePosition();
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

// One clickable dialog button; returns true when clicked this frame.
static bool DialogButton(Rectangle rect, const char *label, int font, bool enabled) {
    Vector2 mouse = GetMousePosition();
    bool hovered = enabled && CheckCollisionPointRec(mouse, rect);
    DrawRectangleRec(rect, !enabled ? (Color){ 40, 40, 40, 255 }
                     : hovered ? (Color){ 80, 90, 120, 255 } : (Color){ 55, 60, 80, 255 });
    DrawRectangleLinesEx(rect, 1, LIGHTGRAY);
    UIText(label, (int)rect.x + 8, (int)rect.y + ((int)rect.height - font) / 2, font,
           enabled ? RAYWHITE : GRAY);
    return hovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
}

static void DrawShop(int screenWidth, int screenHeight) {
    float scale = UIScale(screenHeight);
    int rowH = (int)(26 * scale);
    int font = (int)(12 * scale);
    int pad = (int)(10 * scale);
    int w = (int)(380 * scale);
    int rows = SHOP_STOCK_COUNT + g_inventoryCount;
    int h = pad * 4 + font * 2 + (rows + 2) * rowH;

    g_shopRect = (Rectangle){ (float)(screenWidth - w) / 2.0f, (float)(90 * scale), (float)w, (float)h };
    DrawRectangleRec(g_shopRect, (Color){ 20, 22, 30, 240 });
    DrawRectangleLinesEx(g_shopRect, 2, (Color){ 120, 120, 140, 255 });

    int x = (int)g_shopRect.x + pad;
    int y = (int)g_shopRect.y + pad;
    char title[48];
    snprintf(title, sizeof(title), "Merchant        you have %d gold", g_gold);
    UIText(title, x, y, font, GOLD);
    y += font + pad;

    bool click = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    Vector2 mouse = GetMousePosition();

    for (int i = 0; i < SHOP_STOCK_COUNT; i++) {
        const ShopEntry *entry = &g_shopStock[i];
        Rectangle row = { g_shopRect.x + 4, (float)y - 2, g_shopRect.width - 8, (float)rowH };
        bool canAfford = g_gold >= entry->price && g_inventoryCount < MAX_INVENTORY;
        bool hovered = CheckCollisionPointRec(mouse, row);
        if (hovered && canAfford) DrawRectangleRec(row, (Color){ 60, 60, 80, 255 });

        char label[80];
        if (entry->item.kind == ITEM_WEAPON) {
            snprintf(label, sizeof(label), "Buy: %s %d-%d  (%dg)", entry->item.name,
                     entry->item.dmgMin, entry->item.dmgMax, entry->price);
        } else {
            snprintf(label, sizeof(label), "Buy: %s  (%dg)", entry->item.name, entry->price);
        }
        UIText(label, x, y, font, canAfford ? RAYWHITE : GRAY);

        if (hovered && click && canAfford) {
            g_gold -= entry->price;
            Items_AddToInventory(entry->item);
        }
        y += rowH;
    }

    y += pad;
    UIText("Sell (click - equipped items can't be sold):", x, y, font, LIGHTGRAY);
    y += font + 6;

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

    float scale = UIScale(screenHeight);
    int font = (int)(13 * scale);
    int pad = (int)(12 * scale);
    int btnH = (int)(30 * scale);
    int w = (int)(360 * scale);
    int h = pad * 3 + font * 2 + btnH + 8;

    g_dialogRect = (Rectangle){ (float)(screenWidth - w) / 2.0f,
                                (float)screenHeight - (float)(220 * scale), (float)w, (float)h };
    DrawRectangleRec(g_dialogRect, (Color){ 20, 22, 30, 240 });
    DrawRectangleLinesEx(g_dialogRect, 2, (Color){ 120, 120, 140, 255 });

    int x = (int)g_dialogRect.x + pad;
    int y = (int)g_dialogRect.y + pad;
    UIText(npc->name, x, y, font, (Color){ 130, 220, 130, 255 });
    y += font + 6;

    Rectangle btn = { (float)x, (float)y + font + 6, g_dialogRect.width - 2 * pad, (float)btnH };

    switch (npc->npcRole) {
        case NPC_QUEST_GIVER: {
            if (g_quest.state == QUEST_AVAILABLE) {
                UIText("The Charr prowl our plains. Will you thin them out?", x, y, font, LIGHTGRAY);
                if (DialogButton(btn, "Accept: Charr at the Gate (250 XP, 100g)", font, true)) {
                    Quests_Accept();
                }
            } else if (g_quest.state == QUEST_ACTIVE) {
                UIText("The Charr still prowl. Come back when it's done.", x, y, font, LIGHTGRAY);
            } else if (g_quest.state == QUEST_READY_TO_TURN_IN) {
                UIText("You've done it! Ascalon thanks you.", x, y, font, LIGHTGRAY);
                if (DialogButton(btn, "Claim reward (250 XP, 100g)", font, true)) {
                    Quests_TurnIn(player);
                }
            } else {
                UIText("Ashford is safer for your work, friend.", x, y, font, LIGHTGRAY);
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
                // the spot; zone loads keep him from then on.
                npc->kind = ENT_HERO;
                npc->npcRole = NPC_NONE;
                npc->primaryProfession = PROF_WARRIOR;
                npc->secondaryProfession = PROF_MONK;
                npc->level = 5;
                npc->maxHp = npc->hp = 100 + 20 * (npc->level - 1);
                npc->armor = 80;
                npc->attributeRank[ATTR_STRENGTH] = 4;
                npc->attributeRank[ATTR_TACTICS] = 3;
                npc->skillBar[0] = 0; // Gash
                npc->skillBar[1] = 1; // Rush Strike
                npc->skillBar[2] = 2; // Battle Cry
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
    if (IsKeyPressed(KEY_ESCAPE)) { g_dialogNpc = -1; g_shopOpen = false; }

    if (g_invOpen) DrawInventory(player, screenHeight);
    if (g_attrOpen) DrawAttributes(player, screenWidth, screenHeight);
    if (g_dialogNpc >= 0) DrawNpcDialog(player, screenWidth, screenHeight);
    if (g_shopOpen && g_dialogNpc >= 0) DrawShop(screenWidth, screenHeight);
}
