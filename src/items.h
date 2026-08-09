#ifndef ITEMS_H
#define ITEMS_H

#include "raylib.h"
#include <stdbool.h>

struct Entity;

// GW1's Backpack holds 20; bags add to it. The cap here is what the
// arrays are sized for - the CARRYABLE count is Items_Capacity(), which
// is the backpack plus whatever bags are equipped.
#define MAX_INVENTORY 40
#define INVENTORY_BASE_SLOTS 20
#define MAX_DROPS 32

typedef enum {
    ITEM_NONE = 0,
    ITEM_WEAPON,
    ITEM_ARMOR,
    ITEM_OFFHAND,      // shield or focus; blocked by a two-handed weapon
    ITEM_BAG,          // equipping one adds `count` inventory slots
    ITEM_MATERIAL,     // crafting stock (stacks): armor is crafted, not looted
    ITEM_KIT_SALVAGE,  // breaks gear into materials; count = uses left
    ITEM_KIT_ID,       // reveals unidentified weapons; count = uses left
    ITEM_RUNE,         // an armour upgrade: applied to an equipped piece
    ITEM_INSIGNIA,     // an armour upgrade: adds armour to a piece
    ITEM_DYE           // recolours the character's armour
} ItemKind;

// GW1 dresses a character in five armour pieces, not one "armor" item,
// and that is the whole reason armour is a collection game: you upgrade
// a set a piece at a time. The weapon and offhand share the enum so one
// table describes everything a character wears.
typedef enum {
    EQUIP_HEAD = 0,
    EQUIP_CHEST,
    EQUIP_ARMS,
    EQUIP_LEGS,
    EQUIP_FEET,
    EQUIP_WEAPON,
    EQUIP_OFFHAND,
    EQUIP_BAG,        // one carried bag; adds slots rather than stats
    EQUIP_SLOT_COUNT
} EquipSlot;

#define EQUIP_ARMOR_PIECES 5   // HEAD..FEET - the ones that make up AL

// GW1 item rarity, shown by the colour of an item's name. White items
// carry no mods; from blue up they always have at least one, and the
// tier sets how good the values roll - blue low, purple mid, gold max.
// Green is off the spectrum: unique boss drops with fixed perfect mods.
typedef enum {
    RARITY_WHITE = 0,  // Common - no modifiers
    RARITY_BLUE,       // Magical - one low-range mod
    RARITY_PURPLE,     // Rare - mid-range mods
    RARITY_GOLD,       // Rare - max/near-max mods, the premium prefixes
    RARITY_GREEN       // Unique - fixed perfect mods, from bosses
} ItemRarity;

const char *Items_SlotName(EquipSlot slot);

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
    // On an armour PIECE this is the rating of the whole set it belongs
    // to - wearing all five pieces of an "AL 60" set gives AL 60, and a
    // missing piece leaves that part of you unprotected.
    int armor;
    // Which slot this occupies. Meaningful for ITEM_ARMOR (HEAD..FEET);
    // weapons, offhands and bags are routed by kind.
    EquipSlot slot;
    // Two-handed weapons (bows, staves, hammers) rule out an offhand.
    bool twoHanded;
    // Materials stack and kits carry uses here; everything else is
    // count 1. Zero means 1 (older saves and short struct literals).
    int count;
    // GW1's identification loop: dropped weapons come up unidentified -
    // masked name, hidden stats, can't be equipped, nearly worthless -
    // until an Identification Kit reveals them.
    bool unidentified;

    // --- Weapon upgrades (mods) --------------------------------------
    // GW1 weapons carry a prefix and a suffix upgrade, and the loot game
    // is really about the mods, not the base type. A dropped weapon rolls
    // these; identifying it reveals them (and the fuller name). While the
    // weapon is held, Items_RecomputeEquipped folds them into the wielder.
    int rarity;         // ItemRarity - drives name colour, mods and value
    int modHealth;      // "of Fortitude": +health while wielded (10-30)
    int modArmorPen;    // "Sundering": % armor penetration on basic attacks
    int modLifesteal;   // "Vampiric": health stolen per basic hit (1-5); -1 hp regen
    int modEnergyGain;  // "Zealous": energy gained per basic hit (1); -1 energy regen
    int modArmor;       // "of Shelter": +armor while wielded (4-7)
    int modEnchantPct;  // "of Enchanting": % longer enchantments (10-20)

    // --- Armor upgrades (runes + insignia) ---------------------------
    // Each armour piece can hold one rune and one insignia. A rune boosts
    // an attribute (at a health cost) or health (Vigor); an insignia adds
    // armour. Applied to an equipped piece with a rune/insignia item, the
    // way GW1 slots them in.
    int runeAttr;      // AttributeKind the rune boosts, or -1 for none/Vigor
    int runeAttrBonus; // +ranks from the rune
    int runeHealth;    // health delta: Vigor is +, an attribute rune is -
    int insigniaArmor; // flat armour the insignia adds

    // ITEM_DYE: the colour it paints the armour.
    Color dyeColor;
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
// Inventory indices of what is worn, -1 = empty. One array rather than
// a named variable per slot, so adding a slot doesn't mean touching
// every site that iterates equipment.
extern int g_equipped[EQUIP_SLOT_COUNT];

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

// What to show for an item: its name, or the GW1-style mask while a
// weapon is unidentified. Use everywhere an item is displayed.
const char *Items_DisplayName(const Item *item);

// The colour an item's name is drawn in, from its rarity.
Color Items_RarityColor(const Item *item);

// Applies a kit to an inventory item, GW1's click-kit-then-item flow.
// Identification reveals an unidentified weapon; salvage destroys a
// non-equipped weapon/armor and yields 1-3 Charr Hides. One use per
// application; the kit disappears at zero uses. Returns the kit's new
// inventory index (indices shift when salvage removes an item), -1
// when the kit was consumed by this use, or -2 when nothing happened.
int Items_UseKitOn(int kitIndex, int targetIndex);

// Removes an item, keeping the equipped-item indices consistent.
// Refuses to remove something currently equipped (returns false).
bool Items_RemoveFromInventory(int inventoryIndex);

// What the merchant pays for an item - deliberately a fraction of buy
// prices, like GW1 merchants.
int Items_SellValue(const Item *item);

// Equips an inventory item into whichever slot it belongs in, and
// recomputes the player's armour and weapon stats. Refuses an offhand
// while a two-handed weapon is held (and unequips the offhand when a
// two-hander goes on). Returns false when nothing could be equipped.
bool Items_Equip(struct Entity *player, int inventoryIndex);

// Empties a slot and recomputes. Returns false if it was already empty.
bool Items_Unequip(struct Entity *player, EquipSlot slot);

// True when this inventory index is worn in any slot.
bool Items_IsEquipped(int inventoryIndex);

// Recomputes armour from the five worn pieces plus any offhand bonus,
// and weapon stats from the held weapon. Called by the equip paths;
// exposed because loading a save equips several things at once.
void Items_RecomputeEquipped(struct Entity *player);

// How many inventory slots the character actually has: the backpack
// plus whatever the equipped bag adds.
int Items_Capacity(void);

// Rolls GW1-style loot for a killed monster: always some gold, a decent
// chance of a weapon or a crafting material. Armor never drops - like
// GW1, armor is only crafted (see the Armorer NPC). Hides only come off
// creatures that have them - Charr, not Devourers.
void Items_SpawnMonsterDrops(Vector2 pos, int monsterLevel, int species);

// Drops a GREEN unique weapon - fixed perfect mods, named for the boss
// that dropped it. Called from the boss death path, GW1's "greens come
// off bosses" rule.
void Items_SpawnUnique(Vector2 pos, const char *bossName);

// Walk-over pickup: anything within reach of the player is collected.
void Items_UpdatePickup(struct Entity *player);

#endif
