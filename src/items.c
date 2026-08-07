#include "items.h"
#include "character.h"
#include "audio.h"
#include "entity.h"
#include "raylib.h"
#include <math.h>
#include <string.h>
#include <string.h>

#define PICKUP_RADIUS 30.0f

Item g_inventory[MAX_INVENTORY];
int g_inventoryCount = 0;
int g_gold = 0;
int g_equipped[EQUIP_SLOT_COUNT];

GroundDrop g_drops[MAX_DROPS];

// Weapon damage ranges are GW1's actual per-type maximums.
static const Item g_weaponTable[] = {
    { .kind = ITEM_WEAPON, .name = "Long Sword", .dmgMin = 15, .dmgMax = 22,
      .range = 28.0f, .attackInterval = 1.33f, .count = 1 },
    { .kind = ITEM_WEAPON, .name = "War Hammer", .dmgMin = 19, .dmgMax = 35,
      .range = 30.0f, .attackInterval = 1.75f, .count = 1, .twoHanded = true },
    { .kind = ITEM_WEAPON, .name = "Fire Staff", .dmgMin = 11, .dmgMax = 22,
      .range = 220.0f, .attackInterval = 1.75f, .count = 1, .twoHanded = true },
    { .kind = ITEM_WEAPON, .name = "Smiting Rod", .dmgMin = 11, .dmgMax = 22,
      .range = 160.0f, .attackInterval = 1.75f, .count = 1 },
};
#define WEAPON_TABLE_COUNT (int)(sizeof(g_weaponTable) / sizeof(g_weaponTable[0]))

// The crafting material Charr leave behind - the Armorer turns these
// (plus gold) into armor, GW1's craft-only armor economy in miniature.
static const Item g_charrHide = { .kind = ITEM_MATERIAL, .name = "Charr Carving", .count = 1 };
static const Item g_skaleFin  = { .kind = ITEM_MATERIAL, .name = "Skale Fin",  .count = 1 };
static const Item g_grawlNecklace = { .kind = ITEM_MATERIAL, .name = "Grawl Necklace", .count = 1 };

const char *Items_SlotName(EquipSlot slot) {
    switch (slot) {
        case EQUIP_HEAD:    return "Head";
        case EQUIP_CHEST:   return "Chest";
        case EQUIP_ARMS:    return "Arms";
        case EQUIP_LEGS:    return "Legs";
        case EQUIP_FEET:    return "Feet";
        case EQUIP_WEAPON:  return "Weapon";
        case EQUIP_OFFHAND: return "Offhand";
        case EQUIP_BAG:     return "Bag";
        default:            return "";
    }
}

void Items_Reset(void) {
    g_inventoryCount = 0;
    g_gold = 0;
    for (int i = 0; i < EQUIP_SLOT_COUNT; i++) g_equipped[i] = -1;
    memset(g_drops, 0, sizeof(g_drops));
}

int Items_Capacity(void) {
    int cap = INVENTORY_BASE_SLOTS;
    int bag = g_equipped[EQUIP_BAG];
    if (bag >= 0 && bag < g_inventoryCount && g_inventory[bag].kind == ITEM_BAG) {
        cap += g_inventory[bag].count;
    }
    return cap > MAX_INVENTORY ? MAX_INVENTORY : cap;
}

bool Items_IsEquipped(int inventoryIndex) {
    if (inventoryIndex < 0) return false;
    for (int i = 0; i < EQUIP_SLOT_COUNT; i++) {
        if (g_equipped[i] == inventoryIndex) return true;
    }
    return false;
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
    if (g_inventoryCount >= Items_Capacity()) return false;
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
    if (Items_IsEquipped(inventoryIndex)) return false;

    for (int i = inventoryIndex; i < g_inventoryCount - 1; i++) {
        g_inventory[i] = g_inventory[i + 1];
    }
    g_inventoryCount--;
    // Everything worn that sat after the hole shifts down with it.
    for (int i = 0; i < EQUIP_SLOT_COUNT; i++) {
        if (g_equipped[i] > inventoryIndex) g_equipped[i]--;
    }
    return true;
}

int Items_SellValue(const Item *item) {
    // Unidentified gear is nearly worthless to a merchant - identify
    // first if you want the real price, exactly GW1's incentive.
    if (item->kind == ITEM_WEAPON && item->unidentified) return 8;
    if (item->kind == ITEM_ARMOR) return 10 + item->armor / 2;
    if (item->kind == ITEM_OFFHAND) return 20 + item->armor * 4;
    if (item->kind == ITEM_BAG) return 15 + item->count * 2;
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
        if ((target->kind == ITEM_WEAPON || target->kind == ITEM_ARMOR ||
             target->kind == ITEM_OFFHAND) && !Items_IsEquipped(targetIndex)) {
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

void Items_RecomputeEquipped(Entity *player) {
    if (!player) return;

    // Armour: each of the five pieces protects its share of you, so a
    // full matching "AL 60" set reads as AL 60 and a missing piece
    // leaves a real hole. This is GW1's per-piece armour condensed into
    // one number the damage formula can use.
    int total = 0;
    for (int slot = 0; slot < EQUIP_ARMOR_PIECES; slot++) {
        int idx = g_equipped[slot];
        if (idx < 0 || idx >= g_inventoryCount) continue;
        if (g_inventory[idx].kind != ITEM_ARMOR) continue;
        total += g_inventory[idx].armor;
    }
    player->armor = total / EQUIP_ARMOR_PIECES;

    // A shield adds flat AL on top, the reason a one-handed set is not
    // simply worse than a two-hander.
    int off = g_equipped[EQUIP_OFFHAND];
    if (off >= 0 && off < g_inventoryCount && g_inventory[off].kind == ITEM_OFFHAND) {
        player->armor += g_inventory[off].armor;
    }

    int wep = g_equipped[EQUIP_WEAPON];
    if (wep >= 0 && wep < g_inventoryCount && g_inventory[wep].kind == ITEM_WEAPON) {
        const Item *it = &g_inventory[wep];
        player->attackDamageMin = it->dmgMin;
        player->attackDamageMax = it->dmgMax;
        player->attackRange = it->range;
        player->attackInterval = it->attackInterval;
    } else {
        // Bare hands: GW1 lets you fight unarmed, badly.
        player->attackDamageMin = 3;
        player->attackDamageMax = 6;
        player->attackRange = 28.0f;
        player->attackInterval = 1.33f;
    }
}

static bool WeaponIsTwoHanded(void) {
    int wep = g_equipped[EQUIP_WEAPON];
    if (wep < 0 || wep >= g_inventoryCount) return false;
    return g_inventory[wep].kind == ITEM_WEAPON && g_inventory[wep].twoHanded;
}

bool Items_Unequip(Entity *player, EquipSlot slot) {
    if (slot < 0 || slot >= EQUIP_SLOT_COUNT) return false;
    if (g_equipped[slot] < 0) return false;
    g_equipped[slot] = -1;
    Items_RecomputeEquipped(player);
    return true;
}

bool Items_Equip(Entity *player, int inventoryIndex) {
    if (!player) return false;
    if (inventoryIndex < 0 || inventoryIndex >= g_inventoryCount) return false;
    Item *it = &g_inventory[inventoryIndex];
    if (it->unidentified) return false; // identify before wielding, like GW1

    EquipSlot slot;
    switch (it->kind) {
        case ITEM_WEAPON:  slot = EQUIP_WEAPON; break;
        case ITEM_OFFHAND: slot = EQUIP_OFFHAND; break;
        case ITEM_BAG:     slot = EQUIP_BAG; break;
        case ITEM_ARMOR:
            slot = it->slot;
            if (slot < EQUIP_HEAD || slot > EQUIP_FEET) slot = EQUIP_CHEST;
            break;
        default: return false;
    }

    // A two-hander needs both hands: taking one up puts the offhand
    // away, and an offhand can't go on while one is held.
    if (slot == EQUIP_OFFHAND && WeaponIsTwoHanded()) return false;
    if (slot == EQUIP_WEAPON && it->twoHanded) g_equipped[EQUIP_OFFHAND] = -1;

    // Unequipping a bag that still has things in it would lose them.
    if (slot == EQUIP_BAG && g_equipped[EQUIP_BAG] != inventoryIndex) {
        int capWithout = INVENTORY_BASE_SLOTS + it->count;
        if (capWithout > MAX_INVENTORY) capWithout = MAX_INVENTORY;
        if (g_inventoryCount > capWithout) return false;
    }

    g_equipped[slot] = inventoryIndex;
    Items_RecomputeEquipped(player);
    return true;
}

static GroundDrop *FindFreeDrop(void) {
    for (int i = 0; i < MAX_DROPS; i++) {
        if (!g_drops[i].active) return &g_drops[i];
    }
    return NULL;
}

// GW1 assigns each drop to one member of the party, so what YOU see on
// the ground is divided by how many of you there are: a solo run drops
// everything to you, a full party drops you roughly an Nth. This is why
// henchmen are a real cost rather than free help, and it is what stops
// "bring everyone, always" from being the only sane answer.
//
// Gold is not divided - GW1 splits gold drops across the party instead
// of assigning them, so the player's share shrinks rather than
// vanishing. That is applied to the amount, below.
static int PartySize(void) {
    int n = 0;
    for (int i = 0; i < g_entityCount; i++) {
        const Entity *e = &g_entities[i];
        if (!e->alive) continue;
        if (e->team != 0) continue;
        if (e->kind == ENT_PLAYER || e->kind == ENT_HERO) n++;
    }
    return n < 1 ? 1 : n;
}

// True when this drop falls to the player rather than to a party member.
static bool DropIsMine(int partySize) {
    return GetRandomValue(1, partySize) == 1;
}

void Items_SpawnMonsterDrops(Vector2 pos, int monsterLevel, int species) {
    int party = PartySize();

    GroundDrop *d = FindFreeDrop();
    if (d) {
        memset(d, 0, sizeof(GroundDrop));
        d->active = true;
        d->pos = (Vector2){ pos.x - 10, pos.y + 6 };
        d->gold = 8 + monsterLevel * 4 + GetRandomValue(0, monsterLevel * 3);
        // Gold is SPLIT rather than assigned, so a party always leaves
        // you something - just less of it.
        d->gold /= party;
        if (d->gold < 1) d->gold = 1;
        // Reforged Mode's 5% gold bonus, applied where gold is minted
        // rather than where it's picked up - so what you see on the
        // ground is what you get.
        if (Character_IsReforged()) d->gold = d->gold * 105 / 100;
    }

    // A weapon sometimes, a crafting hide often - never armor, which is
    // craft-only like GW1.
    if (GetRandomValue(1, 100) <= 30 && DropIsMine(party)) {
        d = FindFreeDrop();
        if (d) {
            memset(d, 0, sizeof(GroundDrop));
            d->active = true;
            d->pos = (Vector2){ pos.x + 12, pos.y - 4 };
            d->item = g_weaponTable[GetRandomValue(0, WEAPON_TABLE_COUNT - 1)];
            d->item.count = 1;
            d->item.unidentified = true; // looted weapons need an ID kit
        }
    } else {
        // Crafting materials come off the body they'd come off in the
        // fiction: hides from Charr, fins from skale. Quests that ask
        // for a material are therefore also telling you where to hunt.
        const Item *material = NULL;
        if (species == SPECIES_CHARR) material = &g_charrHide;
        else if (species == SPECIES_SKALE) material = &g_skaleFin;
        else if (species == SPECIES_GRAWL) material = &g_grawlNecklace;

        if (material && GetRandomValue(1, 100) <= 55 && DropIsMine(party)) {
            d = FindFreeDrop();
            if (d) {
                memset(d, 0, sizeof(GroundDrop));
                d->active = true;
                d->pos = (Vector2){ pos.x + 12, pos.y - 4 };
                d->item = *material;
            }
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
            Audio_Play(SFX_LOOT);
        } else if (d->item.kind != ITEM_NONE) {
            if (Items_AddToInventory(d->item)) {
                d->active = false;
                Audio_Play(SFX_LOOT);
            }
            // Inventory full: leave it on the ground.
        }
    }
}
