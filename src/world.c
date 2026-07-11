#include "world.h"
#include "entity.h"
#include "items.h"
#include "attributes.h"
#include "projectile.h"
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

static Vector2 g_shrinePos;
static bool g_hasShrine = false;
static float g_wipeTimer = 0.0f;
static float g_autoResTimer = 0.0f;

GameMode World_GetMode(void) { return g_mode; }
const char *World_GetZoneName(void) { return g_zoneName; }
bool World_IsThomHired(void) { return g_thomHired; }
void World_SetThomHired(bool hired) { g_thomHired = hired; }

void World_GetPortal(Vector2 *pos, const char **label) {
    if (pos) *pos = g_portalPos;
    if (label) *label = g_portalLabel;
}

bool World_GetShrine(Vector2 *pos) {
    if (pos) *pos = g_shrinePos;
    return g_hasShrine;
}

void World_DismissHenchman(Entity *henchman) {
    if (!henchman || !henchman->isHenchman) return;
    if (g_mode != MODE_OUTPOST) return; // GW1: party changes only in outposts

    g_thomHired = false;
    henchman->kind = ENT_NPC;
    henchman->npcRole = NPC_HENCHMAN;
    henchman->isHenchman = false;
    henchman->alive = true;
    henchman->hp = henchman->maxHp;
    henchman->targetIndex = -1;
    henchman->hasMoveTarget = false;
}

// Strips combat/zone-transient state off the persistent player entity
// when crossing a portal; progression (level/xp/attributes) and the
// global inventory survive untouched. Death penalty clears on rezoning,
// exactly like GW1.
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
    p->alive = true;
    p->deathPenalty = 0;
    Entity_RecomputePenalizedStats(p);
    for (int i = 0; i < MAX_ACTIVE_EFFECTS; i++) p->effects[i].active = false;
    for (int i = 0; i < SKILL_BAR_SIZE; i++) p->skillRecharge[i] = 0.0f;
}

static void SpawnVekk(Vector2 pos) {
    int idx = Entity_Spawn(ENT_HERO, "Vekk", 0, pos, (Color){ 60, 120, 220, 255 });
    Entity *hero = Entity_Get(idx);
    hero->primaryProfession = PROF_ELEMENTALIST;
    hero->secondaryProfession = PROF_MONK;
    hero->level = 5;
    hero->baseMaxHp = 100 + 20 * (hero->level - 1);
    hero->baseMaxEnergy = 50;
    Entity_RecomputePenalizedStats(hero);
    hero->hp = hero->maxHp;
    hero->energy = hero->maxEnergy;
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
    thom->isHenchman = true;
    thom->primaryProfession = PROF_WARRIOR;
    thom->secondaryProfession = PROF_MONK;
    thom->level = 5;
    thom->baseMaxHp = 100 + 20 * (thom->level - 1);
    thom->baseMaxEnergy = 20;
    Entity_RecomputePenalizedStats(thom);
    thom->hp = thom->maxHp;
    thom->energy = thom->maxEnergy;
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
    Projectile_ClearAll();               // and neither do shots in flight

    g_entities[g_entityCount++] = saved;
    Entity *player = Entity_Get(PLAYER_INDEX);
    ResetPlayerTransientState(player, playerEntry);

    g_mode = mode;
    g_portalCooldown = PORTAL_COOLDOWN;

    g_wipeTimer = 0.0f;
    g_autoResTimer = 0.0f;

    if (mode == MODE_OUTPOST) {
        g_zoneName = "Ashford Camp";
        g_portalPos = (Vector2){ 260, 0 };
        g_portalLabel = "To Ashford Plains";
        g_hasShrine = false;

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
        g_shrinePos = (Vector2){ -320, 140 };
        g_hasShrine = true;

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
    player->baseMaxHp = 100 + 20 * (player->level - 1); // GW1: +20 HP per level
    player->baseMaxEnergy = 30;
    Entity_RecomputePenalizedStats(player);
    player->hp = player->maxHp;
    player->energy = player->maxEnergy;
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
    if (!player) return;

    // --- Death & resurrection bookkeeping (explorable only) ---
    if (g_mode == MODE_EXPLORABLE) {
        int aliveCount = 0, deadCount = 0;
        for (int i = 0; i < g_entityCount; i++) {
            Entity *e = &g_entities[i];
            if (e->team != 0 || (e->kind != ENT_PLAYER && e->kind != ENT_HERO)) continue;
            if (e->alive) aliveCount++; else deadCount++;
        }

        if (aliveCount == 0 && deadCount > 0) {
            // Full party wipe: after a beat, everyone respawns at the
            // resurrection shrine, carrying the death penalty their
            // deaths already stacked - straight GW1.
            g_wipeTimer += dt;
            if (g_wipeTimer >= 2.5f) {
                g_wipeTimer = 0.0f;
                int slot = 0;
                for (int i = 0; i < g_entityCount; i++) {
                    Entity *e = &g_entities[i];
                    if (e->team != 0 || (e->kind != ENT_PLAYER && e->kind != ENT_HERO)) continue;
                    e->alive = true;
                    e->hp = e->maxHp; // already penalized by DP
                    e->energy = e->maxEnergy;
                    e->pos = (Vector2){ g_shrinePos.x + (slot % 2) * 34.0f,
                                        g_shrinePos.y + (slot / 2) * 34.0f };
                    e->moveTarget = e->pos;
                    e->hasMoveTarget = false;
                    e->targetIndex = -1;
                    e->castingSlot = -1;
                    for (int j = 0; j < MAX_ACTIVE_EFFECTS; j++) e->effects[j].active = false;
                    slot++;
                }
            }
            return; // no portal use while wiped
        }

        if (deadCount > 0) {
            // Some of the party is down but the fight was survived: once
            // no monster is aggroed, the survivors revive the fallen after
            // a few seconds. This stands in for GW1's res signets and
            // hero res skills until dead-ally targeting exists.
            bool anyAggro = false;
            Entity *anchor = NULL; // a living member the fallen revive beside
            for (int i = 0; i < g_entityCount; i++) {
                Entity *e = &g_entities[i];
                if (e->kind == ENT_MONSTER && e->alive && e->aggroed) anyAggro = true;
                if (e->team == 0 && e->alive && !anchor &&
                    (e->kind == ENT_PLAYER || e->kind == ENT_HERO)) anchor = e;
            }
            if (anyAggro || !anchor) {
                g_autoResTimer = 0.0f;
            } else {
                g_autoResTimer += dt;
                if (g_autoResTimer >= 5.0f) {
                    g_autoResTimer = 0.0f;
                    for (int i = 0; i < g_entityCount; i++) {
                        Entity *e = &g_entities[i];
                        if (e->team != 0 || (e->kind != ENT_PLAYER && e->kind != ENT_HERO) || e->alive) continue;
                        e->alive = true;
                        e->hp = e->maxHp / 2; // revived weakened, like a res signet
                        e->energy = e->maxEnergy / 2;
                        e->pos = (Vector2){ anchor->pos.x + 30.0f, anchor->pos.y + 30.0f };
                        e->targetIndex = -1;
                        e->castingSlot = -1;
                        for (int j = 0; j < MAX_ACTIVE_EFFECTS; j++) e->effects[j].active = false;
                    }
                }
            }
        } else {
            g_autoResTimer = 0.0f;
        }
    }

    if (!player->alive) return;

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
