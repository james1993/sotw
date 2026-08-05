#include "world.h"
#include "character.h"
#include "entity.h"
#include "items.h"
#include "attributes.h"
#include "skill.h"
#include "skillbook.h"
#include "projectile.h"
#include "fx.h"
#include "save.h"
#include <math.h>
#include <string.h>

#define PLAYER_INDEX 0
#define PORTAL_TRIGGER_RADIUS 40.0f
#define PORTAL_COOLDOWN 1.5f

// ---------------------------------------------------------------------
// Zone content data. A zone is a ZoneDef: name, mode, colors, portals,
// shrine, decorative props, and a spawn table. Adding a zone = adding
// data here plus a ZoneId in world.h; the loader below is generic.
// ---------------------------------------------------------------------

typedef enum {
    SPAWN_MONSTER,
    SPAWN_MONSTER_PATROL,
    SPAWN_NPC
} SpawnKind;

typedef struct {
    SpawnKind kind;
    const char *name;
    Vector2 pos;      // spawn point; patrols: waypoint A
    Vector2 posB;     // patrols: waypoint B
    int level, hp, armor;
    float aggro;
    int strengthRank;
    bool withHowl;    // monsters: carry Feral Howl, the interruptible self-heal
    bool caster;      // monsters: ranged Fire Magic loadout instead of claws
    bool boss;        // monsters: tougher, marked, and teaches capSkill
    int capSkill;     // boss: SkillId captured on death. 0 is a real
                      // SkillId, so bosses must set this explicitly and
                      // non-bosses are filtered by the .boss flag.
    Species species;  // monsters: which body sprite.c draws (and whether
                      // the corpse leaves a Charr Hide - only Charr do)
    int group;        // monsters: spawn group id, 0 = ungrouped. Groups
                      // aggro as one (pull any member, all join) and are
                      // kept to at most 4 members so pulls stay winnable.
    NpcRole npcRole;  // NPCs only
    Color npcColor;   // NPCs only
} SpawnDef;

typedef struct {
    const char *name;
    GameMode mode;
    Color clearColor; // window background
    Color gridColor;  // ground grid
    ZonePortal portals[MAX_ZONE_PORTALS];
    int portalCount;
    Rectangle bounds; // playable area; movement clamps to this
    bool hasShrine;
    Vector2 shrinePos;
    const EnvProp *props;
    int propCount;
    const SpawnDef *spawns;
    int spawnCount;
} ZoneDef;

// --- Ashford Camp: a small circle of tents around a fire ---

static const EnvProp g_campProps[] = {
    { { 0, -140 }, PROP_FIRE, 1.0f },
    { { -150, -140 }, PROP_TENT, 1.0f },
    { { -190, -40 }, PROP_TENT, 0.9f },
    { { 150, -150 }, PROP_TENT, 1.1f },
    { { 190, -50 }, PROP_TENT, 0.9f },
    { { -120, 150 }, PROP_TENT, 1.0f },
    { { 140, 160 }, PROP_TENT, 0.95f },
    { { -260, -180 }, PROP_TREE, 1.0f },
    { { 280, -200 }, PROP_TREE, 1.1f },
    { { -280, 120 }, PROP_ROCK, 1.0f },
    { { 240, 140 }, PROP_ROCK, 0.9f },
};

static const SpawnDef g_campSpawns[] = {
    // Little Thom's standing NPC only appears while he isn't hired -
    // the loader skips henchman NPCs who are currently in the party.
    { .kind = SPAWN_NPC, .name = "Little Thom", .pos = { 40, 110 },
      .npcRole = NPC_HENCHMAN, .npcColor = { 170, 80, 60, 255 } },
    { .kind = SPAWN_NPC, .name = "Captain Osric", .pos = { -120, -60 },
      .npcRole = NPC_QUEST_GIVER, .npcColor = { 90, 170, 90, 255 } },
    { .kind = SPAWN_NPC, .name = "Merchant", .pos = { 90, -90 },
      .npcRole = NPC_MERCHANT, .npcColor = { 90, 170, 90, 255 } },
    { .kind = SPAWN_NPC, .name = "Armorer Dunda", .pos = { -200, 60 },
      .npcRole = NPC_CRAFTER, .npcColor = { 90, 170, 90, 255 } },
    { .kind = SPAWN_NPC, .name = "Master Ilsa", .pos = { 230, 120 },
      .npcRole = NPC_SKILL_TRAINER, .npcColor = { 90, 170, 90, 255 } },
};

// --- Ashford Plains: three static camps spaced beyond each other's
// aggro bubbles, two patrols sweeping the ground between them. The
// strategy is pure GW1 - watch the compass, pull a camp when the patrol
// is at the far end of its route, and finish the fight before it swings
// back through. ---

static const EnvProp g_plainsProps[] = {
    { { -320, -160 }, PROP_TREE, 1.1f },
    { { -260, -230 }, PROP_TREE, 0.85f },
    { { -140, -260 }, PROP_TREE, 1.0f },
    { { 120, -260 }, PROP_TREE, 0.9f },
    { { 340, -180 }, PROP_TREE, 1.2f },
    { { 420, -60 }, PROP_TREE, 0.8f },
    { { 400, 160 }, PROP_TREE, 1.0f },
    { { 300, 260 }, PROP_TREE, 0.9f },
    { { -80, 260 }, PROP_TREE, 1.1f },
    { { -300, 200 }, PROP_TREE, 0.85f },
    { { -420, 40 }, PROP_TREE, 1.0f },
    { { -220, -60 }, PROP_ROCK, 1.0f },
    { { -160, 120 }, PROP_ROCK, 0.8f },
    { { 140, -80 }, PROP_ROCK, 1.1f },
    { { 380, 40 }, PROP_ROCK, 0.9f },
    { { 60, 180 }, PROP_ROCK, 0.7f },
    { { -60, -160 }, PROP_ROCK, 0.9f },
    { { 200, 140 }, PROP_GRASS, 1.0f },
    { { -180, 20 }, PROP_GRASS, 0.9f },
    { { 100, -40 }, PROP_GRASS, 1.1f },
    { { -100, 140 }, PROP_GRASS, 0.8f },
    { { 320, -20 }, PROP_GRASS, 1.0f },
    { { -20, 220 }, PROP_GRASS, 0.9f },
    { { 220, -160 }, PROP_GRASS, 1.0f },
    // Eastern reaches of the expanded plains.
    { { 620, -420 }, PROP_TREE, 1.1f },
    { { 980, -400 }, PROP_TREE, 0.9f },
    { { 1180, -160 }, PROP_TREE, 1.2f },
    { { 1240, 240 }, PROP_TREE, 1.0f },
    { { 760, 430 }, PROP_TREE, 1.1f },
    { { 1050, 60 }, PROP_ROCK, 1.1f },
    { { 640, 200 }, PROP_ROCK, 0.9f },
    { { 880, -60 }, PROP_ROCK, 0.8f },
    { { 1300, -60 }, PROP_ROCK, 1.0f },
    { { 720, -120 }, PROP_GRASS, 1.0f },
    { { 1000, 180 }, PROP_GRASS, 1.1f },
    { { 1150, -280 }, PROP_GRASS, 0.9f },
    { { 560, 60 }, PROP_GRASS, 1.0f },
    { { 1330, 150 }, PROP_GRASS, 1.0f },
};

static const SpawnDef g_plainsSpawns[] = {
    // Camp 1, near the entrance - the first pull.
    { .kind = SPAWN_MONSTER, .name = "Charr Brute", .pos = { 300, 40 },
      .level = 5, .hp = 220, .armor = 60, .aggro = 130.0f, .strengthRank = 8, .withHowl = true, .species = SPECIES_CHARR, .group = 1 },
    { .kind = SPAWN_MONSTER, .name = "Charr Grunt", .pos = { 380, -50 },
      .level = 2, .hp = 140, .armor = 40, .aggro = 120.0f, .strengthRank = 6, .species = SPECIES_CHARR, .group = 1 },

    // Camp 2, northeast.
    { .kind = SPAWN_MONSTER, .name = "Charr Stalker", .pos = { 820, -300 },
      .level = 4, .hp = 180, .armor = 50, .aggro = 130.0f, .strengthRank = 7, .withHowl = true, .species = SPECIES_CHARR, .group = 2 },
    { .kind = SPAWN_MONSTER, .name = "Charr Grunt", .pos = { 760, -220 },
      .level = 2, .hp = 140, .armor = 40, .aggro = 120.0f, .strengthRank = 6, .species = SPECIES_CHARR, .group = 2 },
    { .kind = SPAWN_MONSTER, .name = "Charr Grunt", .pos = { 900, -230 },
      .level = 3, .hp = 160, .armor = 40, .aggro = 120.0f, .strengthRank = 6, .species = SPECIES_CHARR, .group = 2 },

    // Camp 3, southeast.
    { .kind = SPAWN_MONSTER, .name = "Charr Stalker", .pos = { 900, 320 },
      .level = 4, .hp = 180, .armor = 50, .aggro = 130.0f, .strengthRank = 7, .withHowl = true, .species = SPECIES_CHARR, .group = 3 },
    { .kind = SPAWN_MONSTER, .name = "Charr Grunt", .pos = { 830, 250 },
      .level = 2, .hp = 140, .armor = 40, .aggro = 120.0f, .strengthRank = 6, .species = SPECIES_CHARR, .group = 3 },
    { .kind = SPAWN_MONSTER, .name = "Charr Grunt", .pos = { 980, 260 },
      .level = 3, .hp = 160, .armor = 40, .aggro = 120.0f, .strengthRank = 6, .species = SPECIES_CHARR, .group = 3 },

    // Patrols hunt alone (no .group): their interception threat comes
    // from timing, not numbers. The north-south sweep crosses the
    // corridor between camp 1 and the eastern camps; the east-west
    // prowler covers the road to the ridge. Aggro capped at
    // AGGRO_RING_RADIUS so the drawn bubble never under-promises what a
    // patrol can notice.
    { .kind = SPAWN_MONSTER_PATROL, .name = "Charr Patrol",
      .pos = { 560, -320 }, .posB = { 560, 320 },
      .level = 4, .hp = 170, .armor = 45, .aggro = AGGRO_RING_RADIUS, .strengthRank = 7, .species = SPECIES_CHARR },
    // Kruul the Emberfang: the plains boss, alone in the far northeast so
    // finding him is its own small expedition. He casts the Meteor elite
    // and teaches it when he falls - the first elite most players will
    // own, and the reason to come back here once you can handle him.
    { .kind = SPAWN_MONSTER, .name = "Kruul the Emberfang", .pos = { 1320, -380 },
      .level = 8, .hp = 420, .armor = 70, .aggro = 140.0f, .strengthRank = 10,
      .caster = true, .boss = true, .capSkill = SK_METEOR, .species = SPECIES_CHARR, .group = 4 },

    { .kind = SPAWN_MONSTER_PATROL, .name = "Charr Prowler",
      .pos = { 700, 40 }, .posB = { 1240, 40 },
      .level = 4, .hp = 170, .armor = 45, .aggro = AGGRO_RING_RADIUS, .strengthRank = 7, .species = SPECIES_CHARR },
};

// --- Charr Foothills: the ashen ground past the eastern ridge, on the
// road to Piken Watch. Harder than the plains: warbands bring a Shaman
// (a ranged Fire Magic caster - kill or interrupt it first), and the
// gullies crawl with Devourers, whose corpses yield no hides. ---

static const EnvProp g_foothillsProps[] = {
    { { -380, -200 }, PROP_ROCK, 1.3f },
    { { -300, 180 },  PROP_ROCK, 1.1f },
    { { -120, -80 },  PROP_ROCK, 0.9f },
    { { 60, -260 },   PROP_ROCK, 1.2f },
    { { 240, 120 },   PROP_ROCK, 1.0f },
    { { 420, -140 },  PROP_ROCK, 1.4f },
    { { 620, 220 },   PROP_ROCK, 1.1f },
    { { 840, -40 },   PROP_ROCK, 0.8f },
    { { 1020, -260 }, PROP_ROCK, 1.2f },
    { { 1160, 160 },  PROP_ROCK, 1.0f },
    { { -200, -320 }, PROP_TREE, 0.8f }, // scorched stragglers
    { { 500, 320 },   PROP_TREE, 0.7f },
    { { 900, 340 },   PROP_TREE, 0.75f },
    { { 1240, -80 },  PROP_TREE, 0.8f },
    { { 160, 40 },    PROP_GRASS, 0.8f },
    { { 700, -180 },  PROP_GRASS, 0.9f },
    { { 1080, 60 },   PROP_GRASS, 0.8f },
    { { -40, 240 },   PROP_GRASS, 0.9f },
};

static const SpawnDef g_foothillsSpawns[] = {
    // Warband 1 guards the road in: the Shaman hangs back and burns you
    // while the legionnaires close - focus or interrupt it, GW1 rule #1.
    { .kind = SPAWN_MONSTER, .name = "Charr Legionnaire", .pos = { 260, -60 },
      .level = 6, .hp = 240, .armor = 65, .aggro = 130.0f, .strengthRank = 9, .species = SPECIES_CHARR, .group = 1 },
    { .kind = SPAWN_MONSTER, .name = "Charr Shaman", .pos = { 340, 20 },
      .level = 6, .hp = 180, .armor = 45, .aggro = 130.0f, .strengthRank = 8, .caster = true, .species = SPECIES_CHARR, .group = 1 },
    { .kind = SPAWN_MONSTER, .name = "Charr Grunt", .pos = { 300, -140 },
      .level = 4, .hp = 170, .armor = 45, .aggro = 120.0f, .strengthRank = 7, .species = SPECIES_CHARR, .group = 1 },

    // Devourer gully, south: a pinned nest, all melee, hits hard.
    { .kind = SPAWN_MONSTER, .name = "Plague Devourer", .pos = { 600, 260 },
      .level = 5, .hp = 200, .armor = 55, .aggro = 125.0f, .strengthRank = 8, .species = SPECIES_DEVOURER, .group = 2 },
    { .kind = SPAWN_MONSTER, .name = "Whiptail Devourer", .pos = { 680, 320 },
      .level = 5, .hp = 180, .armor = 50, .aggro = 125.0f, .strengthRank = 8, .species = SPECIES_DEVOURER, .group = 2 },
    { .kind = SPAWN_MONSTER, .name = "Whiptail Devourer", .pos = { 540, 340 },
      .level = 4, .hp = 170, .armor = 50, .aggro = 120.0f, .strengthRank = 7, .species = SPECIES_DEVOURER, .group = 2 },

    // Warband 2 holds the pass to Piken Watch: the hardest pull, with a
    // howling brute AND a shaman behind it.
    { .kind = SPAWN_MONSTER, .name = "Charr Warcaller", .pos = { 1000, -120 },
      .level = 7, .hp = 300, .armor = 70, .aggro = 135.0f, .strengthRank = 10, .withHowl = true, .species = SPECIES_CHARR, .group = 3 },
    { .kind = SPAWN_MONSTER, .name = "Charr Shaman", .pos = { 1080, -40 },
      .level = 6, .hp = 180, .armor = 45, .aggro = 130.0f, .strengthRank = 8, .caster = true, .species = SPECIES_CHARR, .group = 3 },
    { .kind = SPAWN_MONSTER, .name = "Charr Legionnaire", .pos = { 940, -40 },
      .level = 6, .hp = 240, .armor = 65, .aggro = 130.0f, .strengthRank = 9, .species = SPECIES_CHARR, .group = 3 },

    // Vharn the Bonesmith: the foothills boss, keeping his warband alive
    // with the Healing Light elite. Kill him and the elite is yours -
    // the Monk capture, and a genuinely hard fight because he heals
    // himself unless you interrupt or burst him down.
    { .kind = SPAWN_MONSTER, .name = "Vharn the Bonesmith", .pos = { 1180, 300 },
      .level = 9, .hp = 460, .armor = 70, .aggro = 140.0f, .strengthRank = 10,
      .caster = true, .boss = true, .capSkill = SK_HEALING_LIGHT,
      .species = SPECIES_CHARR, .group = 4 },
    { .kind = SPAWN_MONSTER, .name = "Charr Legionnaire", .pos = { 1080, 350 },
      .level = 6, .hp = 240, .armor = 65, .aggro = 130.0f, .strengthRank = 9,
      .species = SPECIES_CHARR, .group = 4 },

    // A lone Devourer prowls the middle ground - no group, pure ambush.
    { .kind = SPAWN_MONSTER_PATROL, .name = "Lurking Devourer",
      .pos = { 400, -280 }, .posB = { 820, 160 },
      .level = 5, .hp = 190, .armor = 50, .aggro = AGGRO_RING_RADIUS, .strengthRank = 8, .species = SPECIES_DEVOURER },
};

// --- Piken Watch: a forward outpost dug into the foothills. Warmaster
// Grast hands out the frontier work; a trader keeps the party stocked
// without the walk home. ---

static const EnvProp g_pikenProps[] = {
    { { 0, -150 },    PROP_FIRE, 1.1f },
    { { -160, -120 }, PROP_TENT, 1.0f },
    { { 150, -130 },  PROP_TENT, 1.05f },
    { { -190, 60 },   PROP_TENT, 0.9f },
    { { 180, 80 },    PROP_TENT, 0.95f },
    { { -300, -60 },  PROP_ROCK, 1.3f },
    { { 290, -40 },   PROP_ROCK, 1.2f },
    { { -240, 160 },  PROP_ROCK, 1.0f },
    { { 250, 170 },   PROP_ROCK, 1.1f },
    { { 0, 220 },     PROP_ROCK, 0.9f },
};

static const SpawnDef g_pikenSpawns[] = {
    { .kind = SPAWN_NPC, .name = "Warmaster Grast", .pos = { -110, -50 },
      .npcRole = NPC_QUEST_GIVER, .npcColor = { 90, 170, 90, 255 } },
    { .kind = SPAWN_NPC, .name = "Trader Hurm", .pos = { 100, -80 },
      .npcRole = NPC_MERCHANT, .npcColor = { 90, 170, 90, 255 } },
    { .kind = SPAWN_NPC, .name = "Adept Kerra", .pos = { 210, 90 },
      .npcRole = NPC_SKILL_TRAINER, .npcColor = { 90, 170, 90, 255 } },
};

#define COUNT(a) ((int)(sizeof(a) / sizeof((a)[0])))

static const ZoneDef g_zones[ZONE_COUNT] = {
    [ZONE_ASHFORD_CAMP] = {
        .name = "Ashford Camp",
        .mode = MODE_OUTPOST,
        .clearColor = { 24, 20, 14, 255 },  // warm dirt tones
        .gridColor = { 80, 68, 52, 255 },
        .portals = {
            { { 260, 0 }, "To Ashford Plains", ZONE_ASHFORD_PLAINS, { -320, 0 } },
        },
        .portalCount = 1,
        .bounds = { -450, -300, 900, 600 },
        .hasShrine = false,
        .props = g_campProps, .propCount = COUNT(g_campProps),
        .spawns = g_campSpawns, .spawnCount = COUNT(g_campSpawns),
    },
    [ZONE_ASHFORD_PLAINS] = {
        .name = "Ashford Plains",
        .mode = MODE_EXPLORABLE,
        .clearColor = { 14, 22, 13, 255 },  // cool grass tones
        .gridColor = { 52, 76, 48, 255 },
        .portals = {
            { { -420, 0 }, "To Ashford Camp", ZONE_ASHFORD_CAMP, { 160, 0 } },
            // Past the ridge marker the scouting quest sends you to.
            { { 1400, 40 }, "To Charr Foothills", ZONE_CHARR_FOOTHILLS, { -400, 0 } },
        },
        .portalCount = 2,
        .bounds = { -520, -480, 1980, 960 },
        .hasShrine = true,
        .shrinePos = { -320, 140 },
        .props = g_plainsProps, .propCount = COUNT(g_plainsProps),
        .spawns = g_plainsSpawns, .spawnCount = COUNT(g_plainsSpawns),
    },
    [ZONE_CHARR_FOOTHILLS] = {
        .name = "Charr Foothills",
        .mode = MODE_EXPLORABLE,
        .clearColor = { 24, 17, 14, 255 },  // ashen scorched ground
        .gridColor = { 84, 62, 50, 255 },
        .portals = {
            { { -480, 0 }, "To Ashford Plains", ZONE_ASHFORD_PLAINS, { 1320, 40 } },
            { { 1300, -180 }, "To Piken Watch", ZONE_PIKEN_WATCH, { -220, 0 } },
        },
        .portalCount = 2,
        .bounds = { -560, -400, 1960, 800 },
        .hasShrine = true,
        .shrinePos = { -380, 120 },
        .props = g_foothillsProps, .propCount = COUNT(g_foothillsProps),
        .spawns = g_foothillsSpawns, .spawnCount = COUNT(g_foothillsSpawns),
    },
    [ZONE_PIKEN_WATCH] = {
        .name = "Piken Watch",
        .mode = MODE_OUTPOST,
        .clearColor = { 26, 22, 20, 255 },  // stone and torchlight
        .gridColor = { 88, 76, 66, 255 },
        .portals = {
            { { -300, 0 }, "To Charr Foothills", ZONE_CHARR_FOOTHILLS, { 1200, -180 } },
        },
        .portalCount = 1,
        .bounds = { -400, -280, 800, 560 },
        .hasShrine = false,
        .props = g_pikenProps, .propCount = COUNT(g_pikenProps),
        .spawns = g_pikenSpawns, .spawnCount = COUNT(g_pikenSpawns),
    },
};

// ---------------------------------------------------------------------
// Zone state + accessors
// ---------------------------------------------------------------------

static const ZoneDef *g_zone = &g_zones[ZONE_ASHFORD_CAMP];
static ZoneId g_zoneId = ZONE_ASHFORD_CAMP;
static ZoneId g_lastOutpostId = ZONE_ASHFORD_CAMP;
static bool g_thomHired = false;
static float g_portalCooldown = 0.0f;
static float g_wipeTimer = 0.0f;
static float g_autoResTimer = 0.0f;

GameMode World_GetMode(void) { return g_zone->mode; }
const char *World_GetZoneName(void) { return g_zone->name; }
ZoneId World_GetZoneId(void) { return g_zoneId; }
Color World_GetClearColor(void) { return g_zone->clearColor; }
Color World_GetGridColor(void) { return g_zone->gridColor; }
bool World_IsThomHired(void) { return g_thomHired; }

void World_SetThomHired(bool hired) {
    g_thomHired = hired;
    Save_Write(); // party composition is part of the saved character
}

ZoneId World_GetLastOutpostId(void) { return g_lastOutpostId; }

int World_GetPortalCount(void) { return g_zone->portalCount; }

const ZonePortal *World_GetPortal(int index) {
    if (index < 0 || index >= g_zone->portalCount) return NULL;
    return &g_zone->portals[index];
}

const EnvProp *World_GetProps(int *count) {
    if (count) *count = g_zone->propCount;
    return g_zone->props;
}

Rectangle World_GetBounds(void) {
    return g_zone->bounds;
}

bool World_GetShrine(Vector2 *pos) {
    if (pos) *pos = g_zone->shrinePos;
    return g_zone->hasShrine;
}

void World_DismissHenchman(Entity *henchman) {
    if (!henchman || !henchman->isHenchman) return;
    if (g_zone->mode != MODE_OUTPOST) return; // GW1: party changes only in outposts

    g_thomHired = false;
    henchman->kind = ENT_NPC;
    henchman->npcRole = NPC_HENCHMAN;
    henchman->isHenchman = false;
    henchman->alive = true;
    henchman->hp = henchman->maxHp;
    henchman->targetRef = Entity_NoRef();
    henchman->hasMoveTarget = false;
    Save_Write();
}

// ---------------------------------------------------------------------
// Spawning
// ---------------------------------------------------------------------

// Strips combat/zone-transient state off the persistent player entity
// when crossing a portal; progression (level/xp/attributes) and the
// global inventory survive untouched. Death penalty clears on rezoning,
// exactly like GW1.
static void ResetPlayerTransientState(Entity *p, Vector2 entryPos) {
    p->pos = entryPos;
    p->moveTarget = entryPos;
    p->hasMoveTarget = false;
    p->targetRef = Entity_NoRef();
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
    hero->skillBar[0] = SK_FIRE_BOLT;
    hero->skillBar[1] = SK_CINDER_STORM;
    hero->skillBar[2] = SK_MIND_SEAR; // energy management
}

// Everything that makes Little Thom a fighting Warrior henchman - used
// both when zone loads respawn him as a party member (below) and when
// the hire dialog converts his standing NPC (ui_panels.c), so the two
// copies of his stat block can't drift apart.
void World_SetupThomStats(Entity *thom) {
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
    thom->skillBar[0] = SK_GASH;
    thom->skillBar[1] = SK_RUSH_STRIKE;
    thom->skillBar[2] = SK_BATTLE_CRY;
}

// Little Thom, the pre-Searing Warrior henchman. As a party member he
// fights with a fixed Warrior bar - fixed skill sets being exactly what
// separates henchmen from heroes in GW1
// (docs/research/gw1-mechanics.md #10).
static void SpawnThomCompanion(Vector2 pos) {
    int idx = Entity_Spawn(ENT_HERO, "Little Thom", 0, pos, (Color){ 170, 80, 60, 255 });
    World_SetupThomStats(Entity_Get(idx));
}

static Entity *SpawnMonster(const SpawnDef *def) {
    int idx = Entity_Spawn(ENT_MONSTER, def->name, 1, def->pos, (Color){ 100, 90, 80, 255 });
    Entity *m = Entity_Get(idx);
    if (!m) return NULL;
    m->level = def->level;
    m->maxHp = m->hp = def->hp;
    m->maxEnergy = m->energy = 20;
    m->armor = def->armor;
    m->aggroRange = def->aggro;
    m->leashRange = def->aggro * 2.5f;
    m->attributeRank[ATTR_STRENGTH] = def->strengthRank;
    m->groupId = def->group;
    m->species = def->species;
    m->isBoss = def->boss;
    m->capturedSkill = def->boss ? def->capSkill : -1;
    if (def->boss) {
        // Bosses read as bigger on the field and hit harder, so a
        // capture run is a real fight rather than a detour.
        m->radius *= 1.35f;
        m->attackDamageMin = (int)(m->attackDamageMin * 1.4f);
        m->attackDamageMax = (int)(m->attackDamageMax * 1.4f);
    }
    if (def->caster) {
        // Shaman loadout: hangs back and casts Fire Magic - the ranged
        // threat that makes the party pick targets instead of piling on.
        m->attackRange = 210.0f;
        m->maxEnergy = m->energy = 40;
        m->attributeRank[ATTR_FIRE_MAGIC] = def->strengthRank;
        m->attributeRank[ATTR_SMITING_PRAYERS] = def->strengthRank;
        m->skillBar[0] = SK_FIRE_BOLT;
        m->skillBar[1] = SK_MIND_SEAR;
        // Shamans hex as well as burn, which is what makes hex removal
        // worth a slot on the way through the foothills.
        m->skillBar[2] = SK_SHROUD_OF_DOUBT;
        if (def->withHowl) m->skillBar[3] = SK_FERAL_HOWL;
    } else if (def->species == SPECIES_DEVOURER) {
        // Devourers hobble what they catch - Crippled is their threat.
        m->skillBar[0] = SK_HOBBLING_STRIKE;
        m->skillBar[1] = SK_CLAW_SWIPE;
    } else if (def->withHowl) {
        m->skillBar[0] = SK_FERAL_HOWL; // self-heal - interrupt it!
        m->skillBar[2] = SK_RENDING_CLAWS; // bleeding + weakness
        m->skillBar[1] = SK_CLAW_SWIPE;
    } else {
        m->skillBar[0] = SK_CLAW_SWIPE;
        m->skillBar[1] = SK_RENDING_CLAWS;
    }
    return m;
}

// A monster that walks a route between two points instead of standing
// still - GW1's roaming patrols, the reason pull timing matters: fight
// a static group in a patrol's path and the patrol joins in.
static void SpawnMonsterPatrol(const SpawnDef *def) {
    Entity *m = SpawnMonster(def);
    if (!m) return;
    Vector2 a = def->pos, b = def->posB;
    // spawnPos anchors at the route midpoint so the leash covers the
    // whole path; the monster itself starts at one end.
    m->spawnPos = (Vector2){ (a.x + b.x) / 2.0f, (a.y + b.y) / 2.0f };
    m->pos = a;
    m->hasPatrol = true;
    m->patrolA = a;
    m->patrolB = b;
    m->patrolDir = +1;
    float dx = b.x - a.x, dy = b.y - a.y;
    m->leashRange = sqrtf(dx * dx + dy * dy) / 2.0f + 380.0f;
    m->color = (Color){ 120, 85, 65, 255 }; // reads differently from statics
}

static void SpawnNpc(const SpawnDef *def) {
    int idx = Entity_Spawn(ENT_NPC, def->name, 0, def->pos, def->npcColor);
    Entity *npc = Entity_Get(idx);
    if (npc) npc->npcRole = def->npcRole;
}

// Rebuilds the entity array for a zone while carrying the player
// (slot 0) across. GW1 semantics: explorables are a fresh instance on
// every entry; returning to an outpost fully restores the party.
static void LoadZone(ZoneId zoneId, Vector2 playerEntry) {
    Entity saved = g_entities[PLAYER_INDEX];
    g_entityCount = 0;
    memset(g_drops, 0, sizeof(g_drops)); // ground loot doesn't survive rezoning, like GW1
    Projectile_ClearAll();               // and neither do shots in flight
    Fx_Clear();

    g_entities[g_entityCount++] = saved;
    Entity *player = Entity_Get(PLAYER_INDEX);
    ResetPlayerTransientState(player, playerEntry);

    g_zone = &g_zones[zoneId];
    g_zoneId = zoneId;
    if (g_zone->mode == MODE_OUTPOST) g_lastOutpostId = zoneId;
    g_portalCooldown = PORTAL_COOLDOWN;
    g_wipeTimer = 0.0f;
    g_autoResTimer = 0.0f;

    if (g_zone->mode == MODE_OUTPOST) {
        // Outposts restore the party completely, GW1-style.
        player->hp = player->maxHp;
        player->energy = player->maxEnergy;
    }

    // The party spawns around the player's entry point.
    SpawnVekk((Vector2){ playerEntry.x - 50, playerEntry.y + 50 });
    if (g_thomHired) {
        SpawnThomCompanion((Vector2){ playerEntry.x + 30, playerEntry.y + 60 });
    }

    for (int i = 0; i < g_zone->spawnCount; i++) {
        const SpawnDef *def = &g_zone->spawns[i];
        switch (def->kind) {
            case SPAWN_MONSTER:
                SpawnMonster(def);
                break;
            case SPAWN_MONSTER_PATROL:
                SpawnMonsterPatrol(def);
                break;
            case SPAWN_NPC:
                // A henchman standing in the outpost is the same person
                // as the one in your party - don't spawn his NPC while
                // he's hired.
                if (def->npcRole == NPC_HENCHMAN && g_thomHired) break;
                SpawnNpc(def);
                break;
        }
    }

    // GW1 autosaves around zone transitions; so do we. (No-op until the
    // save system is enabled, so the initial World_Init load and the
    // save-restore load can never clobber an existing file.)
    Save_Write();
}

void World_RestoreToOutpost(ZoneId zone) {
    if (zone < 0 || zone >= ZONE_COUNT || g_zones[zone].mode != MODE_OUTPOST) {
        zone = ZONE_ASHFORD_CAMP;
    }
    LoadZone(zone, (Vector2){ 0, 0 });
}

void World_Init(void) {
    // Fresh-start state, so a New Game from the menu after a previous
    // run doesn't inherit the old party composition.
    g_thomHired = false;
    g_lastOutpostId = ZONE_ASHFORD_CAMP;

    // The persistent player, built from whatever the creator produced
    // (character.c). Every zone load carries this entity across.
    char title[64];
    Character_FormatTitle(&g_character, title, sizeof(title));
    int playerIdx = Entity_Spawn(ENT_PLAYER, title, 0, (Vector2){ 0, 0 },
                                 (Color){ 220, 200, 120, 255 });
    Entity *player = Entity_Get(playerIdx);
    player->primaryProfession = g_character.primary;
    // Until a profession trainer grants a secondary, the character is
    // single-profession. Attribute_Accessible takes the primary twice,
    // which correctly opens nothing extra.
    player->secondaryProfession = (g_character.secondary == PROF_NONE)
                                  ? g_character.primary
                                  : (Profession)g_character.secondary;
    player->sex = g_character.sex;
    player->skinTone = g_character.skinTone;
    player->hairColor = g_character.hairColor;
    player->hairStyle = g_character.hairStyle;
    player->level = 5;
    player->baseMaxHp = 100 + 20 * (player->level - 1); // GW1: +20 HP per level

    // Per-profession starting kit. The choice has to change how the
    // character actually plays from the first fight, not just which
    // word appears on the nameplate: a Warrior soaks hits and has
    // almost no energy, an Elementalist is the reverse, a Monk sits
    // between them and heals.
    //
    // Armour is NOT set here - it comes from the armour piece each
    // profession is issued below, because Items_EquipArmor is the one
    // place that owns player->armor. A Warrior is tougher because the
    // harness is AL 40, not because of a number written twice.
    Skillbook_Reset();
    for (int i = 0; i < SKILL_BAR_SIZE; i++) player->skillBar[i] = -1;

    Item startWeapon, startArmor;
    switch (g_character.primary) {
        case PROF_WARRIOR:
            player->baseMaxEnergy = 20;
            player->attributeRank[ATTR_STRENGTH] = 4;
            player->attributeRank[ATTR_TACTICS] = 3;
            player->skillBar[0] = SK_GASH;
            startWeapon = (Item){ ITEM_WEAPON, "Ascalon Sword", 13, 20, 28.0f, 1.33f, 0, 1, false };
            startArmor  = (Item){ ITEM_ARMOR, "Warrior Harness (AL 40)", 0, 0, 0, 0, 40, 1, false };
            break;
        case PROF_ELEMENTALIST:
            player->baseMaxEnergy = 50;
            player->attributeRank[ATTR_FIRE_MAGIC] = 4;
            player->attributeRank[ATTR_ENERGY_STORAGE] = 3;
            player->skillBar[0] = SK_FIRE_BOLT;
            startWeapon = (Item){ ITEM_WEAPON, "Kindling Staff", 11, 22, 220.0f, 1.75f, 0, 1, false };
            startArmor  = (Item){ ITEM_ARMOR, "Elementalist Robes (AL 30)", 0, 0, 0, 0, 30, 1, false };
            break;
        case PROF_MONK:
        default:
            player->baseMaxEnergy = 30;
            player->attributeRank[ATTR_HEALING_PRAYERS] = 4;
            player->attributeRank[ATTR_SMITING_PRAYERS] = 3;
            player->attributeRank[ATTR_DIVINE_FAVOR] = 1;
            player->skillBar[0] = SK_ORISON_OF_HEALING;
            startWeapon = (Item){ ITEM_WEAPON, "Smiting Rod", 11, 22, 160.0f, 1.75f, 0, 1, false };
            startArmor  = (Item){ ITEM_ARMOR, "Monk Raiment (AL 30)", 0, 0, 0, 0, 30, 1, false };
            break;
    }

    Entity_RecomputePenalizedStats(player);
    player->hp = player->maxHp;
    player->energy = player->maxEnergy;
    // Level 5 grants 20 attribute points; the ranks above spend most of
    // them, leaving a few free for the attributes panel (K).
    player->attributePoints = 3;

    // A new character knows exactly one skill - their profession's
    // signature. The other seven slots are empty on purpose: filling
    // them is the game.
    g_skillPoints = 1; // one to spend at the trainer straight away
    Skillbook_Unlock(player->skillBar[0]);

    Items_AddToInventory(startWeapon);
    Items_AddToInventory(startArmor);
    Items_EquipWeapon(player, 0);
    Items_EquipArmor(player, 1);

    LoadZone(ZONE_ASHFORD_CAMP, (Vector2){ 0, 0 });
}

void World_Update(Entity *player, float dt) {
    if (g_portalCooldown > 0.0f) {
        g_portalCooldown -= dt;
        return;
    }
    if (!player) return;

    // --- Death & resurrection bookkeeping (explorable only) ---
    if (g_zone->mode == MODE_EXPLORABLE) {
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
                    e->pos = (Vector2){ g_zone->shrinePos.x + (slot % 2) * 34.0f,
                                        g_zone->shrinePos.y + (slot / 2) * 34.0f };
                    e->moveTarget = e->pos;
                    e->hasMoveTarget = false;
                    e->targetRef = Entity_NoRef();
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
                        e->targetRef = Entity_NoRef();
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

    // --- Portals: walk into a gate and cross to its destination ---
    for (int i = 0; i < g_zone->portalCount; i++) {
        const ZonePortal *portal = &g_zone->portals[i];
        float dx = player->pos.x - portal->pos.x;
        float dy = player->pos.y - portal->pos.y;
        if (sqrtf(dx * dx + dy * dy) <= PORTAL_TRIGGER_RADIUS) {
            LoadZone(portal->destZone, portal->destEntry);
            return;
        }
    }
}
