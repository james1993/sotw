#include "ui_panels.h"
#include "entity.h"
#include "items.h"
#include "attributes.h"
#include "ui_font.h"
#include <stdio.h>

#define PLAYER_INDEX 0

static bool g_invOpen = false;
static bool g_attrOpen = false;

static Rectangle g_invRect, g_attrRect;

static float UIScale(int screenHeight) {
    float scale = (float)screenHeight / 800.0f;
    if (scale < 0.85f) scale = 0.85f;
    if (scale > 5.0f) scale = 5.0f;
    return scale;
}

bool UI_PointerOverPanels(Vector2 point) {
    if (g_invOpen && CheckCollisionPointRec(point, g_invRect)) return true;
    if (g_attrOpen && CheckCollisionPointRec(point, g_attrRect)) return true;
    return false;
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

void UI_PanelsUpdateAndDraw(int screenWidth, int screenHeight) {
    Entity *player = Entity_Get(PLAYER_INDEX);
    if (!player) return;

    if (IsKeyPressed(KEY_I)) g_invOpen = !g_invOpen;
    if (IsKeyPressed(KEY_K)) g_attrOpen = !g_attrOpen;

    if (g_invOpen) DrawInventory(player, screenHeight);
    if (g_attrOpen) DrawAttributes(player, screenWidth, screenHeight);
}
