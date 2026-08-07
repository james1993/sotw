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
#include "progression.h"
#include <math.h>
#include <string.h>

#define PLAYER_INDEX 0
#define PORTAL_TRIGGER_RADIUS 40.0f
#define PORTAL_COOLDOWN 1.5f

// What a level-5 character has banked. GW1 hands out attribute points
// on a curve reaching 200 at level 20; this is that curve's value here.
#define STARTING_ATTRIBUTE_POINTS 0

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
    // Profession trainers (npcRole == NPC_PROFESSION_CHANGER): the ONE
    // profession this trainer teaches. Pre-Searing scatters the six
    // trainers across six areas, so which secondary you can take is a
    // question of where you can get to - see docs/research/pre-searing.md.
    Profession teaches;
    // Reforged Mode adds spawns to the Northlands. Marked rather than
    // held in a second table, so the area reads as one place with more
    // in it rather than as two different zones.
    bool reforgedOnly;
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

// ---------------------------------------------------------------------
// Pre-Searing Ascalon. Two outposts, six explorable areas each themed on
// a profession, and Piken Square for Reforged characters who can fight
// their way north. See docs/research/pre-searing.md for what's canon and
// what's a liberty.
// ---------------------------------------------------------------------

// --- Ashford Abbey: the cloister you start at ---

static const EnvProp g_abbeyProps[] = {
    { {    0, -150 }, PROP_FIRE,  1.0f },
    { { -170, -130 }, PROP_TENT,  1.0f },
    { {  170, -140 }, PROP_TENT,  1.0f },
    { { -210,   40 }, PROP_TENT,  0.9f },
    { {  200,   60 }, PROP_TENT,  0.9f },
    { { -300, -190 }, PROP_TREE,  1.1f },
    { {  300, -200 }, PROP_TREE,  1.0f },
    { { -320,  150 }, PROP_TREE,  0.9f },
    { {  330,  170 }, PROP_TREE,  1.0f },
    { { -110,  190 }, PROP_ROCK,  0.9f },
    { {  120,  200 }, PROP_ROCK,  1.0f },
    { {  -60,   80 }, PROP_GRASS, 1.0f },
    { {   70,  -30 }, PROP_GRASS, 0.9f },
};

static const SpawnDef g_abbeySpawns[] = {
    // Brother Mhenlo is the Monk trainer in the real Ashford Abbey, and
    // the closest trainer to where you start - which is a large part of
    // why so many Prophecies characters ended up Monk-secondary.
    { .kind = SPAWN_NPC, .name = "Brother Mhenlo", .pos = { -120, -60 },
      .npcRole = NPC_PROFESSION_CHANGER, .teaches = PROF_MONK,
      .npcColor = { 205, 190, 150, 255 } },
    { .kind = SPAWN_NPC, .name = "Abbot Ciglo", .pos = { 120, -80 },
      .npcRole = NPC_QUEST_GIVER, .npcColor = { 90, 170, 90, 255 } },
    { .kind = SPAWN_NPC, .name = "Merchant Niles", .pos = { 230, 40 },
      .npcRole = NPC_MERCHANT, .npcColor = { 90, 170, 90, 255 } },
    { .kind = SPAWN_NPC, .name = "Armorer Dunda", .pos = { -240, 60 },
      .npcRole = NPC_CRAFTER, .npcColor = { 90, 170, 90, 255 } },
    { .kind = SPAWN_NPC, .name = "Little Thom", .pos = { 40, 130 },
      .npcRole = NPC_HENCHMAN, .npcColor = { 170, 80, 60, 255 } },
};

// --- Lakeside County: the first field. Deliberately gentle - skale in
// the shallows and moa on the grass, nothing that hunts you. ---

static const EnvProp g_lakesideProps[] = {
    { { -340, -170 }, PROP_TREE,  1.1f },
    { { -190, -250 }, PROP_TREE,  0.9f },
    { {   80, -270 }, PROP_TREE,  1.0f },
    { {  350, -190 }, PROP_TREE,  1.2f },
    { {  430,   50 }, PROP_TREE,  0.9f },
    { {  300,  260 }, PROP_TREE,  1.0f },
    { {  -90,  280 }, PROP_TREE,  1.1f },
    { { -330,  210 }, PROP_TREE,  0.9f },
    { { -230,  -50 }, PROP_ROCK,  1.0f },
    { {  160,  -90 }, PROP_ROCK,  0.9f },
    { {   40,  170 }, PROP_ROCK,  0.8f },
    { {  760, -180 }, PROP_TREE,  1.0f },
    { {  900,  120 }, PROP_TREE,  1.1f },
    { {  620,  240 }, PROP_ROCK,  0.9f },
    { {  210,   30 }, PROP_GRASS, 1.0f },
    { { -140,  110 }, PROP_GRASS, 0.9f },
    { {  120, -170 }, PROP_GRASS, 1.1f },
    { {  520,  -40 }, PROP_GRASS, 1.0f },
    { {  840,  -20 }, PROP_GRASS, 0.9f },
    { { -250,  260 }, PROP_GRASS, 1.0f },
};

static const SpawnDef g_lakesideSpawns[] = {
    // River Skale in the water margin - the first thing most Prophecies
    // characters ever killed.
    { .kind = SPAWN_MONSTER, .name = "River Skale", .pos = { 300, 60 },
      .level = 1, .hp = 80, .armor = 20, .aggro = 110.0f, .strengthRank = 3,
      .species = SPECIES_SKALE, .group = 1 },
    { .kind = SPAWN_MONSTER, .name = "River Skale", .pos = { 380, 140 },
      .level = 1, .hp = 80, .armor = 20, .aggro = 110.0f, .strengthRank = 3,
      .species = SPECIES_SKALE, .group = 1 },
    { .kind = SPAWN_MONSTER, .name = "River Skale Fin", .pos = { 250, 160 },
      .level = 2, .hp = 110, .armor = 25, .aggro = 115.0f, .strengthRank = 4,
      .species = SPECIES_SKALE, .group = 1 },

    // Moa wander and won't start anything - the tutorial's way of
    // teaching that not everything on the field is a fight.
    { .kind = SPAWN_MONSTER_PATROL, .name = "Moa Bird", .pos = { -200, -150 },
      .posB = { 200, -200 }, .level = 1, .hp = 70, .armor = 20, .aggro = 70.0f,
      .strengthRank = 2, .species = SPECIES_MOA },
    { .kind = SPAWN_MONSTER_PATROL, .name = "Moa Bird", .pos = { 600, 200 },
      .posB = { 900, 60 }, .level = 2, .hp = 90, .armor = 20, .aggro = 70.0f,
      .strengthRank = 3, .species = SPECIES_MOA },

    { .kind = SPAWN_MONSTER, .name = "River Skale", .pos = { 820, -60 },
      .level = 2, .hp = 100, .armor = 25, .aggro = 115.0f, .strengthRank = 4,
      .species = SPECIES_SKALE, .group = 2 },
    { .kind = SPAWN_MONSTER, .name = "River Skale Fin", .pos = { 900, -140 },
      .level = 3, .hp = 130, .armor = 30, .aggro = 120.0f, .strengthRank = 5,
      .species = SPECIES_SKALE, .group = 2 },
};

// --- Ascalon City: the capital, and the hub every road runs back to ---

static const EnvProp g_cityProps[] = {
    { {    0, -170 }, PROP_FIRE,  1.1f },
    { { -230, -160 }, PROP_TENT,  1.1f },
    { {  230, -160 }, PROP_TENT,  1.1f },
    { { -280,   30 }, PROP_TENT,  1.0f },
    { {  280,   30 }, PROP_TENT,  1.0f },
    { { -150,  190 }, PROP_TENT,  0.9f },
    { {  160,  200 }, PROP_TENT,  0.9f },
    { { -380, -220 }, PROP_TREE,  1.0f },
    { {  390, -230 }, PROP_TREE,  1.0f },
    { { -400,  180 }, PROP_ROCK,  1.0f },
    { {  400,  190 }, PROP_ROCK,  0.9f },
};

static const SpawnDef g_citySpawns[] = {
    // Sir Tydus sends you off to find a second profession; Prince Rurik
    // is the one who takes you to the Charr. Both are canon givers.
    { .kind = SPAWN_NPC, .name = "Sir Tydus", .pos = { -140, -70 },
      .npcRole = NPC_QUEST_GIVER, .npcColor = { 90, 170, 90, 255 } },
    { .kind = SPAWN_NPC, .name = "Prince Rurik", .pos = { 150, -70 },
      .npcRole = NPC_QUEST_GIVER, .npcColor = { 120, 190, 120, 255 } },
    { .kind = SPAWN_NPC, .name = "Merchant Vassar", .pos = { -260, 90 },
      .npcRole = NPC_MERCHANT, .npcColor = { 90, 170, 90, 255 } },
    { .kind = SPAWN_NPC, .name = "Armorer Gali", .pos = { 260, 100 },
      .npcRole = NPC_CRAFTER, .npcColor = { 90, 170, 90, 255 } },
    // Kept just above the HUD's skill bar: at the default zoom anything
    // past y ~= +130 sits behind it while you stand in the plaza, and a
    // skill trainer you can't see is the one NPC that most needs finding.
    { .kind = SPAWN_NPC, .name = "Halbrik", .pos = { 0, 115 },
      .npcRole = NPC_SKILL_TRAINER, .npcColor = { 90, 170, 90, 255 } },
};

// --- Green Hills County: Warrior country, and the theatre where Lady
// Althea teaches Mesmers. Grawl come down off the hills in packs. ---

static const EnvProp g_greenHillsProps[] = {
    { { -300, -200 }, PROP_TREE,  1.2f },
    { { -120, -260 }, PROP_TREE,  1.0f },
    { {  180, -240 }, PROP_TREE,  1.1f },
    { {  400, -140 }, PROP_TREE,  0.9f },
    { {  330,  200 }, PROP_TREE,  1.0f },
    { {  -40,  270 }, PROP_TREE,  1.1f },
    { { -350,  180 }, PROP_TREE,  0.9f },
    { {  -80,  -80 }, PROP_ROCK,  1.1f },
    { {  240,   40 }, PROP_ROCK,  1.0f },
    { { -260,   60 }, PROP_ROCK,  0.9f },
    { {  100,  140 }, PROP_GRASS, 1.0f },
    { { -180,  -20 }, PROP_GRASS, 0.9f },
    { {  340,  -40 }, PROP_GRASS, 1.0f },
    // The theatre: a stage with a ring of seating stones around it.
    { { -430,  -60 }, PROP_TENT,  1.2f },
    { { -500,   20 }, PROP_ROCK,  0.8f },
    { { -430,   90 }, PROP_ROCK,  0.8f },
    { { -360,   20 }, PROP_ROCK,  0.8f },
};

static const SpawnDef g_greenHillsSpawns[] = {
    { .kind = SPAWN_NPC, .name = "Warmaster Grast", .pos = { 300, -180 },
      .npcRole = NPC_PROFESSION_CHANGER, .teaches = PROF_WARRIOR,
      .npcColor = { 170, 96, 72, 255 } },
    // Lady Althea holds the theatre, north-west of the city.
    { .kind = SPAWN_NPC, .name = "Lady Althea", .pos = { -440, 10 },
      .npcRole = NPC_PROFESSION_CHANGER, .teaches = PROF_MESMER,
      .npcColor = { 168, 92, 148, 255 } },

    { .kind = SPAWN_MONSTER, .name = "Grawl", .pos = { 120, -60 },
      .level = 3, .hp = 150, .armor = 35, .aggro = 125.0f, .strengthRank = 5,
      .species = SPECIES_GRAWL, .group = 1 },
    { .kind = SPAWN_MONSTER, .name = "Grawl", .pos = { 200, 20 },
      .level = 3, .hp = 150, .armor = 35, .aggro = 125.0f, .strengthRank = 5,
      .species = SPECIES_GRAWL, .group = 1 },
    { .kind = SPAWN_MONSTER, .name = "Grawl Longeye", .pos = { 60, 40 },
      .level = 4, .hp = 160, .armor = 35, .aggro = 130.0f, .strengthRank = 6,
      .caster = true, .species = SPECIES_GRAWL, .group = 1 },

    { .kind = SPAWN_MONSTER_PATROL, .name = "Moa Bird", .pos = { -200, 200 },
      .posB = { 200, 240 }, .level = 2, .hp = 90, .armor = 20, .aggro = 70.0f,
      .strengthRank = 3, .species = SPECIES_MOA },

    // A grawl chief holds the high ground: the elite in Warrior country.
    { .kind = SPAWN_MONSTER, .name = "Ulrick Grawl Chief", .pos = { 420, 160 },
      .level = 6, .hp = 320, .armor = 45, .aggro = 140.0f, .strengthRank = 8,
      .withHowl = true, .boss = true, .capSkill = SK_DEATHBLOW,
      .species = SPECIES_GRAWL, .group = 2 },
    { .kind = SPAWN_MONSTER, .name = "Grawl", .pos = { 350, 230 },
      .level = 3, .hp = 150, .armor = 35, .aggro = 125.0f, .strengthRank = 5,
      .species = SPECIES_GRAWL, .group = 2 },
};

// --- Regent Valley: Ranger country. Bandits hold the road, spiders the
// treeline, and Duke Barradin's estate sits at the far end. ---

static const EnvProp g_regentProps[] = {
    { { -320, -180 }, PROP_TREE,  1.2f },
    { { -180, -240 }, PROP_TREE,  1.1f },
    { {   60, -260 }, PROP_TREE,  1.0f },
    { {  280, -200 }, PROP_TREE,  1.2f },
    { {  460,  -80 }, PROP_TREE,  1.0f },
    { {  400,  180 }, PROP_TREE,  1.1f },
    { {  120,  260 }, PROP_TREE,  1.0f },
    { { -200,  240 }, PROP_TREE,  1.1f },
    { { -400,   60 }, PROP_TREE,  0.9f },
    { { -100,  -60 }, PROP_ROCK,  1.0f },
    { {  200,   60 }, PROP_ROCK,  0.9f },
    { {  -40,  140 }, PROP_GRASS, 1.0f },
    { {  300,  -40 }, PROP_GRASS, 0.9f },
    // Barradin's estate, east.
    { {  700, -100 }, PROP_TENT,  1.3f },
    { {  820,  -20 }, PROP_TENT,  1.1f },
    { {  760,   90 }, PROP_FIRE,  1.0f },
};

static const SpawnDef g_regentSpawns[] = {
    { .kind = SPAWN_NPC, .name = "Master Ranger Nente", .pos = { -300, 120 },
      .npcRole = NPC_PROFESSION_CHANGER, .teaches = PROF_RANGER,
      .npcColor = { 96, 134, 78, 255 } },
    { .kind = SPAWN_NPC, .name = "Duke Barradin", .pos = { 760, -30 },
      .npcRole = NPC_QUEST_GIVER, .npcColor = { 90, 170, 90, 255 } },

    // Bandits on the road: human, and the only pre-Searing enemy that
    // fights with a real skill bar rather than teeth.
    { .kind = SPAWN_MONSTER, .name = "Bandit Highwayman", .pos = { 120, -80 },
      .level = 4, .hp = 170, .armor = 40, .aggro = 130.0f, .strengthRank = 6,
      .species = SPECIES_HUMAN, .group = 1 },
    { .kind = SPAWN_MONSTER, .name = "Bandit Raider", .pos = { 210, -20 },
      .level = 3, .hp = 140, .armor = 35, .aggro = 125.0f, .strengthRank = 5,
      .species = SPECIES_HUMAN, .group = 1 },
    { .kind = SPAWN_MONSTER, .name = "Bandit Mesmer", .pos = { 60, 30 },
      .level = 4, .hp = 130, .armor = 30, .aggro = 130.0f, .strengthRank = 6,
      .caster = true, .species = SPECIES_HUMAN, .group = 1 },

    { .kind = SPAWN_MONSTER, .name = "Giant Needle Spider", .pos = { -160, -160 },
      .level = 4, .hp = 150, .armor = 35, .aggro = 120.0f, .strengthRank = 6,
      .species = SPECIES_DEVOURER, .group = 2 },
    { .kind = SPAWN_MONSTER, .name = "Giant Needle Spider", .pos = { -80, -210 },
      .level = 3, .hp = 130, .armor = 35, .aggro = 120.0f, .strengthRank = 5,
      .species = SPECIES_DEVOURER, .group = 2 },

    { .kind = SPAWN_MONSTER_PATROL, .name = "Grawl", .pos = { 300, 200 },
      .posB = { 600, 120 }, .level = 3, .hp = 150, .armor = 35, .aggro = 125.0f,
      .strengthRank = 5, .species = SPECIES_GRAWL },
    { .kind = SPAWN_MONSTER, .name = "River Skale", .pos = { -260, -60 },
      .level = 2, .hp = 100, .armor = 25, .aggro = 110.0f, .strengthRank = 4,
      .species = SPECIES_SKALE },
};

// --- Wizard's Folly: Elementalist country. Sodden ground, skale, and
// the aloes that made "Unnatural Growths" a quest. ---

static const EnvProp g_follyProps[] = {
    { { -280, -160 }, PROP_TREE,  0.9f },
    { {  -60, -220 }, PROP_TREE,  1.0f },
    { {  240, -180 }, PROP_TREE,  0.9f },
    { {  360,   80 }, PROP_TREE,  1.0f },
    { {  -20,  240 }, PROP_TREE,  0.9f },
    { { -320,  120 }, PROP_TREE,  1.0f },
    { { -160,  -40 }, PROP_ROCK,  1.1f },
    { {  140,   20 }, PROP_ROCK,  1.0f },
    { {  260,  200 }, PROP_ROCK,  0.9f },
    { { -240,  220 }, PROP_ROCK,  0.8f },
    { {   40,  120 }, PROP_GRASS, 1.1f },
    { { -120,  160 }, PROP_GRASS, 1.0f },
    { {  200,  -80 }, PROP_GRASS, 0.9f },
    { {  -60,  -90 }, PROP_FIRE,  0.9f },
};

static const SpawnDef g_follySpawns[] = {
    { .kind = SPAWN_NPC, .name = "Elementalist Aziure", .pos = { -60, -140 },
      .npcRole = NPC_PROFESSION_CHANGER, .teaches = PROF_ELEMENTALIST,
      .npcColor = { 80, 120, 200, 255 } },

    { .kind = SPAWN_MONSTER, .name = "River Skale", .pos = { 160, 120 },
      .level = 3, .hp = 120, .armor = 30, .aggro = 115.0f, .strengthRank = 5,
      .species = SPECIES_SKALE, .group = 1 },
    { .kind = SPAWN_MONSTER, .name = "River Skale Fin", .pos = { 250, 60 },
      .level = 4, .hp = 150, .armor = 35, .aggro = 120.0f, .strengthRank = 6,
      .species = SPECIES_SKALE, .group = 1 },

    // Aloes don't move. A stationary hazard is exactly what made them a
    // teaching tool for pulling one thing without waking three.
    { .kind = SPAWN_MONSTER, .name = "Aloe Husk", .pos = { -220, 40 },
      .level = 3, .hp = 140, .armor = 25, .aggro = 95.0f, .strengthRank = 5,
      .caster = true, .species = SPECIES_ALOE, .group = 2 },
    { .kind = SPAWN_MONSTER, .name = "Aloe Seed", .pos = { -160, 110 },
      .level = 2, .hp = 90, .armor = 20, .aggro = 90.0f, .strengthRank = 3,
      .species = SPECIES_ALOE, .group = 2 },
    { .kind = SPAWN_MONSTER, .name = "Aloe Seed", .pos = { -280, 130 },
      .level = 2, .hp = 90, .armor = 20, .aggro = 90.0f, .strengthRank = 3,
      .species = SPECIES_ALOE, .group = 2 },

    { .kind = SPAWN_MONSTER, .name = "Grawl", .pos = { 300, -140 },
      .level = 4, .hp = 160, .armor = 35, .aggro = 125.0f, .strengthRank = 6,
      .species = SPECIES_GRAWL, .group = 3 },
    { .kind = SPAWN_MONSTER, .name = "Grawl Longeye", .pos = { 380, -60 },
      .level = 4, .hp = 150, .armor = 35, .aggro = 130.0f, .strengthRank = 6,
      .caster = true, .species = SPECIES_GRAWL, .group = 3 },
};

// --- The Catacombs: Necromancer country, under the abbey. The only
// pre-Searing area that is genuinely unpleasant. ---

static const EnvProp g_catacombProps[] = {
    { { -260, -140 }, PROP_ROCK, 1.2f },
    { { -120, -200 }, PROP_ROCK, 1.0f },
    { {  100, -190 }, PROP_ROCK, 1.1f },
    { {  280, -120 }, PROP_ROCK, 1.2f },
    { {  320,   60 }, PROP_ROCK, 1.0f },
    { {  180,  200 }, PROP_ROCK, 1.1f },
    { {  -80,  230 }, PROP_ROCK, 1.0f },
    { { -300,  140 }, PROP_ROCK, 1.2f },
    { { -180,   20 }, PROP_ROCK, 0.8f },
    { {  120,   40 }, PROP_ROCK, 0.9f },
    { {  -20, -100 }, PROP_FIRE, 0.8f },
    { {  240,  -20 }, PROP_FIRE, 0.7f },
};

static const SpawnDef g_catacombSpawns[] = {
    { .kind = SPAWN_NPC, .name = "Necromancer Munne", .pos = { -240, -60 },
      .npcRole = NPC_PROFESSION_CHANGER, .teaches = PROF_NECROMANCER,
      .npcColor = { 86, 74, 104, 255 } },

    { .kind = SPAWN_MONSTER, .name = "Bone Minion", .pos = { 80, -60 },
      .level = 3, .hp = 110, .armor = 30, .aggro = 125.0f, .strengthRank = 5,
      .species = SPECIES_UNDEAD, .group = 1 },
    { .kind = SPAWN_MONSTER, .name = "Skeleton Warrior", .pos = { 160, -120 },
      .level = 4, .hp = 170, .armor = 45, .aggro = 130.0f, .strengthRank = 6,
      .species = SPECIES_UNDEAD, .group = 1 },
    { .kind = SPAWN_MONSTER, .name = "Skeleton Mesmer", .pos = { 40, -150 },
      .level = 4, .hp = 130, .armor = 30, .aggro = 130.0f, .strengthRank = 6,
      .caster = true, .species = SPECIES_UNDEAD, .group = 1 },

    // Diseased Devourers are canon Catacombs residents.
    { .kind = SPAWN_MONSTER, .name = "Diseased Devourer", .pos = { -100, 140 },
      .level = 4, .hp = 150, .armor = 40, .aggro = 120.0f, .strengthRank = 6,
      .species = SPECIES_DEVOURER, .group = 2 },
    { .kind = SPAWN_MONSTER, .name = "Diseased Devourer", .pos = { -30, 200 },
      .level = 3, .hp = 130, .armor = 40, .aggro = 120.0f, .strengthRank = 5,
      .species = SPECIES_DEVOURER, .group = 2 },

    { .kind = SPAWN_MONSTER, .name = "Vengeful Grenth's Champion", .pos = { 300, 180 },
      .level = 7, .hp = 360, .armor = 50, .aggro = 145.0f, .strengthRank = 9,
      .withHowl = true, .boss = true, .capSkill = SK_HEALING_LIGHT,
      .species = SPECIES_UNDEAD, .group = 3 },
    { .kind = SPAWN_MONSTER, .name = "Bone Minion", .pos = { 230, 240 },
      .level = 3, .hp = 110, .armor = 30, .aggro = 125.0f, .strengthRank = 5,
      .species = SPECIES_UNDEAD, .group = 3 },
};

// --- The Northlands: the frontier. Charr here, and the road to Piken
// Square for anyone playing Reforged. ---

static const EnvProp g_northProps[] = {
    { { -340, -200 }, PROP_ROCK,  1.2f },
    { { -160, -260 }, PROP_ROCK,  1.0f },
    { {  120, -250 }, PROP_ROCK,  1.1f },
    { {  380, -180 }, PROP_ROCK,  1.2f },
    { {  480,   40 }, PROP_ROCK,  1.0f },
    { {  300,  240 }, PROP_ROCK,  1.1f },
    { {  -60,  280 }, PROP_ROCK,  1.0f },
    { { -360,  180 }, PROP_ROCK,  1.2f },
    { { -220,  -40 }, PROP_TREE,  0.8f },
    { {  200,   60 }, PROP_TREE,  0.8f },
    { {  760, -120 }, PROP_ROCK,  1.1f },
    { {  920,   80 }, PROP_ROCK,  1.0f },
    { {  640,  200 }, PROP_TREE,  0.8f },
    { {  -40,  -60 }, PROP_FIRE,  1.0f },
};

static const SpawnDef g_northSpawns[] = {
    { .kind = SPAWN_MONSTER, .name = "Charr Grunt", .pos = { 180, -80 },
      .level = 5, .hp = 200, .armor = 50, .aggro = 130.0f, .strengthRank = 7,
      .species = SPECIES_CHARR, .group = 1 },
    { .kind = SPAWN_MONSTER, .name = "Charr Axe Fiend", .pos = { 280, -20 },
      .level = 6, .hp = 240, .armor = 55, .aggro = 135.0f, .strengthRank = 8,
      .withHowl = true, .species = SPECIES_CHARR, .group = 1 },
    { .kind = SPAWN_MONSTER, .name = "Charr Shaman", .pos = { 120, 20 },
      .level = 6, .hp = 190, .armor = 45, .aggro = 135.0f, .strengthRank = 8,
      .caster = true, .species = SPECIES_CHARR, .group = 1 },

    { .kind = SPAWN_MONSTER_PATROL, .name = "Grawl", .pos = { -280, 120 },
      .posB = { 100, 220 }, .level = 4, .hp = 160, .armor = 35, .aggro = 125.0f,
      .strengthRank = 6, .species = SPECIES_GRAWL },

    // Reforged Mode adds spawns to the Northlands. These are them: the
    // extra pressure that makes reaching Piken Square a fight rather
    // than a walk, which is exactly how Reforged frames it.
    { .kind = SPAWN_MONSTER, .name = "Charr Grunt", .pos = { 620, -60 },
      .level = 5, .hp = 200, .armor = 50, .aggro = 130.0f, .strengthRank = 7,
      .species = SPECIES_CHARR, .group = 2, .reforgedOnly = true },
    { .kind = SPAWN_MONSTER, .name = "Charr Ash Walker", .pos = { 700, 20 },
      .level = 6, .hp = 220, .armor = 50, .aggro = 135.0f, .strengthRank = 8,
      .caster = true, .species = SPECIES_CHARR, .group = 2, .reforgedOnly = true },
    { .kind = SPAWN_MONSTER, .name = "Charr Axe Fiend", .pos = { 800, -120 },
      .level = 6, .hp = 240, .armor = 55, .aggro = 135.0f, .strengthRank = 8,
      .withHowl = true, .species = SPECIES_CHARR, .group = 3, .reforgedOnly = true },
    { .kind = SPAWN_MONSTER, .name = "Bonfaaz Burntfur", .pos = { 950, 60 },
      .level = 8, .hp = 420, .armor = 60, .aggro = 150.0f, .strengthRank = 10,
      .withHowl = true, .boss = true, .capSkill = SK_METEOR,
      .species = SPECIES_CHARR, .group = 3, .reforgedOnly = true },
};

// --- Piken Square: Reforged Mode's pre-Searing outpost, past the Charr ---

static const EnvProp g_pikenProps[] = {
    { {    0, -140 }, PROP_FIRE, 1.1f },
    { { -180, -120 }, PROP_TENT, 1.1f },
    { {  180, -130 }, PROP_TENT, 1.1f },
    { { -220,   60 }, PROP_TENT, 0.9f },
    { {  220,   70 }, PROP_TENT, 0.9f },
    { { -300, -190 }, PROP_ROCK, 1.1f },
    { {  310, -180 }, PROP_ROCK, 1.0f },
    { { -280,  180 }, PROP_ROCK, 1.0f },
    { {  290,  190 }, PROP_ROCK, 0.9f },
};

static const SpawnDef g_pikenSpawns[] = {
    { .kind = SPAWN_NPC, .name = "Warmaster Riga", .pos = { -120, -50 },
      .npcRole = NPC_QUEST_GIVER, .npcColor = { 90, 170, 90, 255 } },
    { .kind = SPAWN_NPC, .name = "Trader Hurm", .pos = { 110, -70 },
      .npcRole = NPC_MERCHANT, .npcColor = { 90, 170, 90, 255 } },
    { .kind = SPAWN_NPC, .name = "Adept Kerra", .pos = { 200, 90 },
      .npcRole = NPC_SKILL_TRAINER, .npcColor = { 90, 170, 90, 255 } },
    { .kind = SPAWN_NPC, .name = "Armorer Sten", .pos = { -210, 80 },
      .npcRole = NPC_CRAFTER, .npcColor = { 90, 170, 90, 255 } },
};

#define COUNT(a) ((int)(sizeof(a) / sizeof((a)[0])))

static const ZoneDef g_zones[ZONE_COUNT] = {
    [ZONE_ASHFORD_ABBEY] = {
        .name = "Ashford Abbey",
        .mode = MODE_OUTPOST,
        .clearColor = { 22, 24, 18, 255 },  // cool cloister stone and grass
        .gridColor = { 74, 80, 60, 255 },
        .portals = {
            { {  380, 0 }, "To Lakeside County", ZONE_LAKESIDE_COUNTY, { -420, 0 } },
            { { -380, 0 }, "To The Catacombs",   ZONE_CATACOMBS,       {  320, 0 } },
        },
        .portalCount = 2,
        .bounds = { -450, -300, 900, 600 },
        .hasShrine = false,
        .props = g_abbeyProps, .propCount = COUNT(g_abbeyProps),
        .spawns = g_abbeySpawns, .spawnCount = COUNT(g_abbeySpawns),
    },
    [ZONE_LAKESIDE_COUNTY] = {
        .name = "Lakeside County",
        .mode = MODE_EXPLORABLE,
        .clearColor = { 16, 26, 18, 255 },  // green, wet, and safe
        .gridColor = { 58, 92, 58, 255 },
        .portals = {
            { { -460, 0 },   "To Ashford Abbey",     ZONE_ASHFORD_ABBEY,  {  320, 0 } },
            { { 1020, 0 },   "To Ascalon City",      ZONE_ASCALON_CITY,   { -360, 0 } },
            { { 280, -420 }, "To Green Hills County", ZONE_GREEN_HILLS,   {    0, 300 } },
            { { 280,  420 }, "To Regent Valley",     ZONE_REGENT_VALLEY,  {    0, -280 } },
        },
        .portalCount = 4,
        .bounds = { -520, -480, 1600, 960 },
        .hasShrine = true,
        .shrinePos = { -360, 160 },
        .props = g_lakesideProps, .propCount = COUNT(g_lakesideProps),
        .spawns = g_lakesideSpawns, .spawnCount = COUNT(g_lakesideSpawns),
    },
    [ZONE_ASCALON_CITY] = {
        .name = "Ascalon City",
        .mode = MODE_OUTPOST,
        .clearColor = { 26, 23, 17, 255 },  // warm stone, banners, torchlight
        .gridColor = { 92, 80, 58, 255 },
        .portals = {
            { { -420, 0 }, "To Lakeside County", ZONE_LAKESIDE_COUNTY, {  960, 0 } },
            { {  420, 0 }, "To The Northlands",  ZONE_NORTHLANDS,      { -420, 0 } },
        },
        .portalCount = 2,
        .bounds = { -480, -300, 960, 600 },
        .hasShrine = false,
        .props = g_cityProps, .propCount = COUNT(g_cityProps),
        .spawns = g_citySpawns, .spawnCount = COUNT(g_citySpawns),
    },
    [ZONE_GREEN_HILLS] = {
        .name = "Green Hills County",
        .mode = MODE_EXPLORABLE,
        .clearColor = { 17, 25, 15, 255 },
        .gridColor = { 62, 94, 54, 255 },
        .portals = {
            { { 0, 340 }, "To Lakeside County", ZONE_LAKESIDE_COUNTY, { 280, -360 } },
        },
        .portalCount = 1,
        .bounds = { -560, -340, 1120, 720 },
        .hasShrine = true,
        .shrinePos = { -160, 250 },
        .props = g_greenHillsProps, .propCount = COUNT(g_greenHillsProps),
        .spawns = g_greenHillsSpawns, .spawnCount = COUNT(g_greenHillsSpawns),
    },
    [ZONE_REGENT_VALLEY] = {
        .name = "Regent Valley",
        .mode = MODE_EXPLORABLE,
        .clearColor = { 18, 23, 15, 255 },  // deeper woodland
        .gridColor = { 66, 86, 50, 255 },
        .portals = {
            { {    0, -320 }, "To Lakeside County",  ZONE_LAKESIDE_COUNTY, { 280,  360 } },
            { { -480,    0 }, "To Wizard's Folly",   ZONE_WIZARDS_FOLLY,   { 380,    0 } },
        },
        .portalCount = 2,
        .bounds = { -540, -340, 1500, 700 },
        .hasShrine = true,
        .shrinePos = { -260, -180 },
        .props = g_regentProps, .propCount = COUNT(g_regentProps),
        .spawns = g_regentSpawns, .spawnCount = COUNT(g_regentSpawns),
    },
    [ZONE_WIZARDS_FOLLY] = {
        .name = "Wizard's Folly",
        .mode = MODE_EXPLORABLE,
        .clearColor = { 15, 21, 24, 255 },  // sodden, blue-grey
        .gridColor = { 54, 74, 88, 255 },
        .portals = {
            { { 420, 0 }, "To Regent Valley", ZONE_REGENT_VALLEY, { -440, 0 } },
        },
        .portalCount = 1,
        .bounds = { -460, -300, 920, 600 },
        .hasShrine = true,
        .shrinePos = { 300, 200 },
        .props = g_follyProps, .propCount = COUNT(g_follyProps),
        .spawns = g_follySpawns, .spawnCount = COUNT(g_follySpawns),
    },
    [ZONE_CATACOMBS] = {
        .name = "The Catacombs",
        .mode = MODE_EXPLORABLE,
        .clearColor = { 14, 13, 16, 255 },  // near-black; the one grim place
        .gridColor = { 62, 58, 70, 255 },
        .portals = {
            { { 380, 0 }, "To Ashford Abbey", ZONE_ASHFORD_ABBEY, { -320, 0 } },
        },
        .portalCount = 1,
        .bounds = { -420, -300, 840, 600 },
        .hasShrine = true,
        .shrinePos = { -300, -200 },
        .props = g_catacombProps, .propCount = COUNT(g_catacombProps),
        .spawns = g_catacombSpawns, .spawnCount = COUNT(g_catacombSpawns),
    },
    [ZONE_NORTHLANDS] = {
        .name = "The Northlands",
        .mode = MODE_EXPLORABLE,
        .clearColor = { 20, 21, 24, 255 },  // cold, grey, no green left
        .gridColor = { 78, 82, 90, 255 },
        .portals = {
            { { -460,  0 }, "To Ascalon City", ZONE_ASCALON_CITY, {  380, 0 } },
            // Reforged only. World_ZoneUnlocked hides it otherwise, so a
            // character who can't go there is never shown a door.
            { { 1120, 60 }, "To Piken Square", ZONE_PIKEN_SQUARE, { -300, 0 } },
        },
        .portalCount = 2,
        .bounds = { -520, -340, 1740, 700 },
        .hasShrine = true,
        .shrinePos = { -380, -200 },
        .props = g_northProps, .propCount = COUNT(g_northProps),
        .spawns = g_northSpawns, .spawnCount = COUNT(g_northSpawns),
    },
    [ZONE_PIKEN_SQUARE] = {
        .name = "Piken Square",
        .mode = MODE_OUTPOST,
        .clearColor = { 26, 22, 20, 255 },  // stone and torchlight
        .gridColor = { 88, 76, 66, 255 },
        .portals = {
            { { -340, 0 }, "To The Northlands", ZONE_NORTHLANDS, { 1040, 60 } },
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

static const ZoneDef *g_zone = &g_zones[ZONE_ASHFORD_ABBEY];
static ZoneId g_zoneId = ZONE_ASHFORD_ABBEY;
static ZoneId g_lastOutpostId = ZONE_ASHFORD_ABBEY;
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

// Portals to locked zones are filtered out of BOTH accessors, so the
// renderer, the compass and the transition check all agree on which
// doors exist. Filtering in one place and not the others is how you get
// a gate you can see but not use.
int World_GetPortalCount(void) {
    int n = 0;
    for (int i = 0; i < g_zone->portalCount; i++) {
        if (World_ZoneUnlocked(g_zone->portals[i].destZone)) n++;
    }
    return n;
}

const ZonePortal *World_GetPortal(int index) {
    if (index < 0) return NULL;
    for (int i = 0; i < g_zone->portalCount; i++) {
        if (!World_ZoneUnlocked(g_zone->portals[i].destZone)) continue;
        if (index-- == 0) return &g_zone->portals[i];
    }
    return NULL;
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

// GW1's henchmen scale to the party leader rather than sitting at a
// fixed level, which is what stops them from either carrying a new
// character or becoming dead weight at 20. These two helpers are that
// rule, in one place, so Cynn and Thom can't drift apart.
static int HenchmanLevel(void) {
    const Entity *player = Entity_Get(PLAYER_INDEX);
    int level = player ? player->level : 1;
    if (level < 1) level = 1;
    if (level > MAX_LEVEL) level = MAX_LEVEL;
    return level;
}

// Rank 1 at level 1 up to the rank 12 cap at 20 - the same shape a
// player following their primary would have, without the freedom to
// spike one line, which is the point of a henchman.
static int HenchmanRank(int level, int offset) {
    int rank = 1 + ((level - 1) * 11) / (MAX_LEVEL - 1) - offset;
    if (rank < 1) rank = 1;
    if (rank > ATTRIBUTE_RANK_CAP) rank = ATTRIBUTE_RANK_CAP;
    return rank;
}

// Cynn is Prophecies' own Elementalist henchman. Vekk was an asura
// from an expansion two campaigns away, which is a long way to come
// for a walk around Lakeside.
static void SpawnCynn(Vector2 pos) {
    int idx = Entity_Spawn(ENT_HERO, "Cynn", 0, pos, (Color){ 60, 120, 220, 255 });
    Entity *hero = Entity_Get(idx);
    hero->primaryProfession = PROF_ELEMENTALIST;
    hero->secondaryProfession = PROF_MONK;
    hero->level = HenchmanLevel();
    hero->attackRange = 220.0f; // caster keeps distance
    hero->armor = 30;           // Ascalon-tier robes
    hero->attributeRank[ATTR_FIRE_MAGIC] = HenchmanRank(hero->level, 0);
    // His pool is 20 base plus 3 per rank of Energy Storage, and nothing
    // else. Being an Elementalist doesn't hand out energy; spending
    // points on the primary attribute does.
    hero->attributeRank[ATTR_ENERGY_STORAGE] = HenchmanRank(hero->level, 1);
    Entity_RecomputeAttributeStats(hero);
    hero->hp = hero->maxHp;
    hero->energy = hero->maxEnergy;
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
    thom->level = HenchmanLevel();
    thom->attributeRank[ATTR_SWORDSMANSHIP] = HenchmanRank(thom->level, 0);
    thom->attributeRank[ATTR_STRENGTH] = HenchmanRank(thom->level, 1);
    Entity_RecomputeAttributeStats(thom);
    thom->hp = thom->maxHp;
    thom->energy = thom->maxEnergy;
    thom->armor = 40; // Ascalon-tier Warrior harness, same as the player's
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

// Body colour per species. The sprite shapes carry most of the read,
// but colour is what tells you at a glance whether the thing across the
// field is a moa you can walk past or a Charr that will kill you.
static Color SpeciesColor(Species s) {
    switch (s) {
        case SPECIES_SKALE:    return (Color){  92, 134, 116, 255 }; // wet green
        case SPECIES_GRAWL:    return (Color){ 132, 112,  86, 255 }; // matted fur
        case SPECIES_MOA:      return (Color){ 198, 158,  96, 255 }; // straw plumage
        case SPECIES_UNDEAD:   return (Color){ 150, 146, 128, 255 }; // grave-grey
        case SPECIES_ALOE:     return (Color){  96, 148,  72, 255 }; // rank green
        case SPECIES_DEVOURER: return (Color){ 118,  92,  74, 255 }; // chitin
        case SPECIES_CHARR:    return (Color){ 112,  82,  62, 255 };
        default:               return (Color){ 120, 104,  92, 255 }; // bandits
    }
}

static Entity *SpawnMonster(const SpawnDef *def) {
    int idx = Entity_Spawn(ENT_MONSTER, def->name, 1, def->pos, SpeciesColor(def->species));
    Entity *m = Entity_Get(idx);
    if (!m) return NULL;
    m->level = def->level;
    // Reforged Mode gives pre-Searing enemies reduced health and armor.
    // Applied here, once, so every spawn table stays written in the
    // game's normal numbers rather than carrying two sets.
    int hp = def->hp, armor = def->armor;
    if (Character_IsReforged()) {
        hp = hp * 85 / 100;
        armor -= 5;
        if (armor < 0) armor = 0;
    }
    m->maxHp = m->hp = hp;
    m->maxEnergy = m->energy = 20;
    m->armor = armor;
    m->aggroRange = def->aggro;
    m->leashRange = def->aggro * 2.5f;
    // Attack damage scales with the monster's level, as GW1's does. It
    // used to be a flat 6-12 for everything, which was tuned around a
    // character who started at level 5 - against a level-1 Ascalonian in
    // starter cloth, that made a River Skale hit as hard as a Charr and
    // killed you on the walk out of the abbey.
    m->attackDamageMin = 2 + def->level;
    m->attackDamageMax = 4 + def->level * 2;
    m->attributeRank[ATTR_MONSTROUS] = def->strengthRank;
    // Aloes are rooted: they fight what comes to them and never chase.
    if (def->species == SPECIES_ALOE) m->moveSpeed = 0.0f;
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
    if (npc) {
        npc->npcRole = def->npcRole;
        npc->teachesProfession = def->teaches;
    }
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
    SpawnCynn((Vector2){ playerEntry.x - 50, playerEntry.y + 50 });
    if (g_thomHired) {
        SpawnThomCompanion((Vector2){ playerEntry.x + 30, playerEntry.y + 60 });
    }

    for (int i = 0; i < g_zone->spawnCount; i++) {
        const SpawnDef *def = &g_zone->spawns[i];
        // Reforged Mode's additional Northlands spawns simply aren't
        // there for anyone else.
        if (def->reforgedOnly && !Character_IsReforged()) continue;
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

bool World_ZoneUnlocked(ZoneId zone) {
    // Piken Square is Reforged Mode's addition to pre-Searing. Everyone
    // else never sees the portal at all - a locked door you can't ever
    // open is worse than no door.
    if (zone == ZONE_PIKEN_SQUARE) return Character_IsReforged();
    return true;
}

void World_RestoreToOutpost(ZoneId zone) {
    if (zone < 0 || zone >= ZONE_COUNT || g_zones[zone].mode != MODE_OUTPOST ||
        !World_ZoneUnlocked(zone)) {
        zone = ZONE_ASHFORD_ABBEY;
    }
    LoadZone(zone, (Vector2){ 0, 0 });
}

void World_Init(void) {
    // Fresh-start state, so a New Game from the menu after a previous
    // run doesn't inherit the old party composition.
    g_thomHired = false;
    g_lastOutpostId = ZONE_ASHFORD_ABBEY;

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
    // Pre-Searing starts you at level 1, with nothing spent and nothing
    // banked. Everything below - attribute points, skill points, the
    // ranks behind your one skill - is earned from here.
    player->level = 1;

    // Per-profession starting kit. The choice has to change how the
    // character actually plays from the first fight, not just which
    // word appears on the nameplate.
    //
    // Two things deliberately are NOT set per profession, because GW1
    // doesn't set them per profession either:
    //
    //   Energy. Everyone gets the same 20. An Elementalist's famously
    //   deep pool comes entirely from Energy Storage ranks, which is
    //   why it's their PRIMARY attribute and why a secondary Ele never
    //   gets it. Entity_RecomputeAttributeStats derives it below.
    //
    //   Armour. It comes from the armour piece each profession is
    //   issued, because Items_EquipArmor is the one place that owns
    //   player->armor. A Warrior is tougher because the harness is
    //   AL 40, not because of a number written twice.
    Skillbook_Reset();
    for (int i = 0; i < SKILL_BAR_SIZE; i++) player->skillBar[i] = -1;

    Item startWeapon, startArmor;
    switch (g_character.primary) {
        case PROF_WARRIOR:
            player->skillBar[0] = SK_GASH;
            startWeapon = (Item){ ITEM_WEAPON, "Ascalon Sword", 13, 20, 28.0f, 1.33f, 0, 1, false };
            startArmor  = (Item){ ITEM_ARMOR, "Warrior Harness (AL 40)", 0, 0, 0, 0, 40, 1, false };
            break;
        case PROF_RANGER:
            player->skillBar[0] = SK_POWER_SHOT;
            // A bow: long reach, slow swing, and the only starting
            // weapon that lets you open a fight before it reaches you.
            startWeapon = (Item){ ITEM_WEAPON, "Ascalon Longbow", 12, 21, 240.0f, 2.0f, 0, 1, false };
            startArmor  = (Item){ ITEM_ARMOR, "Ranger Leathers (AL 35)", 0, 0, 0, 0, 35, 1, false };
            break;
        case PROF_MONK:
            player->skillBar[0] = SK_ORISON_OF_HEALING;
            startWeapon = (Item){ ITEM_WEAPON, "Smiting Rod", 11, 22, 160.0f, 1.75f, 0, 1, false };
            startArmor  = (Item){ ITEM_ARMOR, "Monk Raiment (AL 30)", 0, 0, 0, 0, 30, 1, false };
            break;
        case PROF_NECROMANCER:
            player->skillBar[0] = SK_VAMPIRIC_GAZE;
            startWeapon = (Item){ ITEM_WEAPON, "Bone Idol", 10, 20, 220.0f, 1.75f, 0, 1, false };
            startArmor  = (Item){ ITEM_ARMOR, "Necromancer Vestments (AL 30)", 0, 0, 0, 0, 30, 1, false };
            break;
        case PROF_MESMER:
            player->skillBar[0] = SK_ETHER_FEAST;
            startWeapon = (Item){ ITEM_WEAPON, "Jeweled Wand", 10, 20, 220.0f, 1.75f, 0, 1, false };
            startArmor  = (Item){ ITEM_ARMOR, "Mesmer Attire (AL 30)", 0, 0, 0, 0, 30, 1, false };
            break;
        case PROF_ELEMENTALIST:
        default:
            player->skillBar[0] = SK_FIRE_BOLT;
            startWeapon = (Item){ ITEM_WEAPON, "Kindling Staff", 11, 22, 220.0f, 1.75f, 0, 1, false };
            startArmor  = (Item){ ITEM_ARMOR, "Elementalist Robes (AL 30)", 0, 0, 0, 0, 30, 1, false };
            break;
    }

    // Level -> health, Energy Storage -> energy. GW1 derives both; so
    // does this, in one place, so no kit can quietly disagree.
    Entity_RecomputeAttributeStats(player);
    player->hp = player->maxHp;
    player->energy = player->maxEnergy;
    // A level-1 character has no attribute points at all - GW1's
    // schedule is 5 per level to 10, 10 to 15, 15 to 20, so level 1 is
    // zero and every rank you ever hold is one you levelled for. The
    // deduction below still runs at the real GW1 rate in case a
    // starting kit ever pre-spends again.
    {
        int spent = 0;
        for (int a = 0; a < ATTR_COUNT; a++) {
            spent += g_attrCumulativeCost[player->attributeRank[a]];
        }
        player->attributePoints = STARTING_ATTRIBUTE_POINTS - spent;
        if (player->attributePoints < 0) player->attributePoints = 0;
    }

    // A new character knows exactly one skill - their profession's
    // signature. The other seven slots are empty on purpose: filling
    // them is the game.
    // No banked skill point either: at level 1 the trainer has nothing
    // for you, and your first skills come from quests. Levelling is what
    // opens the trainer, which is the pre-Searing order.
    g_skillPoints = 0;
    Skillbook_Unlock(player->skillBar[0]);

    Items_AddToInventory(startWeapon);
    Items_AddToInventory(startArmor);
    Items_EquipWeapon(player, 0);
    Items_EquipArmor(player, 1);

    LoadZone(ZONE_ASHFORD_ABBEY, (Vector2){ 0, 0 });
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
    for (int i = 0; i < World_GetPortalCount(); i++) {
        const ZonePortal *portal = World_GetPortal(i);
        float dx = player->pos.x - portal->pos.x;
        float dy = player->pos.y - portal->pos.y;
        if (sqrtf(dx * dx + dy * dy) <= PORTAL_TRIGGER_RADIUS) {
            LoadZone(portal->destZone, portal->destEntry);
            return;
        }
    }
}
