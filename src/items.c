#include "items.h"
#include "entity.h"
#include <math.h>
#include <string.h>

#define PICKUP_RADIUS 30.0f

Item g_inventory[MAX_INVENTORY];
int g_inventoryCount = 0;
int g_gold = 0;
int g_equippedWeapon = -1;
int g_equippedArmor = -1;

GroundDrop g_drops[MAX_DROPS];

// Weapon damage ranges are GW1's actual per-type maximums.
static const Item g_weaponTable[] = {
    { ITEM_WEAPON, "Long Sword",  15, 22, 28.0f,  1.33f, 0 },
    { ITEM_WEAPON, "War Hammer",  19, 35, 30.0f,  1.75f, 0 },
    { ITEM_WEAPON, "Fire Staff",  11, 22, 220.0f, 1.75f, 0 },
    { ITEM_WEAPON, "Smiting Rod", 11, 22, 160.0f, 1.75f, 0 },
};
#define WEAPON_TABLE_COUNT (int)(sizeof(g_weaponTable) / sizeof(g_weaponTable[0]))

static const Item g_armorTable[] = {
    { ITEM_ARMOR, "Monk Raiment (AL 30)", 0, 0, 0, 0, 30 },
    { ITEM_ARMOR, "Monk Raiment (AL 45)", 0, 0, 0, 0, 45 },
    { ITEM_ARMOR, "Monk Raiment (AL 60)", 0, 0, 0, 0, 60 },
};
#define ARMOR_TABLE_COUNT (int)(sizeof(g_armorTable) / sizeof(g_armorTable[0]))

void Items_Reset(void) {
    g_inventoryCount = 0;
    g_gold = 0;
    g_equippedWeapon = -1;
    g_equippedArmor = -1;
    memset(g_drops, 0, sizeof(g_drops));
}

bool Items_AddToInventory(Item item) {
    if (g_inventoryCount >= MAX_INVENTORY) return false;
    g_inventory[g_inventoryCount++] = item;
    return true;
}

bool Items_RemoveFromInventory(int inventoryIndex) {
    if (inventoryIndex < 0 || inventoryIndex >= g_inventoryCount) return false;
    if (inventoryIndex == g_equippedWeapon || inventoryIndex == g_equippedArmor) return false;

    for (int i = inventoryIndex; i < g_inventoryCount - 1; i++) {
        g_inventory[i] = g_inventory[i + 1];
    }
    g_inventoryCount--;
    if (g_equippedWeapon > inventoryIndex) g_equippedWeapon--;
    if (g_equippedArmor > inventoryIndex) g_equippedArmor--;
    return true;
}

int Items_SellValue(const Item *item) {
    if (item->kind == ITEM_ARMOR) return 10 + item->armor / 2;
    if (item->kind == ITEM_WEAPON) return 15 + item->dmgMax / 2;
    return 0;
}

void Items_EquipWeapon(Entity *player, int inventoryIndex) {
    if (inventoryIndex < 0 || inventoryIndex >= g_inventoryCount) return;
    Item *it = &g_inventory[inventoryIndex];
    if (it->kind != ITEM_WEAPON) return;
    g_equippedWeapon = inventoryIndex;
    player->attackDamageMin = it->dmgMin;
    player->attackDamageMax = it->dmgMax;
    player->attackRange = it->range;
    player->attackInterval = it->attackInterval;
}

void Items_EquipArmor(Entity *player, int inventoryIndex) {
    if (inventoryIndex < 0 || inventoryIndex >= g_inventoryCount) return;
    Item *it = &g_inventory[inventoryIndex];
    if (it->kind != ITEM_ARMOR) return;
    g_equippedArmor = inventoryIndex;
    player->armor = it->armor;
}

static GroundDrop *FindFreeDrop(void) {
    for (int i = 0; i < MAX_DROPS; i++) {
        if (!g_drops[i].active) return &g_drops[i];
    }
    return NULL;
}

void Items_SpawnMonsterDrops(Vector2 pos, int monsterLevel) {
    GroundDrop *d = FindFreeDrop();
    if (d) {
        memset(d, 0, sizeof(GroundDrop));
        d->active = true;
        d->pos = (Vector2){ pos.x - 10, pos.y + 6 };
        d->gold = 8 + monsterLevel * 4 + GetRandomValue(0, monsterLevel * 3);
    }

    if (GetRandomValue(1, 100) <= 45) {
        d = FindFreeDrop();
        if (d) {
            memset(d, 0, sizeof(GroundDrop));
            d->active = true;
            d->pos = (Vector2){ pos.x + 12, pos.y - 4 };
            d->item = g_weaponTable[GetRandomValue(0, WEAPON_TABLE_COUNT - 1)];
        }
    } else if (GetRandomValue(1, 100) <= 25) {
        d = FindFreeDrop();
        if (d) {
            memset(d, 0, sizeof(GroundDrop));
            d->active = true;
            d->pos = (Vector2){ pos.x + 12, pos.y - 4 };
            d->item = g_armorTable[GetRandomValue(0, ARMOR_TABLE_COUNT - 1)];
        }
    }
}

void Items_UpdatePickup(Entity *player) {
    if (!player || !player->alive) return;
    for (int i = 0; i < MAX_DROPS; i++) {
        GroundDrop *d = &g_drops[i];
        if (!d->active) continue;
        float dx = d->pos.x - player->pos.x, dy = d->pos.y - player->pos.y;
        if (sqrtf(dx * dx + dy * dy) > PICKUP_RADIUS) continue;

        if (d->gold > 0) {
            g_gold += d->gold;
            d->active = false;
        } else if (d->item.kind != ITEM_NONE) {
            if (Items_AddToInventory(d->item)) {
                d->active = false;
            }
            // Inventory full: leave it on the ground.
        }
    }
}
