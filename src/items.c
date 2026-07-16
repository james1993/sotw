#include "items.h"
#include "entity.h"
#include "raylib.h"
#include <math.h>
#include <string.h>
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
    { ITEM_WEAPON, "Long Sword",  15, 22, 28.0f,  1.33f, 0, 1, false },
    { ITEM_WEAPON, "War Hammer",  19, 35, 30.0f,  1.75f, 0, 1, false },
    { ITEM_WEAPON, "Fire Staff",  11, 22, 220.0f, 1.75f, 0, 1, false },
    { ITEM_WEAPON, "Smiting Rod", 11, 22, 160.0f, 1.75f, 0, 1, false },
};
#define WEAPON_TABLE_COUNT (int)(sizeof(g_weaponTable) / sizeof(g_weaponTable[0]))

// The crafting material Charr leave behind - the Armorer turns these
// (plus gold) into armor, GW1's craft-only armor economy in miniature.
static const Item g_charrHide = { ITEM_MATERIAL, "Charr Hide", 0, 0, 0, 0, 0, 1, false };

void Items_Reset(void) {
    g_inventoryCount = 0;
    g_gold = 0;
    g_equippedWeapon = -1;
    g_equippedArmor = -1;
    memset(g_drops, 0, sizeof(g_drops));
}

bool Items_AddToInventory(Item item) {
    if (item.count <= 0) item.count = 1;
    if (item.kind == ITEM_MATERIAL) {
        for (int i = 0; i < g_inventoryCount; i++) {
            if (g_inventory[i].kind == ITEM_MATERIAL &&
                strcmp(g_inventory[i].name, item.name) == 0) {
                g_inventory[i].count += item.count;
                return true;
            }
        }
    }
    if (g_inventoryCount >= MAX_INVENTORY) return false;
    g_inventory[g_inventoryCount++] = item;
    return true;
}

int Items_CountMaterial(const char *name) {
    for (int i = 0; i < g_inventoryCount; i++) {
        if (g_inventory[i].kind == ITEM_MATERIAL &&
            strcmp(g_inventory[i].name, name) == 0) {
            return g_inventory[i].count;
        }
    }
    return 0;
}

bool Items_ConsumeMaterial(const char *name, int n) {
    for (int i = 0; i < g_inventoryCount; i++) {
        Item *it = &g_inventory[i];
        if (it->kind != ITEM_MATERIAL || strcmp(it->name, name) != 0) continue;
        if (it->count < n) return false;
        it->count -= n;
        if (it->count <= 0) Items_RemoveFromInventory(i);
        return true;
    }
    return false;
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
    // Unidentified gear is nearly worthless to a merchant - identify
    // first if you want the real price, exactly GW1's incentive.
    if (item->kind == ITEM_WEAPON && item->unidentified) return 8;
    if (item->kind == ITEM_ARMOR) return 10 + item->armor / 2;
    if (item->kind == ITEM_WEAPON) return 15 + item->dmgMax / 2;
    if (item->kind == ITEM_MATERIAL) return 5; // per hide
    if (item->kind == ITEM_KIT_SALVAGE || item->kind == ITEM_KIT_ID) {
        return 2 * (item->count > 0 ? item->count : 1); // half-ish value per use
    }
    return 0;
}

const char *Items_DisplayName(const Item *item) {
    if (item->kind == ITEM_WEAPON && item->unidentified) return "Unidentified Weapon";
    return item->name;
}

int Items_UseKitOn(int kitIndex, int targetIndex) {
    if (kitIndex < 0 || kitIndex >= g_inventoryCount) return -2;
    if (targetIndex < 0 || targetIndex >= g_inventoryCount) return -2;
    if (kitIndex == targetIndex) return -2;

    Item *kit = &g_inventory[kitIndex];
    Item *target = &g_inventory[targetIndex];
    bool used = false;

    if (kit->kind == ITEM_KIT_ID) {
        if (target->kind == ITEM_WEAPON && target->unidentified) {
            target->unidentified = false;
            used = true;
        }
    } else if (kit->kind == ITEM_KIT_SALVAGE) {
        // Equipped gear can't be salvaged (Items_RemoveFromInventory
        // refuses anyway, but check first so the kit isn't spent).
        if ((target->kind == ITEM_WEAPON || target->kind == ITEM_ARMOR) &&
            targetIndex != g_equippedWeapon && targetIndex != g_equippedArmor) {
            Item hide = g_charrHide;
            hide.count = GetRandomValue(1, 3);
            if (Items_RemoveFromInventory(targetIndex)) {
                if (targetIndex < kitIndex) kitIndex--;
                Items_AddToInventory(hide);
                used = true;
            }
        }
    }

    if (!used) return -2;

    Item *k = &g_inventory[kitIndex];
    k->count--;
    if (k->count <= 0) {
        Items_RemoveFromInventory(kitIndex);
        return -1;
    }
    return kitIndex;
}

void Items_EquipWeapon(Entity *player, int inventoryIndex) {
    if (inventoryIndex < 0 || inventoryIndex >= g_inventoryCount) return;
    Item *it = &g_inventory[inventoryIndex];
    if (it->kind != ITEM_WEAPON) return;
    if (it->unidentified) return; // identify before wielding, like GW1
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

    // A weapon sometimes, a crafting hide often - never armor, which is
    // craft-only like GW1.
    if (GetRandomValue(1, 100) <= 30) {
        d = FindFreeDrop();
        if (d) {
            memset(d, 0, sizeof(GroundDrop));
            d->active = true;
            d->pos = (Vector2){ pos.x + 12, pos.y - 4 };
            d->item = g_weaponTable[GetRandomValue(0, WEAPON_TABLE_COUNT - 1)];
            d->item.count = 1;
            d->item.unidentified = true; // looted weapons need an ID kit
        }
    } else if (GetRandomValue(1, 100) <= 55) {
        d = FindFreeDrop();
        if (d) {
            memset(d, 0, sizeof(GroundDrop));
            d->active = true;
            d->pos = (Vector2){ pos.x + 12, pos.y - 4 };
            d->item = g_charrHide;
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
