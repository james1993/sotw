#include "world.h"
#include "character.h"
#include "entity.h"
#include "items.h"
#include "attributes.h"
#include "skill.h"
#include "skillbook.h"
#include "projectile.h"
#include "fx.h"
#include "mapdraw.h"
#include "area.h"
#include "ai_hero.h"
#include "save.h"
#include "progression.h"
#include "ui_hints.h"
#include <stdio.h>
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
    // Tighter aggro on the opening skale so a solo character can pull one
    // at a time instead of the whole margin at once - the pre-Searing
    // teaching fight, not an ambush.
    { .kind = SPAWN_MONSTER, .name = "River Skale", .pos = { 300, 60 },
      .level = 1, .hp = 80, .armor = 20, .aggro = 88.0f, .strengthRank = 3,
      .species = SPECIES_SKALE, .group = 1 },
    { .kind = SPAWN_MONSTER, .name = "River Skale", .pos = { 380, 140 },
      .level = 1, .hp = 80, .armor = 20, .aggro = 88.0f, .strengthRank = 3,
      .species = SPECIES_SKALE, .group = 1 },
    { .kind = SPAWN_MONSTER, .name = "River Skale Fin", .pos = { 250, 160 },
      .level = 2, .hp = 110, .armor = 25, .aggro = 92.0f, .strengthRank = 4,
      .species = SPECIES_SKALE, .group = 1 },

    // A collector, out in the field where GW1 puts them: not in the
    // safety of an outpost, but far enough in that reaching one is
    // itself a small errand.
    { .kind = SPAWN_NPC, .name = "Farmer Hamnet", .pos = { 140, 250 },
      .npcRole = NPC_COLLECTOR, .npcColor = { 176, 154, 96, 255 } },

    // Pre-Searing's quest-givers out in the field: Gwen the flute-girl,
    // Pitney with his runaway moa, and Grazden with the Monk's trial.
    { .kind = SPAWN_NPC, .name = "Gwen", .pos = { -140, 200 },
      .npcRole = NPC_QUEST_GIVER, .npcColor = { 210, 180, 200, 255 } },
    { .kind = SPAWN_NPC, .name = "Pitney", .pos = { 60, -230 },
      .npcRole = NPC_QUEST_GIVER, .npcColor = { 176, 154, 96, 255 } },
    { .kind = SPAWN_NPC, .name = "Grazden the Protector", .pos = { -300, -60 },
      .npcRole = NPC_QUEST_GIVER, .npcColor = { 205, 190, 150, 255 } },

    // Moa wander and won't start anything - the tutorial's way of
    // teaching that not everything on the field is a fight.
    { .kind = SPAWN_MONSTER_PATROL, .name = "Moa Bird", .pos = { -200, -150 },
      .posB = { 200, -200 }, .level = 1, .hp = 70, .armor = 20, .aggro = 70.0f,
      .strengthRank = 2, .species = SPECIES_MOA },
    { .kind = SPAWN_MONSTER_PATROL, .name = "Moa Bird", .pos = { 600, 200 },
      .posB = { 900, 60 }, .level = 2, .hp = 90, .armor = 20, .aggro = 70.0f,
      .strengthRank = 3, .species = SPECIES_MOA },

    { .kind = SPAWN_MONSTER, .name = "River Skale", .pos = { 820, -60 },
      .level = 2, .hp = 100, .armor = 25, .aggro = 90.0f, .strengthRank = 4,
      .species = SPECIES_SKALE, .group = 2 },
    { .kind = SPAWN_MONSTER, .name = "River Skale Fin", .pos = { 900, -140 },
      .level = 3, .hp = 130, .armor = 30, .aggro = 95.0f, .strengthRank = 5,
      .species = SPECIES_SKALE, .group = 2 },
};

// --- Ascalon City: the capital, and the hub every road runs back to ---

// Ascalon City, the capital - dressed in stone, not canvas. Ordered
// back-to-front (low y first) so nearer buildings overlap farther ones,
// and laid out around the plaza the NPCs stand in.
static const EnvProp g_cityProps[] = {
    // A skyline of sandstone houses across the back and down the sides.
    { { -360, -262 }, PROP_HOUSE, 1.10f },
    { { -155, -270 }, PROP_HOUSE, 1.15f },
    { {   45, -272 }, PROP_HOUSE, 1.05f },
    { {  250, -262 }, PROP_HOUSE, 1.15f },
    { {  390, -252 }, PROP_HOUSE, 1.00f },
    // Two houses pulled in to frame the square in view from the plaza.
    { { -345,  -35 }, PROP_HOUSE, 1.05f },
    { {  345,  -35 }, PROP_HOUSE, 1.05f },
    { { -455,   90 }, PROP_HOUSE, 1.00f },
    { {  455,   95 }, PROP_HOUSE, 1.00f },
    // The plaza's heart: a statue of Dwayna, banners to either side.
    { {    0, -165 }, PROP_STATUE, 1.15f },
    { {  -82, -132 }, PROP_BANNER, 1.00f },
    { {   82, -132 }, PROP_BANNER, 1.00f },
    // A fountain and market stalls fill out the square.
    { {  150,   35 }, PROP_FOUNTAIN, 1.00f },
    { { -200,  150 }, PROP_STALL, 1.00f },
    { {  190,  155 }, PROP_STALL, 1.00f },
    // Braziers to light the capital.
    { { -262,  -28 }, PROP_BRAZIER, 1.00f },
    { {  262,  -28 }, PROP_BRAZIER, 1.00f },
    { {  -78,   66 }, PROP_BRAZIER, 0.90f },
    { {   78,   66 }, PROP_BRAZIER, 0.90f },
    // A touch of green at the corners.
    { { -420,  205 }, PROP_TREE, 1.00f },
    { {  420,  210 }, PROP_TREE, 1.00f },
    { { -110,  215 }, PROP_GRASS, 1.00f },
    { {  120,  220 }, PROP_GRASS, 1.00f },
};

static const SpawnDef g_citySpawns[] = {
    // --- The people who have business with you, in the central plaza. ---
    // Sir Tydus sends you off to find a second profession; Prince Rurik
    // is the one who takes you to the Charr. Both are canon givers.
    { .kind = SPAWN_NPC, .name = "Sir Tydus", .pos = { -140, -70 },
      .npcRole = NPC_QUEST_GIVER, .npcColor = { 90, 170, 90, 255 } },
    { .kind = SPAWN_NPC, .name = "Prince Rurik", .pos = { 150, -70 },
      .npcRole = NPC_QUEST_GIVER, .npcColor = { 120, 190, 120, 255 } },
    // Sanura is Ascalon City's merchant; Halbrik its skill trainer - both
    // real pre-Searing names.
    { .kind = SPAWN_NPC, .name = "Sanura", .pos = { -260, 90 },
      .npcRole = NPC_MERCHANT, .npcColor = { 90, 170, 90, 255 } },
    { .kind = SPAWN_NPC, .name = "Armorer Gali", .pos = { 260, 100 },
      .npcRole = NPC_CRAFTER, .npcColor = { 90, 170, 90, 255 } },
    // Kept just above the HUD's skill bar: at the default zoom anything
    // past y ~= +130 sits behind it while you stand in the plaza, and a
    // skill trainer you can't see is the one NPC that most needs finding.
    { .kind = SPAWN_NPC, .name = "Halbrik", .pos = { 0, 115 },
      .npcRole = NPC_SKILL_TRAINER, .npcColor = { 90, 170, 90, 255 } },

    // --- The capital's crowd: guards on the walls, nobles and townsfolk
    // going about the last ordinary morning. Kept out toward the edges so
    // they populate the city without stealing the talk prompt from the
    // service NPCs in the middle. Role-less, so they carry no gilded
    // trim and read as ordinary folk; talk to one for a word of the day.
    { .kind = SPAWN_NPC, .name = "Ascalon Guard",    .pos = { -420, -210 },
      .npcColor = { 150, 120, 96, 255 } },
    { .kind = SPAWN_NPC, .name = "Ascalon Guard",    .pos = { 420, -210 },
      .npcColor = { 150, 120, 96, 255 } },
    { .kind = SPAWN_NPC, .name = "Ascalon Guard",    .pos = { -430, 40 },
      .npcColor = { 150, 120, 96, 255 } },
    { .kind = SPAWN_NPC, .name = "Ascalon Guard",    .pos = { 430, 40 },
      .npcColor = { 150, 120, 96, 255 } },
    { .kind = SPAWN_NPC, .name = "Ascalon Noble",    .pos = { -70, -230 },
      .npcColor = { 176, 140, 190, 255 } },
    { .kind = SPAWN_NPC, .name = "Ascalon Noble",    .pos = { 90, -235 },
      .npcColor = { 150, 160, 200, 255 } },
    { .kind = SPAWN_NPC, .name = "Priest of Dwayna", .pos = { -300, -150 },
      .npcColor = { 210, 205, 180, 255 } },
    { .kind = SPAWN_NPC, .name = "Townsperson",      .pos = { 300, -150 },
      .npcColor = { 150, 150, 130, 255 } },
    { .kind = SPAWN_NPC, .name = "Townsperson",      .pos = { -180, -200 },
      .npcColor = { 140, 156, 120, 255 } },
    { .kind = SPAWN_NPC, .name = "Town Crier",       .pos = { 200, -200 },
      .npcColor = { 190, 160, 90, 255 } },
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

    { .kind = SPAWN_NPC, .name = "Sentry Wallin", .pos = { -320, -120 },
      .npcRole = NPC_COLLECTOR, .npcColor = { 150, 160, 176, 255 } },

    // Profession quest-givers who teach a signature skill of their line.
    { .kind = SPAWN_NPC, .name = "Van the Warrior", .pos = { 460, 120 },
      .npcRole = NPC_QUEST_GIVER, .npcColor = { 170, 96, 72, 255 } },
    { .kind = SPAWN_NPC, .name = "Sebedoh the Mesmer", .pos = { -460, 180 },
      .npcRole = NPC_QUEST_GIVER, .npcColor = { 168, 92, 148, 255 } },

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
    // He teaches Eviscerate, the real Axe Mastery elite - the one skill
    // a Warrior most wants and can only get by capping it off a boss.
    { .kind = SPAWN_MONSTER, .name = "Ulrick Grawl Chief", .pos = { 420, 160 },
      .level = 6, .hp = 320, .armor = 45, .aggro = 140.0f, .strengthRank = 8,
      .withHowl = true, .boss = true, .capSkill = SK_EVISCERATE,
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
    { .kind = SPAWN_NPC, .name = "Aidan", .pos = { -380, -40 },
      .npcRole = NPC_QUEST_GIVER, .npcColor = { 96, 134, 78, 255 } },

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

    // Burrowing worms of the field - low aggro, they only stir when you
    // step close (The Worm Problem's quarry).
    { .kind = SPAWN_MONSTER, .name = "Plague Worm", .pos = { 420, 220 },
      .level = 2, .hp = 90, .armor = 20, .aggro = 80.0f, .strengthRank = 3,
      .species = SPECIES_WORM, .group = 4 },
    { .kind = SPAWN_MONSTER, .name = "Plague Worm", .pos = { 500, 160 },
      .level = 2, .hp = 90, .armor = 20, .aggro = 80.0f, .strengthRank = 3,
      .species = SPECIES_WORM, .group = 4 },

    // Charmable animals for a Ranger passing through: a lean Melandru's
    // Stalker and a Black Moa, the pre-Searing pets Regent Valley is known
    // for. Passive until provoked (low aggro), like the Lakeside Moa.
    { .kind = SPAWN_MONSTER, .name = "Melandru's Stalker", .pos = { -420, 200 },
      .level = 5, .hp = 140, .armor = 30, .aggro = 70.0f, .strengthRank = 5,
      .species = SPECIES_STALKER },
    { .kind = SPAWN_MONSTER_PATROL, .name = "Black Moa", .pos = { -500, -180 },
      .posB = { -300, -240 }, .level = 5, .hp = 130, .armor = 30, .aggro = 70.0f,
      .strengthRank = 5, .species = SPECIES_MOA },
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

    // Fire Imps - not a boss but ordinary enemies: fire elementalists that
    // travel in a pack of three, the classic big-XP kill a new caster hunts
    // in Wizard's Folly. Tougher than the local skale and grawl, and they
    // hit hard together, so the danger is the group, not a single champion.
    { .kind = SPAWN_MONSTER, .name = "Fire Imp", .pos = { 120, -180 },
      .level = 5, .hp = 140, .armor = 40, .aggro = 150.0f, .strengthRank = 7,
      .caster = true, .capSkill = -1, .species = SPECIES_IMP, .group = 4 },
    { .kind = SPAWN_MONSTER, .name = "Fire Imp", .pos = { 190, -210 },
      .level = 5, .hp = 140, .armor = 40, .aggro = 150.0f, .strengthRank = 7,
      .caster = true, .capSkill = -1, .species = SPECIES_IMP, .group = 4 },
    { .kind = SPAWN_MONSTER, .name = "Fire Imp", .pos = { 60, -230 },
      .level = 5, .hp = 140, .armor = 40, .aggro = 150.0f, .strengthRank = 7,
      .caster = true, .capSkill = -1, .species = SPECIES_IMP, .group = 4 },
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
    { .kind = SPAWN_NPC, .name = "Verata the Necromancer", .pos = { -120, 120 },
      .npcRole = NPC_QUEST_GIVER, .npcColor = { 86, 74, 104, 255 } },

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

    { .kind = SPAWN_NPC, .name = "Quartermaster Ferrick", .pos = { -300, -160 },
      .npcRole = NPC_COLLECTOR, .npcColor = { 168, 132, 96, 255 } },

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

// --- Foible's Fair: the bazaar up in Wizard's Folly's snowy heights ---

static const EnvProp g_foibleProps[] = {
    { {    0, -140 }, PROP_FIRE,  1.2f },
    { { -180, -120 }, PROP_TENT,  1.1f },
    { {  180, -130 }, PROP_TENT,  1.1f },
    { { -240,   50 }, PROP_TENT,  1.0f },
    { {  240,   60 }, PROP_TENT,  1.0f },
    { {  -80,  120 }, PROP_TENT,  0.9f },
    { {   90,  130 }, PROP_TENT,  0.9f },
    { { -320, -170 }, PROP_TREE,  1.0f },
    { {  330, -160 }, PROP_TREE,  1.0f },
    { {  -60,   40 }, PROP_GRASS, 1.0f },
    { {   70,  -20 }, PROP_GRASS, 0.9f },
};

static const SpawnDef g_foibleSpawns[] = {
    // Vassar runs the Domination Magic quest out of the Fair in GW1.
    { .kind = SPAWN_NPC, .name = "Vassar", .pos = { -120, -50 },
      .npcRole = NPC_QUEST_GIVER, .npcColor = { 90, 170, 90, 255 } },
    { .kind = SPAWN_NPC, .name = "Howland the Elementalist", .pos = { 130, -60 },
      .npcRole = NPC_QUEST_GIVER, .npcColor = { 90, 170, 90, 255 } },
    { .kind = SPAWN_NPC, .name = "Adept Mullenix", .pos = { 210, 90 },
      .npcRole = NPC_SKILL_TRAINER, .npcColor = { 90, 170, 90, 255 } },
    { .kind = SPAWN_NPC, .name = "Merchant Kaya", .pos = { -210, 80 },
      .npcRole = NPC_MERCHANT, .npcColor = { 90, 170, 90, 255 } },
};

// --- The Barradin Estate: the Duke's devourer-plagued vineyard ---

static const EnvProp g_estateProps[] = {
    { { -260, -160 }, PROP_TENT,  1.0f },
    { {  260, -150 }, PROP_TENT,  1.0f },
    { { -360, -220 }, PROP_TREE,  1.1f },
    { {  360, -210 }, PROP_TREE,  1.0f },
    { { -300,  180 }, PROP_TREE,  0.9f },
    { {  320,  190 }, PROP_TREE,  1.0f },
    { { -120,  120 }, PROP_ROCK,  0.9f },
    { {  140,  130 }, PROP_ROCK,  1.0f },
    { {  -40,  -40 }, PROP_GRASS, 1.0f },
    { {   60,   40 }, PROP_GRASS, 0.9f },
};

static const SpawnDef g_estateSpawns[] = {
    // Sandre Elek tends the vineyard and hands out its troubles.
    { .kind = SPAWN_NPC, .name = "Sandre Elek", .pos = { -300, -60 },
      .npcRole = NPC_QUEST_GIVER, .npcColor = { 176, 154, 96, 255 } },
    { .kind = SPAWN_MONSTER, .name = "Carrion Devourer", .pos = { 120, -40 },
      .level = 3, .hp = 130, .armor = 30, .aggro = 115.0f, .strengthRank = 4,
      .species = SPECIES_DEVOURER, .group = 5 },
    { .kind = SPAWN_MONSTER, .name = "Carrion Devourer", .pos = { 200, 30 },
      .level = 3, .hp = 130, .armor = 30, .aggro = 115.0f, .strengthRank = 4,
      .species = SPECIES_DEVOURER, .group = 5 },
    { .kind = SPAWN_MONSTER, .name = "Carrion Devourer", .pos = { 160, 110 },
      .level = 3, .hp = 130, .armor = 30, .aggro = 115.0f, .strengthRank = 4,
      .species = SPECIES_DEVOURER, .group = 5 },
    { .kind = SPAWN_MONSTER_PATROL, .name = "Ridgeback Devourer", .pos = { -200, 160 },
      .posB = { 260, 200 }, .level = 4, .hp = 160, .armor = 35, .aggro = 120.0f,
      .strengthRank = 5, .species = SPECIES_DEVOURER },
    // The Poison Devourer of GW1's quest of the same name.
    { .kind = SPAWN_MONSTER, .name = "The Poison Devourer", .pos = { 380, -120 },
      .level = 6, .hp = 300, .armor = 45, .aggro = 150.0f, .strengthRank = 8,
      .boss = true, .capSkill = SK_APPLY_POISON, .species = SPECIES_DEVOURER },
};

// --- Fort Ranik: Reforged Mode's frontier fort ---

static const EnvProp g_ranikProps[] = {
    { {    0, -140 }, PROP_FIRE, 1.1f },
    { { -190, -120 }, PROP_TENT, 1.0f },
    { {  190, -130 }, PROP_TENT, 1.0f },
    { { -300, -180 }, PROP_ROCK, 1.1f },
    { {  310, -170 }, PROP_ROCK, 1.0f },
    { { -260,  170 }, PROP_ROCK, 1.0f },
    { {  280,  180 }, PROP_ROCK, 0.9f },
};

static const SpawnDef g_ranikSpawns[] = {
    { .kind = SPAWN_NPC, .name = "Captain Arne", .pos = { -110, -50 },
      .npcRole = NPC_QUEST_GIVER, .npcColor = { 90, 170, 90, 255 } },
    { .kind = SPAWN_NPC, .name = "Quartermaster Vund", .pos = { 120, -60 },
      .npcRole = NPC_MERCHANT, .npcColor = { 90, 170, 90, 255 } },
    { .kind = SPAWN_NPC, .name = "Armorer Bael", .pos = { 200, 90 },
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
            // The gate in the wall - the only way north into the Charr
            // frontier. GW1 puts this passage in Lakeside, not the city.
            { { 720, -440 }, "To The Northlands",    ZONE_NORTHLANDS,     { -420, 0 } },
        },
        .portalCount = 5,
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
        },
        .portalCount = 1,
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
            { { 520, 0 }, "To The Barradin Estate", ZONE_BARRADIN_ESTATE, { -380, 0 } },
        },
        .portalCount = 2,
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
            // Reforged only - World_ZoneUnlocked hides the door otherwise.
            { {  980,    0 }, "To Fort Ranik",       ZONE_FORT_RANIK,      { -300,   0 } },
        },
        .portalCount = 3,
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
            // Foible's Fair sits up in the snowy heights of the Folly - the
            // fair is reached through here, not off Ashford Abbey.
            { { 300, -260 }, "To Foible's Fair", ZONE_FOIBLES_FAIR, { 0, 180 } },
        },
        .portalCount = 2,
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
            { { -460,  0 }, "To Lakeside County", ZONE_LAKESIDE_COUNTY, {  720, -380 } },
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
    [ZONE_FOIBLES_FAIR] = {
        .name = "Foible's Fair",
        .mode = MODE_OUTPOST,
        .clearColor = { 24, 22, 16, 255 },  // festival torchlight
        .gridColor = { 92, 82, 58, 255 },
        .portals = {
            { { -360, 0 }, "To Wizard's Folly", ZONE_WIZARDS_FOLLY, { 300, -200 } },
        },
        .portalCount = 1,
        .bounds = { -420, -280, 840, 560 },
        .hasShrine = false,
        .props = g_foibleProps, .propCount = COUNT(g_foibleProps),
        .spawns = g_foibleSpawns, .spawnCount = COUNT(g_foibleSpawns),
    },
    [ZONE_BARRADIN_ESTATE] = {
        .name = "The Barradin Estate",
        .mode = MODE_EXPLORABLE,
        .clearColor = { 19, 24, 16, 255 },  // vineyard green, going to seed
        .gridColor = { 68, 88, 54, 255 },
        .portals = {
            { { -460, 0 }, "To Green Hills County", ZONE_GREEN_HILLS, { 480, 0 } },
        },
        .portalCount = 1,
        .bounds = { -520, -320, 1040, 640 },
        .hasShrine = true,
        .shrinePos = { -360, -200 },
        .props = g_estateProps, .propCount = COUNT(g_estateProps),
        .spawns = g_estateSpawns, .spawnCount = COUNT(g_estateSpawns),
    },
    [ZONE_FORT_RANIK] = {
        .name = "Fort Ranik",
        .mode = MODE_OUTPOST,
        .clearColor = { 25, 22, 19, 255 },
        .gridColor = { 88, 78, 66, 255 },
        .portals = {
            { { -340, 0 }, "To Regent Valley", ZONE_REGENT_VALLEY, { -440, 0 } },
        },
        .portalCount = 1,
        .bounds = { -400, -280, 800, 560 },
        .hasShrine = false,
        .props = g_ranikProps, .propCount = COUNT(g_ranikProps),
        .spawns = g_ranikSpawns, .spawnCount = COUNT(g_ranikSpawns),
    },
};

// ---------------------------------------------------------------------
// Zone state + accessors
// ---------------------------------------------------------------------

static const ZoneDef *g_zone = &g_zones[ZONE_ASCALON_CITY];
static ZoneId g_zoneId = ZONE_ASCALON_CITY;
static ZoneId g_lastOutpostId = ZONE_ASCALON_CITY;
// GW1 has NO henchmen in pre-Searing: your party is you alone (or other
// human players), plus a Ranger's pet or a Necromancer's minions. Cynn,
// Little Thom, and the whole hero/henchman system are kept intact for the
// eventual post-Searing content - they just aren't spawned or hireable
// here. Flip this to 1 when post-Searing outposts arrive.
#define HENCHMEN_AVAILABLE 0

static bool g_thomHired = false;
// Whether the player currently has a charmed animal companion. Like
// g_thomHired it's party composition that outlives a single zone: set by
// Charm Animal, saved with the character, and read by LoadZone to respawn
// the pet in each new instance.
static bool g_petCharmed = false;
// The pet's OWN level and experience, GW1-style: a charmed animal starts
// low and levels up to 20 by fighting, independent of the player. Held
// here (not on the entity) because the pet entity is rebuilt every zone,
// so its progress has to live with the rest of the persistent party
// state and be saved alongside it.
static int g_petLevel = 0;
static int g_petXp = 0;
// Which animal the pet is - a Moa or a charmed Melandru's Stalker - so a
// respawned pet keeps the look of what you tamed. Saved with the rest.
static int g_petSpecies = SPECIES_MOA;
// Which outposts the player has set foot in, as a bitmask over ZoneId.
// GW1 lets you map-travel only to places you've already been; this is
// that memory. Saved with the character.
static unsigned g_visitedOutposts = 0;
static float g_portalCooldown = 0.0f;
static float g_wipeTimer = 0.0f;
static float g_autoResTimer = 0.0f;

GameMode World_GetMode(void) { return g_zone->mode; }
const char *World_GetZoneName(void) { return g_zone->name; }
ZoneId World_GetZoneId(void) { return g_zoneId; }
Color World_GetClearColor(void) { return g_zone->clearColor; }
Color World_GetGridColor(void) { return g_zone->gridColor; }
static bool g_searingHappened = false;

bool World_SearingHappened(void) {
    return g_searingHappened;
}

void World_SetSearingHappened(bool happened) {
    g_searingHappened = happened;
}

bool World_IsThomHired(void) { return g_thomHired; }

void World_SetThomHired(bool hired) {
    g_thomHired = hired;
    Save_Write(); // party composition is part of the saved character
}

bool World_IsPetCharmed(void) { return g_petCharmed; }

void World_SetPetCharmed(bool charmed) {
    g_petCharmed = charmed;
    Save_Write(); // the pet is part of the saved party too
}

int World_GetPetLevel(void) { return g_petLevel; }
int World_GetPetXp(void) { return g_petXp; }
int World_GetPetSpecies(void) { return g_petSpecies; }

void World_SetPetProgress(int level, int xp) {
    g_petLevel = level;
    g_petXp = xp;
}

void World_SetPetSpecies(int species) { g_petSpecies = species; }

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


// ---------------------------------------------------------------------
// Zone edges: a generated ridge instead of a drawn rectangle
//
// Walking the perimeter and dropping overlapping rock masses, each
// nudged inward by a different amount, gives every zone a ragged
// non-rectangular edge without a hand-authored blocker table per zone -
// and because the masses overlap, there is no seam to slip through.
//
// Spacing is deliberately well under twice the minimum radius. That
// overlap IS the wall; widen the spacing and the ridge becomes a row of
// boulders with gaps between them.

#define BARRIER_SPACING     42.0f
#define BARRIER_MIN_RADIUS  34.0f
#define BARRIER_MAX_RADIUS  56.0f
#define BARRIER_MAX_INSET   72.0f
// How much room a portal (or the shrine) needs kept clear. Burying the
// only way out of a zone under a mountain is the one failure mode this
// generator has, so the carve-out is generous.
#define BARRIER_CLEARANCE   110.0f

static ZoneBarrier g_barriers[MAX_ZONE_BARRIERS];
static int g_barrierCount = 0;

// Deterministic value hash - the same zone always generates the same
// ridge, so the world doesn't reshuffle every time you re-enter it.
static float BarrierHash01(unsigned a, unsigned b) {
    unsigned h = a * 374761393u + b * 668265263u;
    h = (h ^ (h >> 13)) * 1274126177u;
    h ^= h >> 16;
    return (float)(h & 0xFFFFFFu) / (float)0xFFFFFF;
}

// Would a mass here block something the player has to reach?
static bool BarrierWouldBlockExit(const ZoneDef *zone, Vector2 pos, float radius) {
    for (int i = 0; i < zone->portalCount; i++) {
        float dx = zone->portals[i].pos.x - pos.x;
        float dy = zone->portals[i].pos.y - pos.y;
        if (sqrtf(dx * dx + dy * dy) < radius + BARRIER_CLEARANCE) return true;
    }
    if (zone->hasShrine) {
        float dx = zone->shrinePos.x - pos.x, dy = zone->shrinePos.y - pos.y;
        if (sqrtf(dx * dx + dy * dy) < radius + BARRIER_CLEARANCE) return true;
    }
    return false;
}

static void AddBarrier(const ZoneDef *zone, unsigned zoneSeed, Vector2 pos, unsigned index) {
    if (g_barrierCount >= MAX_ZONE_BARRIERS) return;
    float r = BARRIER_MIN_RADIUS +
              BarrierHash01(zoneSeed, index * 3u + 1u) * (BARRIER_MAX_RADIUS - BARRIER_MIN_RADIUS);
    if (BarrierWouldBlockExit(zone, pos, r)) return;

    // Corners are walked twice - once by the horizontal pass, once by the
    // vertical - and without this the two chains pile up into a thicket
    // of cones instead of turning a corner.
    for (int i = 0; i < g_barrierCount; i++) {
        float dx = g_barriers[i].pos.x - pos.x, dy = g_barriers[i].pos.y - pos.y;
        if (dx * dx + dy * dy < (BARRIER_SPACING * 0.8f) * (BARRIER_SPACING * 0.8f)) return;
    }

    ZoneBarrier *b = &g_barriers[g_barrierCount++];
    b->pos = pos;
    b->radius = r;
    // Taller masses read as peaks and shorter ones as foothills, which is
    // what stops the ridge from looking like an extruded line.
    b->height = 0.75f + BarrierHash01(zoneSeed, index * 3u + 2u) * 0.85f;
    b->seed = (unsigned)(BarrierHash01(zoneSeed, index * 3u + 3u) * 100000.0f);
}

// Builds the ridge for a zone: four edges walked at a fixed spacing,
// each mass set back from the edge by its own amount.
static void BuildZoneBarriers(const ZoneDef *zone, ZoneId zoneId) {
    g_barrierCount = 0;
    unsigned seed = (unsigned)zoneId * 7919u + 13u;

    Rectangle b = zone->bounds;
    unsigned index = 0;

    // Top and bottom edges.
    int stepsX = (int)(b.width / BARRIER_SPACING) + 1;
    for (int i = 0; i <= stepsX; i++) {
        float x = b.x + (b.width * (float)i) / (float)stepsX;
        float insetTop = BarrierHash01(seed, index) * BARRIER_MAX_INSET;
        AddBarrier(zone, seed, (Vector2){ x, b.y + insetTop }, index);
        index++;
        float insetBot = BarrierHash01(seed, index) * BARRIER_MAX_INSET;
        AddBarrier(zone, seed, (Vector2){ x, b.y + b.height - insetBot }, index);
        index++;
    }

    // Left and right edges.
    int stepsY = (int)(b.height / BARRIER_SPACING) + 1;
    for (int i = 0; i <= stepsY; i++) {
        float y = b.y + (b.height * (float)i) / (float)stepsY;
        float insetL = BarrierHash01(seed, index) * BARRIER_MAX_INSET;
        AddBarrier(zone, seed, (Vector2){ b.x + insetL, y }, index);
        index++;
        float insetR = BarrierHash01(seed, index) * BARRIER_MAX_INSET;
        AddBarrier(zone, seed, (Vector2){ b.x + b.width - insetR, y }, index);
        index++;
    }
}

int World_GetBarrierCount(void) {
    return g_barrierCount;
}

const ZoneBarrier *World_GetBarrier(int index) {
    if (index < 0 || index >= g_barrierCount) return NULL;
    return &g_barriers[index];
}

void World_ResolveBarriers(Vector2 *pos, float moverRadius) {
    if (!pos) return;
    // Two passes: pushing out of one mass can push you into its
    // neighbour, and with overlapping circles a single pass leaves you
    // wedged in the seam between two of them.
    for (int pass = 0; pass < 2; pass++) {
        for (int i = 0; i < g_barrierCount; i++) {
            const ZoneBarrier *b = &g_barriers[i];
            float dx = pos->x - b->pos.x, dy = pos->y - b->pos.y;
            float minDist = b->radius + moverRadius;
            float d2 = dx * dx + dy * dy;
            if (d2 >= minDist * minDist) continue;
            float d = sqrtf(d2);
            if (d < 0.001f) {
                // Dead centre: no direction to push along, so pick one.
                pos->x = b->pos.x + minDist;
                continue;
            }
            pos->x = b->pos.x + dx / d * minDist;
            pos->y = b->pos.y + dy / d * minDist;
        }
    }
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
    p->blindMissFlashTimer = 0.0f;
    p->blockFlashTimer = 0.0f;
    Entity_BreakStance(p);
    for (int i = 0; i < SKILL_BAR_SIZE; i++) p->adrenaline[i] = 0;
    p->alive = true;
    p->morale = 0;
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
    hero->skillBar[3] = SK_RESURRECTION_SIGNET; // henchmen rez the fallen, GW1-style
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
    thom->skillBar[3] = SK_RESURRECTION_SIGNET; // henchmen rez the fallen, GW1-style
}

// Little Thom, the pre-Searing Warrior henchman. As a party member he
// fights with a fixed Warrior bar - fixed skill sets being exactly what
// separates henchmen from heroes in GW1
// (docs/research/gw1-mechanics.md #10).
static void SpawnThomCompanion(Vector2 pos) {
    int idx = Entity_Spawn(ENT_HERO, "Little Thom", 0, pos, (Color){ 170, 80, 60, 255 });
    World_SetupThomStats(Entity_Get(idx));
}

// GW1's pet starting level. A charmed animal begins at 5 and climbs to
// 20 by fighting - the same range the retail game uses for a wild pet.
#define PET_START_LEVEL 5

// A pet's health and per-hit damage, from its own level and the owner's
// Beast Mastery. Health rides on level (a higher-level pet is simply
// tougher); damage takes from both, so the attribute still matters - GW1's
// pet is only as dangerous as the Beast Mastery behind it.
static void PetStatsForLevel(int level, int beastRank,
                             int *maxHp, int *dmgMin, int *dmgMax) {
    if (maxHp)  *maxHp  = 60 + level * 13;             // L5=125 .. L20=320
    if (dmgMin) *dmgMin = 6 + level / 2 + beastRank / 2;
    if (dmgMax) *dmgMax = 10 + level + beastRank;      // L20/BM12 = 42
}

// The Beast Mastery rank the pet scales to - the player's, since the pet
// is theirs.
static int PetBeastRank(void) {
    const Entity *player = Entity_Get(PLAYER_INDEX);
    return player ? Entity_EffectiveRank(player, ATTR_BEAST_MASTERY) : 0;
}

// Turns an entity into the player's charmed companion at a given level -
// used both to convert a wild Moa when Charm Animal resolves and to
// respawn the pet on every zone load. Kept an ENT_HERO so the existing
// party AI (follow the player, engage aggroed foes, auto-attack) carries
// it with no special case - a pet has no skill bar, so it simply bites.
void World_SetupPetStats(Entity *pet, int level, int beastRank) {
    if (!pet) return;
    if (level < PET_START_LEVEL) level = PET_START_LEVEL;
    if (level > MAX_LEVEL) level = MAX_LEVEL;
    if (beastRank < 0) beastRank = 0;
    if (beastRank > ATTRIBUTE_RANK_CAP) beastRank = ATTRIBUTE_RANK_CAP;

    pet->kind = ENT_HERO;
    pet->team = 0;
    pet->isPet = true;
    pet->isHenchman = false;
    pet->npcRole = NPC_NONE;
    pet->isBoss = false;
    pet->capturedSkill = -1;
    // Keep the look of whatever was tamed.
    pet->species = g_petSpecies;
    if (g_petSpecies == SPECIES_STALKER) {
        pet->color = (Color){ 92, 110, 84, 255 }; // mottled hide
        strncpy(pet->name, "Melandru's Stalker", sizeof(pet->name) - 1);
    } else {
        pet->color = (Color){ 198, 158, 96, 255 }; // straw plumage
        strncpy(pet->name, "Moa Bird", sizeof(pet->name) - 1);
    }
    pet->name[sizeof(pet->name) - 1] = '\0';

    int maxHp, dmgMin, dmgMax;
    PetStatsForLevel(level, beastRank, &maxHp, &dmgMin, &dmgMax);
    pet->level = level;
    pet->baseMaxHp = pet->maxHp = maxHp;
    pet->hp = pet->maxHp;
    pet->baseMaxEnergy = pet->maxEnergy = 20; // pets never spend it; the base pool
    pet->energy = pet->maxEnergy;
    pet->morale = 0;

    pet->armor = 60;
    pet->attackDamageMin = dmgMin;
    pet->attackDamageMax = dmgMax;
    pet->attackRange = 28.0f;   // a beak, not a bow
    pet->attackInterval = 1.4f;
    pet->attackTimer = 0.0f;
    pet->moveSpeed = 110.0f;    // moa are quick on their feet
    pet->radius = 12.0f;

    // Scrub anything it carried as a wild monster: no skill bar, no
    // afflictions, no aggro/patrol state.
    for (int i = 0; i < SKILL_BAR_SIZE; i++) {
        pet->skillBar[i] = -1;
        pet->skillRecharge[i] = 0.0f;
        pet->adrenaline[i] = 0;
    }
    for (int i = 0; i < MAX_ACTIVE_EFFECTS; i++) pet->effects[i].active = false;
    pet->aggroed = false;
    pet->engaged = false;
    pet->hasMoveTarget = false;
    pet->hasPatrol = false;
    pet->groupId = 0;
    pet->knockdownTimer = 0.0f;
    pet->castingSlot = -1;
    pet->targetRef = Entity_NoRef();
    pet->castTargetRef = Entity_NoRef();
}

void World_CharmPet(Entity *animal, int beastRank) {
    if (!animal) return;
    // The pet inherits the wild animal's level as its starting point (at
    // least PET_START_LEVEL), then earns its way up from there.
    int start = animal->level;
    if (start < PET_START_LEVEL) start = PET_START_LEVEL;
    if (start > MAX_LEVEL) start = MAX_LEVEL;
    g_petLevel = start;
    g_petXp = 0;
    g_petSpecies = animal->species; // remember what was tamed, for respawns
    World_SetupPetStats(animal, g_petLevel, beastRank);
    World_SetPetCharmed(true); // saves the whole party state, pet included
}

void World_AwardPetXp(int monsterLevel) {
    if (!g_petCharmed || g_petLevel >= MAX_LEVEL) return;
    int petIdx = Entity_FindPet();
    if (petIdx < 0) return;
    Entity *pet = &g_entities[petIdx];
    if (!pet->alive) return; // a dead pet earns nothing, GW1's rule

    // Same shape as party kill XP, measured against the pet's own level.
    int diff = monsterLevel - g_petLevel;
    int xp = 100 + 24 * diff;
    if (xp < 16) xp = 16;
    if (xp > 400) xp = 400;
    g_petXp += xp;

    bool leveled = false;
    while (g_petLevel < MAX_LEVEL && g_petXp >= Progression_XPToNext(g_petLevel)) {
        g_petXp -= Progression_XPToNext(g_petLevel);
        g_petLevel++;
        leveled = true;
    }
    if (g_petLevel >= MAX_LEVEL) g_petXp = 0;

    if (leveled) {
        // Rescale the live pet to its new level and, GW1-style, heal it to
        // full on the level-up. Position/target state is left untouched.
        int maxHp, dmgMin, dmgMax;
        PetStatsForLevel(g_petLevel, PetBeastRank(), &maxHp, &dmgMin, &dmgMax);
        pet->level = g_petLevel;
        pet->baseMaxHp = pet->maxHp = maxHp;
        pet->hp = pet->maxHp;
        pet->attackDamageMin = dmgMin;
        pet->attackDamageMax = dmgMax;
        char msg[64];
        snprintf(msg, sizeof(msg), "%s reached level %d", pet->name, g_petLevel);
        UI_Notify(msg);
    }
}

int World_RaiseMinion(Vector2 pos, int deathRank) {
    if (deathRank < 0) deathRank = 0;
    if (deathRank > ATTRIBUTE_RANK_CAP) deathRank = ATTRIBUTE_RANK_CAP;

    // Bone grey, so a minion reads as yours-but-undead at a glance.
    int idx = Entity_Spawn(ENT_HERO, "Bone Horror", 0, pos, (Color){ 200, 205, 190, 255 });
    if (idx < 0) return -1;
    Entity *m = Entity_Get(idx);

    m->isMinion = true;
    m->species = SPECIES_UNDEAD;
    // Everything about it scales with Death Magic: a higher rank raises a
    // higher-level minion that is tougher, hits harder, and decays slower.
    m->level = 1 + deathRank;
    m->baseMaxHp = m->maxHp = 40 + deathRank * 10;   // rank 12 -> 160
    m->hp = m->maxHp;
    m->armor = 60;
    m->attackDamageMin = 4 + deathRank / 2;
    m->attackDamageMax = 8 + deathRank;              // rank 12 -> 20
    m->attackRange = 28.0f;
    m->attackInterval = 1.5f;
    m->moveSpeed = 95.0f;
    // Decay: health lost per second, slowed by Death Magic. Untended, a
    // rank-12 minion (160 hp, 1.6/s) lasts ~100s - long enough to fight
    // with, short enough that Blood of the Master earns its slot.
    m->minionDecayPerSec = 4.0f - (float)deathRank * 0.2f;
    if (m->minionDecayPerSec < 1.5f) m->minionDecayPerSec = 1.5f;
    m->minionDecayAccum = 0.0f;
    return idx;
}

// Respawns the charmed companion on zone load: a fresh, full-health Moa
// each instance, at its earned level and current Beast Mastery.
static void SpawnPet(Vector2 pos) {
    int idx = Entity_Spawn(ENT_HERO, "Moa Bird", 0, pos, (Color){ 198, 158, 96, 255 });
    if (idx < 0) return;
    World_SetupPetStats(Entity_Get(idx), g_petLevel, PetBeastRank());
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
    // Damage scales gently and LINEARLY with level now. The old
    // 4 + level*2 curve spiked hard - a pack of level-3 skale could drop
    // a fresh caster in a couple of seconds, which pre-Searing never
    // does. A level-1 foe should chip, not delete.
    m->attackDamageMin = 1 + def->level;
    m->attackDamageMax = 3 + def->level;
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
        m->attackDamageMin = (int)(m->attackDamageMin * 1.3f);
        m->attackDamageMax = (int)(m->attackDamageMax * 1.3f);
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
        // Give every NPC their own face from a hash of their name AND
        // spawn point, so a town reads as a crowd of different people -
        // and two "Ascalon Guard"s standing apart still look distinct.
        // Deterministic, so an NPC looks the same on every visit.
        unsigned h = 2166136261u;
        for (const char *c = def->name; *c; c++) h = (h ^ (unsigned char)*c) * 16777619u;
        h = (h ^ (unsigned)(int)def->pos.x) * 16777619u;
        h = (h ^ (unsigned)(int)def->pos.y) * 16777619u;
        npc->sex       = (int)(h & 1u);
        npc->skinTone  = (int)((h >> 1) % SKIN_TONE_COUNT);
        npc->hairColor = (int)((h >> 4) % HAIR_COLOR_COUNT);
        npc->hairStyle = (int)((h >> 8) % HAIR_STYLE_COUNT);
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
    Area_Reset();                        // wards/wells/spirits don't either
    AI_ClearPartyFlag();                 // a party flag doesn't cross zones
    MapDraw_Reset();                     // trail + map scribbles are per-instance
    Fx_Clear();

    g_entities[g_entityCount++] = saved;
    Entity *player = Entity_Get(PLAYER_INDEX);
    ResetPlayerTransientState(player, playerEntry);

    g_zone = &g_zones[zoneId];
    g_zoneId = zoneId;
    BuildZoneBarriers(g_zone, zoneId); // the ridge that shapes this zone's edge
    if (g_zone->mode == MODE_OUTPOST) {
        g_lastOutpostId = zoneId;
        g_visitedOutposts |= (1u << zoneId); // now a map-travel destination
    }
    g_portalCooldown = PORTAL_COOLDOWN;
    g_wipeTimer = 0.0f;
    g_autoResTimer = 0.0f;

    if (g_zone->mode == MODE_OUTPOST) {
        // Outposts restore the party completely, GW1-style.
        player->hp = player->maxHp;
        player->energy = player->maxEnergy;
    }

    // The party spawns around the player's entry point - but only where
    // GW1 actually offers henchmen (post-Searing). Pre-Searing is solo.
    if (HENCHMEN_AVAILABLE) {
        SpawnCynn((Vector2){ playerEntry.x - 50, playerEntry.y + 50 });
        if (g_thomHired) {
            SpawnThomCompanion((Vector2){ playerEntry.x + 30, playerEntry.y + 60 });
        }
    }
    // A charmed pet comes with you into every instance, GW1-style.
    if (g_petCharmed) {
        SpawnPet((Vector2){ playerEntry.x + 55, playerEntry.y + 45 });
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
                // The henchman-for-hire NPC only stands in the outpost
                // where henchmen exist at all (post-Searing), and not while
                // he's already in your party.
                if (def->npcRole == NPC_HENCHMAN && (!HENCHMEN_AVAILABLE || g_thomHired)) break;
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
    if (zone == ZONE_PIKEN_SQUARE || zone == ZONE_FORT_RANIK) return Character_IsReforged();
    return true;
}

bool World_IsOutpost(ZoneId zone) {
    return zone >= 0 && zone < ZONE_COUNT && g_zones[zone].mode == MODE_OUTPOST;
}

const char *World_ZoneName(ZoneId zone) {
    return (zone >= 0 && zone < ZONE_COUNT) ? g_zones[zone].name : "";
}

bool World_OutpostVisited(ZoneId zone) {
    return zone >= 0 && zone < ZONE_COUNT && (g_visitedOutposts & (1u << zone)) != 0;
}

unsigned World_VisitedMask(void) { return g_visitedOutposts; }
void World_SetVisitedMask(unsigned mask) { g_visitedOutposts = mask; }

bool World_TravelToOutpost(ZoneId zone) {
    // GW1's map travel: only to an unlocked outpost you have already
    // visited, and only from the safety of an outpost (never mid-fight).
    if (!World_IsOutpost(zone) || !World_ZoneUnlocked(zone) ||
        !World_OutpostVisited(zone)) return false;
    if (g_zone->mode != MODE_OUTPOST) return false;
    if (zone == g_zoneId) return false; // already here
    LoadZone(zone, (Vector2){ 0, 0 });
    return true;
}

void World_RestoreToOutpost(ZoneId zone) {
    if (zone < 0 || zone >= ZONE_COUNT || g_zones[zone].mode != MODE_OUTPOST ||
        !World_ZoneUnlocked(zone)) {
        zone = ZONE_ASCALON_CITY;
    }
    LoadZone(zone, (Vector2){ 0, 0 });
}

void World_Init(void) {
    // Fresh-start state, so a New Game from the menu after a previous
    // run doesn't inherit the old party composition.
    g_thomHired = false;
    g_petCharmed = false;
    g_petLevel = 0;
    g_petXp = 0;
    g_petSpecies = SPECIES_MOA;
    g_visitedOutposts = 0;
    g_lastOutpostId = ZONE_ASCALON_CITY;

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
    const char *startArmorSetName = "Ascalon";
    switch (g_character.primary) {
        case PROF_WARRIOR:
            player->skillBar[0] = SK_GASH;
            startWeapon = (Item){ .kind = ITEM_WEAPON, .name = "Ascalon Sword", .dmgMin = 13, .dmgMax = 20, .range = 28.0f, .attackInterval = 1.33f, .count = 1 };
            startArmor  = (Item){ .kind = ITEM_ARMOR, .armor = 40, .count = 1 };
            startArmorSetName = "Warrior Harness";
            break;
        case PROF_RANGER:
            player->skillBar[0] = SK_POWER_SHOT;
            // A bow: long reach, slow swing, and the only starting
            // weapon that lets you open a fight before it reaches you.
            startWeapon = (Item){ .kind = ITEM_WEAPON, .name = "Ascalon Longbow", .dmgMin = 12, .dmgMax = 21, .range = 240.0f, .attackInterval = 2.0f, .count = 1, .twoHanded = true };
            startArmor  = (Item){ .kind = ITEM_ARMOR, .armor = 35, .count = 1 };
            startArmorSetName = "Ranger Leathers";
            break;
        case PROF_MONK:
            player->skillBar[0] = SK_ORISON_OF_HEALING;
            startWeapon = (Item){ .kind = ITEM_WEAPON, .name = "Smiting Rod", .dmgMin = 11, .dmgMax = 22, .range = 160.0f, .attackInterval = 1.75f, .count = 1 };
            startArmor  = (Item){ .kind = ITEM_ARMOR, .armor = 30, .count = 1 };
            startArmorSetName = "Monk Raiment";
            break;
        case PROF_NECROMANCER:
            player->skillBar[0] = SK_VAMPIRIC_GAZE;
            startWeapon = (Item){ .kind = ITEM_WEAPON, .name = "Bone Idol", .dmgMin = 10, .dmgMax = 20, .range = 220.0f, .attackInterval = 1.75f, .count = 1 };
            startArmor  = (Item){ .kind = ITEM_ARMOR, .armor = 30, .count = 1 };
            startArmorSetName = "Necromancer Vestments";
            break;
        case PROF_MESMER:
            player->skillBar[0] = SK_ETHER_FEAST;
            startWeapon = (Item){ .kind = ITEM_WEAPON, .name = "Jeweled Wand", .dmgMin = 10, .dmgMax = 20, .range = 220.0f, .attackInterval = 1.75f, .count = 1 };
            startArmor  = (Item){ .kind = ITEM_ARMOR, .armor = 30, .count = 1 };
            startArmorSetName = "Mesmer Attire";
            break;
        case PROF_ELEMENTALIST:
        default:
            player->skillBar[0] = SK_FIRE_BOLT;
            startWeapon = (Item){ .kind = ITEM_WEAPON, .name = "Kindling Staff", .dmgMin = 11, .dmgMax = 22, .range = 220.0f, .attackInterval = 1.75f, .count = 1, .twoHanded = true };
            startArmor  = (Item){ .kind = ITEM_ARMOR, .armor = 30, .count = 1 };
            startArmorSetName = "Elementalist Robes";
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

    // Every character also carries a Resurrection Signet - GW1 hands it out
    // in the first hour of pre-Searing, and without a rez the party's death
    // system has no in-combat answer. Slotted last so it's ready to use.
    player->skillBar[SKILL_BAR_SIZE - 1] = SK_RESURRECTION_SIGNET;
    Skillbook_Unlock(SK_RESURRECTION_SIGNET);

    // A full five-piece set plus the weapon, GW1's actual starting kit.
    // One "armour" item was what made armour feel like a stat rather
    // than a wardrobe.
    Items_AddToInventory(startWeapon);
    for (int piece = EQUIP_HEAD; piece <= EQUIP_FEET; piece++) {
        Item p2 = startArmor;
        p2.slot = (EquipSlot)piece;
        snprintf(p2.name, sizeof(p2.name), "%s %s", startArmorSetName,
                 Items_SlotName((EquipSlot)piece));
        Items_AddToInventory(p2);
    }
    for (int i = 0; i < g_inventoryCount; i++) Items_Equip(player, i);

    // A new Prophecies character opens in Ascalon City, after the
    // intro cinematic - it is the capital, the hub, and where the
    // main questline starts. Ashford Abbey is somewhere you go.
    LoadZone(ZONE_ASCALON_CITY, (Vector2){ 0, 0 });
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
            // Minions are allied creatures, not party members: they neither
            // stave off a wipe nor get resurrected at the shrine.
            if (e->team != 0 || e->isMinion ||
                (e->kind != ENT_PLAYER && e->kind != ENT_HERO)) continue;
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
                    if (e->team != 0 || e->isMinion ||
                        (e->kind != ENT_PLAYER && e->kind != ENT_HERO)) continue;
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
                if (e->team == 0 && e->alive && !anchor && !e->isMinion &&
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
                        if (e->team != 0 || e->isMinion ||
                            (e->kind != ENT_PLAYER && e->kind != ENT_HERO) || e->alive) continue;
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
