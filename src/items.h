#ifndef ITEMS_H
#define ITEMS_H

#include "raylib.h"
#include <stdbool.h>

struct Entity;

#define MAX_INVENTORY 16
#define MAX_DROPS 32

typedef enum {
    ITEM_NONE = 0,
    ITEM_WEAPON,
    ITEM_ARMOR,
    ITEM_MATERIAL // crafting stock (stacks): armor is crafted, not looted
} ItemKind;

typedef struct {
    ItemKind kind;
    char name[40];
    // Weapon fields. Damage ranges follow GW1's real max-damage tables
    // (sword 15-22, hammer 19-35, wand/staff 11-22) - in GW1 a weapon's
    // power comes from its TYPE, not its rarity.
    int dmgMin, dmgMax;
    float range;
    float attackInterval;
    // Armor field: GW1-style armor level (AL). 60 is the caster max.
    int armor;
    // Materials stack; everything else is count 1. Zero means 1 (older
    // saves and struct literals that never set it).
    int count;
} Item;

typedef struct {
    bool active;
    Vector2 pos;
    Item item;  // kind == ITEM_NONE for a pure gold drop
    int gold;
} GroundDrop;

extern Item g_inventory[MAX_INVENTORY];
extern int g_inventoryCount;
extern int g_gold;
extern int g_equippedWeapon; // index into g_inventory, -1 = none
extern int g_equippedArmor;

extern GroundDrop g_drops[MAX_DROPS];

// Wipes inventory, gold, equipment, and ground drops - the fresh-start
// state a New Game (or a save load about to re-fill the inventory)
// begins from.
void Items_Reset(void);

// Adds to inventory; returns false if full. Materials stack onto an
// existing pile of the same name instead of taking a new slot.
bool Items_AddToInventory(Item item);

// Crafting support: how many of a named material the player carries,
// and consuming n of them (shrinking or removing the pile).
int Items_CountMaterial(const char *name);
bool Items_ConsumeMaterial(const char *name, int n);

// Removes an item, keeping the equipped-item indices consistent.
// Refuses to remove something currently equipped (returns false).
bool Items_RemoveFromInventory(int inventoryIndex);

// What the merchant pays for an item - deliberately a fraction of buy
// prices, like GW1 merchants.
int Items_SellValue(const Item *item);

// Applies an inventory weapon/armor to the player's combat stats.
void Items_EquipWeapon(struct Entity *player, int inventoryIndex);
void Items_EquipArmor(struct Entity *player, int inventoryIndex);

// Rolls GW1-style loot for a killed monster: always some gold, a decent
// chance of a weapon or a crafting material. Armor never drops - like
// GW1, armor is only crafted (see the Armorer NPC).
void Items_SpawnMonsterDrops(Vector2 pos, int monsterLevel);

// Walk-over pickup: anything within reach of the player is collected.
void Items_UpdatePickup(struct Entity *player);

#endif
