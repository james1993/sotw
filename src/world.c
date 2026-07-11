#include "world.h"
#include "entity.h"
#include "items.h"
#include "attributes.h"
#include <math.h>
#include <string.h>

#define PLAYER_INDEX 0
#define PORTAL_TRIGGER_RADIUS 40.0f
#define PORTAL_COOLDOWN 1.5f

static GameMode g_mode = MODE_OUTPOST;
static bool g_thomHired = false;
static float g_portalCooldown = 0.0f;

static Vector2 g_portalPos;
static const char *g_portalLabel = "";
static const char *g_zoneName = "";

GameMode World_GetMode(void) { return g_mode; }
const char *World_GetZoneName(void) { return g_zoneName; }
bool World_IsThomHired(void) { return g_thomHired; }
void World_SetThomHired(bool hired) { g_thomHired = hired; }

void World_GetPortal(Vector2 *pos, const char **label) {
    if (pos) *pos = g_portalPos;
    if (label) *label = g_portalLabel;
}

// Strips combat/zone-transient state off the persistent player entity
// when crossing a portal; progression (level/xp/attributes) and the
// global inventory survive untouched.
static void ResetPlayerTransientState(Entity *p, Vector2 entryPos) {
    p->pos = entryPos;
    p->moveTarget = entryPos;
    p->hasMoveTarget = false;
    p->targetIndex = -1;
    p->castingSlot = -1;
    p->lastCastSkillSlot = -1;
    p->postCastDisplayTimer = 0.0f;
    p->interruptFlashTimer = 0.0f;
    p->adrenaline = 0;
    for (int i = 0; i < MAX_ACTIVE_EFFECTS; i++) p->effects[i].active = false;
    for (int i = 0; i < SKILL_BAR_SIZE; i++) p->skillRecharge[i] = 0.0f;
}

static void SpawnVekk(Vector2 pos) {
    int idx = Entity_Spawn(ENT_HERO, "Vekk", 0, pos, (Color){ 60, 120, 220, 255 });
    Entity *hero = Entity_Get(idx);
    hero->primaryProfession = PROF_ELEMENTALIST;
    hero->secondaryProfession = PROF_MONK;
    hero->level = 5;
    hero->maxHp = hero->hp = 100 + 20 * (hero->level - 1);
    hero->maxEnergy = hero->energy = 50;
    hero->attackRange = 220.0f; // caster keeps distance
    hero->attributeRank[ATTR_FIRE_MAGIC] = 4;
    hero->attributeRank[ATTR_ENERGY_STORAGE] = 3;
    hero->skillBar[0] = 4; // Fire Bolt
    hero->skillBar[1] = 5; // Cinder Storm
    hero->skillBar[2] = 6; // Mind Sear (energy management)
}

// Little Thom, the pre-Searing Warrior henchman. As a party member he
// fights with a fixed Warrior bar - fixed skill sets being exactly what
// separates henchmen from heroes in GW1
// (docs/research/gw1-mechanics.md #10).
static void SpawnThomCompanion(Vector2 pos) {
    int idx = Entity_Spawn(ENT_HERO, "Little Thom", 0, pos, (Color){ 170, 80, 60, 255 });
    Entity *thom = Entity_Get(idx);
    thom->primaryProfession = PROF_WARRIOR;
    thom->secondaryProfession = PROF_MONK;
    thom->level = 5;
    thom->maxHp = thom->hp = 100 + 20 * (thom->level - 1);
    thom->armor = 80; // warriors wear heavy armor
    thom->attributeRank[ATTR_STRENGTH] = 4;
    thom->attributeRank[ATTR_TACTICS] = 3;
    thom->skillBar[0] = 0; // Gash
    thom->skillBar[1] = 1; // Rush Strike
    thom->skillBar[2] = 2; // Battle Cry
}

static void SpawnNpc(const char *name, NpcRole role, Vector2 pos, Color color) {
    int idx = Entity_Spawn(ENT_NPC, name, 0, pos, color);
    Entity *npc = Entity_Get(idx);
    npc->npcRole = role;
}

static void SpawnCharr(const char *name, Vector2 pos, int level, int hp, int armor,
                       float aggro, int strengthRank, bool withHowl) {
    int idx = Entity_Spawn(ENT_MONSTER, name, 1, pos, (Color){ 100, 90, 80, 255 });
    Entity *m = Entity_Get(idx);
    m->level = level;
    m->maxHp = m->hp = hp;
    m->maxEnergy = m->energy = 20;
    m->armor = armor;
    m->aggroRange = aggro;
    m->leashRange = aggro * 2.5f;
    m->attributeRank[ATTR_STRENGTH] = strengthRank;
    if (withHowl) {
        m->skillBar[0] = 10; // Feral Howl (self-heal - interrupt it!)
        m->skillBar[1] = 8;  // Claw Swipe
    } else {
        m->skillBar[0] = 8;  // Claw Swipe
    }
}

// Rebuilds the entity array for a zone while carrying the player
// (slot 0) across. GW1 semantics: explorables are a fresh instance on
// every entry; returning to an outpost fully restores the party.
static void LoadZone(GameMode mode, Vector2 playerEntry) {
    Entity saved = g_entities[PLAYER_INDEX];
    g_entityCount = 0;
    memset(g_drops, 0, sizeof(g_drops)); // ground loot doesn't survive rezoning, like GW1

    g_entities[g_entityCount++] = saved;
    Entity *player = Entity_Get(PLAYER_INDEX);
    ResetPlayerTransientState(player, playerEntry);

    g_mode = mode;
    g_portalCooldown = PORTAL_COOLDOWN;

    if (mode == MODE_OUTPOST) {
        g_zoneName = "Ashford Camp";
        g_portalPos = (Vector2){ 260, 0 };
        g_portalLabel = "To Ashford Plains";

        // Outposts restore the party completely, GW1-style.
        player->hp = player->maxHp;
        player->energy = player->maxEnergy;

        SpawnVekk((Vector2){ -50, 50 });
        if (g_thomHired) {
            SpawnThomCompanion((Vector2){ 40, 70 });
        } else {
            SpawnNpc("Little Thom", NPC_HENCHMAN, (Vector2){ 40, 110 }, (Color){ 170, 80, 60, 255 });
        }
        SpawnNpc("Captain Osric", NPC_QUEST_GIVER, (Vector2){ -120, -60 }, (Color){ 90, 170, 90, 255 });
        SpawnNpc("Merchant", NPC_MERCHANT, (Vector2){ 90, -90 }, (Color){ 90, 170, 90, 255 });
    } else {
        g_zoneName = "Ashford Plains";
        g_portalPos = (Vector2){ -420, 0 };
        g_portalLabel = "To Ashford Camp";

        SpawnVekk((Vector2){ playerEntry.x - 40, playerEntry.y + 50 });
        if (g_thomHired) SpawnThomCompanion((Vector2){ playerEntry.x + 30, playerEntry.y + 60 });

        // The Charr, spaced further apart than any single aggro bubble so
        // pulling one at a time stays a real option.
        SpawnCharr("Charr Brute", (Vector2){ 260, 20 }, 5, 220, 60, 130.0f, 8, true);
        SpawnCharr("Charr Grunt", (Vector2){ 440, 150 }, 2, 140, 40, 120.0f, 6, false);
        SpawnCharr("Charr Grunt", (Vector2){ 420, -170 }, 2, 140, 40, 120.0f, 6, false);
        SpawnCharr("Charr Grunt", (Vector2){ 640, -40 }, 3, 160, 40, 120.0f, 6, false);
        SpawnCharr("Charr Stalker", (Vector2){ 700, 190 }, 4, 180, 50, 130.0f, 7, true);
    }
}

void World_Init(void) {
    // The persistent player, created exactly once; every zone load
    // carries this entity across. Monk primary / Elementalist secondary
    // (docs/research/gw1-mechanics.md #4).
    int playerIdx = Entity_Spawn(ENT_PLAYER, "Player (Mo/E)", 0, (Vector2){ 0, 0 }, (Color){ 220, 200, 120, 255 });
    Entity *player = Entity_Get(playerIdx);
    player->primaryProfession = PROF_MONK;
    player->secondaryProfession = PROF_ELEMENTALIST;
    player->level = 5;
    player->maxHp = player->hp = 100 + 20 * (player->level - 1); // GW1: +20 HP per level
    player->maxEnergy = player->energy = 30;
    // Level 5 grants 20 attribute points; these ranks spend 17 per the
    // GW1 cost table, leaving 3 free for the attributes panel (K).
    player->attributeRank[ATTR_HEALING_PRAYERS] = 4;
    player->attributeRank[ATTR_SMITING_PRAYERS] = 3;
    player->attributeRank[ATTR_DIVINE_FAVOR] = 1;
    player->attributePoints = 3;
    player->skillBar[0] = 11; // Orison of Healing
    player->skillBar[1] = 12; // Banish
    player->skillBar[2] = 13; // Smite
    player->skillBar[3] = 14; // Bane Signet
    player->skillBar[4] = 4;  // Fire Bolt (Elementalist secondary)

    // Starting equipment, GW1-style fixed-power items.
    Item startRod = { ITEM_WEAPON, "Smiting Rod", 11, 22, 160.0f, 1.75f, 0 };
    Item startRaiment = { ITEM_ARMOR, "Monk Raiment (AL 30)", 0, 0, 0, 0, 30 };
    Items_AddToInventory(startRod);
    Items_AddToInventory(startRaiment);
    Items_EquipWeapon(player, 0);
    Items_EquipArmor(player, 1);

    LoadZone(MODE_OUTPOST, (Vector2){ 0, 0 });
}

void World_Update(Entity *player, float dt) {
    if (g_portalCooldown > 0.0f) {
        g_portalCooldown -= dt;
        return;
    }
    if (!player || !player->alive) return;

    float dx = player->pos.x - g_portalPos.x;
    float dy = player->pos.y - g_portalPos.y;
    if (sqrtf(dx * dx + dy * dy) > PORTAL_TRIGGER_RADIUS) return;

    if (g_mode == MODE_OUTPOST) {
        // Enter the explorable next to its return portal, offset past the
        // trigger radius so we don't immediately bounce back.
        LoadZone(MODE_EXPLORABLE, (Vector2){ -320, 0 });
    } else {
        LoadZone(MODE_OUTPOST, (Vector2){ 160, 0 });
    }
}
