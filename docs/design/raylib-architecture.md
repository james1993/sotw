# Technical Architecture — raylib implementation

raylib is a thin, immediate-mode C library (window/input/2D-3D
drawing/audio) — it gives you none of an ECS, scene graph, or scripting
layer. Given GW1's real complexity lives in its *skill/effect data*, not
its rendering, the architecture below is built around one central idea:

> **Skills are data, not code.** The engine provides a small, fixed
> vocabulary of effect primitives, targeting rules, and triggers; every
> one of GW1's ~1000 skills is expressed as a composition of that
> vocabulary in a JSON file. The C code that *executes* an effect graph
> is written once and never grows per-skill.

If this is wrong — if every skill ends up as bespoke C — the project
cannot scale past a handful of skills. Everything else in this document
serves that constraint.

## 1. Project layout

```
sotw/
  CMakeLists.txt              # FetchContent(raylib), single executable target
  src/
    main.c                    # window/game loop, top-level state machine
    world.h/.c                 # zone (outpost/explorable) load + tile grid + LoS
    entity.h/.c                 # struct Entity, entity pool (array, not pointers)
    attributes.h/.c             # attribute table, primary/secondary rules, 200pt budget
    skill.h/.c                   # Skill struct, SkillDB (loaded from data/skills/*.json)
    effect.h/.c                  # effect primitive VM: Damage/Heal/Condition/Hex/...
    combat.h/.c                   # resource pools (energy/adrenaline/overcast/hp),
                                   # cast bars, interrupts, aggro, projectiles
    ai_hero.h/.c                   # hero/monster behavior: target selection, skill
                                    # choice, flags/stances
    input.h/.c                      # click-to-move, target picking, skill-bar hotkeys
    ui_skillbar.h/.c                 # 8-slot bar, cooldown/energy overlays, cast bar
    ui_attributes.h/.c                # attribute panel
    render.h/.c                       # camera, Y-sorted draw, AoE footprint overlays
  data/
    skills/                      # one JSON per skill (or per-profession JSON array)
    professions.json             # primary attribute table, base stats
    zones/                       # tilemap + spawn table per zone
  assets/                     # sprites/audio once past placeholder-rectangle stage
```

Single executable, no dynamic linking of game logic — hot-reloading skill
JSON (not the C binary) is the fast-iteration loop, which matters a lot
given how much of the work is *content*, not engine code.

## 2. Entity representation: flat array, not an OOP hierarchy

```c
#define MAX_ENTITIES 256

typedef enum { ENT_PLAYER, ENT_HERO, ENT_HENCHMAN, ENT_MONSTER, ENT_SPIRIT } EntityKind;

typedef struct {
    bool active;
    EntityKind kind;
    Vector2 pos, moveTarget;
    bool hasMoveTarget;
    float moveSpeed;

    int team;                 // 0 = player party, 1 = hostile, etc.
    int hp, maxHp;
    int energy, maxEnergy;
    float energyRegenAccum;   // pips accumulate fractionally, tick every 3s in original
    int overcast;              // reduces effective maxEnergy while > 0
    int adrenaline;            // per-skill in the original; simplified to a shared pool

    int primaryProfession, secondaryProfession;
    int attributeRank[ATTR_COUNT];  // 0..12 (+rune bonuses applied separately)

    int skillBar[8];           // indices into g_skillDB, -1 = empty
    float skillRecharge[8];    // seconds remaining
    int castingSkillSlot;      // -1 if not casting
    float castTimeRemaining;
    bool interrupted;

    ActiveEffect effects[MAX_ACTIVE_EFFECTS]; // hexes/enchantments/conditions/stances
    int effectCount;
} Entity;

static Entity g_entities[MAX_ENTITIES];
```

No inheritance, no per-kind subclassing — a monster and a hero are the
same `Entity` struct with different `kind`/AI driver, exactly the way
GW1's own monsters and players share the same underlying character model
(a monster genuinely *has* an attribute bar and a skill bar internally).
This is a deliberate structural echo of the original, not just a
convenience.

## 3. The effect primitive VM

A skill is: `type`, `attribute`, cost fields (`energyCost`,
`adrenalineCost`, `healthSacrifice`, `overcast`), `castTime`,
`recharge`, `range`, a `targeting` rule, and a list of `EffectStep`s that
fire on defined `trigger`s.

```c
typedef enum {
    TARGET_SELF, TARGET_SINGLE_FOE, TARGET_SINGLE_ALLY,
    TARGET_TOUCH, TARGET_AOE_FOES, TARGET_AOE_ALLIES,
    TARGET_ALL_PARTY, TARGET_ALL_FOES_IN_EARSHOT
} TargetKind;

typedef enum {
    TRIGGER_ON_CAST_END, TRIGGER_ON_HIT, TRIGGER_ON_ENCHANT_END,
    TRIGGER_PERIODIC, TRIGGER_INSTANT
} TriggerKind;

typedef enum {
    FX_DAMAGE, FX_HEAL, FX_APPLY_CONDITION, FX_APPLY_HEX,
    FX_APPLY_ENCHANT, FX_APPLY_STANCE, FX_ENERGY_DELTA,
    FX_ADRENALINE_DELTA, FX_KNOCKDOWN, FX_INTERRUPT,
    FX_REMOVE_HEX, FX_REMOVE_CONDITION, FX_REMOVE_ENCHANT,
    FX_SUMMON_SPIRIT, FX_TELEPORT
} EffectKind;

typedef struct {
    EffectKind kind;
    TriggerKind trigger;
    float baseValue;          // e.g. base damage/heal
    float perAttributeRank;   // linear scaling term, matches wiki's per-skill tables
    int   conditionOrHexId;   // which condition/hex/enchant this applies or removes
    float duration;
} EffectStep;

typedef struct {
    char name[64];
    SkillTypeFlags typeFlags;     // Spell|Enchantment|Hex|Stance|Signet|Shout|...
    int profession;
    int attribute;
    int energyCost, adrenalineCost;
    float healthSacrificePct;
    float castTime, recharge;
    float range;
    bool isElite;
    TargetKind targeting;
    EffectStep steps[MAX_STEPS_PER_SKILL];
    int stepCount;
} Skill;
```

Executing a skill is: validate cost/recharge → resolve target set from
`targeting` → start cast bar if `castTime > 0` (skippable via interrupt) →
on `TRIGGER_ON_CAST_END`, run each `EffectStep` against the resolved
targets, applying `baseValue + perAttributeRank * rank[attribute]`
exactly as GW1's own skill tooltip formulas do. This one interpreter loop
is what all ~1000 skills eventually run through — adding skill #500 is a
JSON entry, not a new C function, as long as its behavior decomposes into
existing `EffectKind`s (the vast majority of GW1 skills do; a small tail
of truly bespoke skills — e.g. Spirit skills that redirect damage — get a
dedicated `EffectKind` case each, which is fine, that's a few dozen extra
enum cases total, not one per skill).

Example skill JSON (Elementalist "Fireball"-equivalent):

```json
{
  "name": "Fire Bolt",
  "typeFlags": ["Spell"],
  "profession": "Elementalist",
  "attribute": "FireMagic",
  "energyCost": 10,
  "castTime": 1.0,
  "recharge": 2.0,
  "range": 20.0,
  "targeting": "TARGET_SINGLE_FOE",
  "steps": [
    { "kind": "FX_DAMAGE", "trigger": "TRIGGER_ON_CAST_END",
      "baseValue": 5, "perAttributeRank": 3 }
  ]
}
```

## 4. Resource/casting/interrupt loop (per-frame, per-entity)

1. Tick energy regen (pip timer), overcast decay, adrenaline decay-on-death,
   effect durations (hexes/enchants/conditions), cooldowns.
2. If casting: decrement `castTimeRemaining`; if an interrupt effect
   landed this frame while `castTimeRemaining > 0`, cancel the cast,
   apply the interrupt skill's recharge penalty, skip step execution.
3. If `castTimeRemaining` reaches 0: resolve target set (re-validate
   range/LoS at cast-end, matching GW1's "target must still be valid"
   behavior), run `EffectStep`s, start `recharge` timer, clear
   `castingSkillSlot`.
4. AI/input layer (see §6) may queue a new skill activation, which is
   validated (cost, recharge, valid target in range/LoS) and, if legal,
   begins the cast (or resolves instantly for Signet/Shout/Stance types
   which have `castTime = 0` by data convention but still consume
   `recharge`).

Keeping this as one linear per-entity update means monsters, heroes, and
the player run through **exactly the same code path** — there is no
special-cased "player skill handler" vs "monster skill handler," only a
different decision layer feeding the same activation function. This
mirrors GW1's own client/server model where monsters are just AI-driven
characters using the same skill system as players.

## 5. Rendering

- Camera: `Camera2D` centered on the player, fixed zoom (adjustable),
  no rotation — a true top-down view rather than the original's angled
  3D camera, which is the standard, correct simplification for a 2D
  demake of this genre.
- Draw order: tilemap (ground) → AoE footprint overlays (translucent
  circles, drawn *under* entities) → entities sorted by `pos.y`
  (cheap pseudo-depth, standard top-down trick) → projectiles → floating
  combat text/cast bars → skill bar / attribute panel / party UI (drawn
  in screen space, unaffected by camera).
- Placeholder-first: v1 renders every entity as a tinted `DrawCircleV`/
  `DrawRectangleRec` (color = profession/team), which is enough to fully
  validate combat feel before any art asset exists. Sprites replace
  shapes later without touching combat code, since rendering reads
  `Entity.pos`/`kind`/`effects` and nothing else.

## 6. AI (heroes and monsters share one driver)

A lightweight utility-style decision loop, re-evaluated a few times a
second (not every frame — matches GW1's own hero AI "thinking" cadence
and saves CPU):

1. **Target selection**: nearest valid foe in aggro/earshot range,
   overridden by an explicit player "call target"; healers instead scan
   party HP% for the lowest-health ally in range.
2. **Positioning**: casters retreat if a foe is within melee range and
   the unit has a ranged skill queued; melee closes distance; all units
   respect a "flag" position if the player set one (matches GW1 hero
   flagging).
3. **Skill selection**: iterate the unit's skill bar in priority order
   (heroes: player-configurable priority list, closest analog to GW1's
   actual hero AI skill-priority settings; monsters: authored per-monster
   priority in zone data), pick the first skill that is off recharge,
   affordable, and has a legal target meeting its `targeting` rule.
4. Feed the chosen skill into the exact same activation function the
   player's input layer uses (§4) — no AI-only shortcuts.

## 7. Scenes: Outpost vs. Explorable, matching GW1's own split

Two `WorldMode`s, not a seamless open world (this is a feature, not a
limitation — it's how the original works and it simplifies save state
enormously):

- **Outpost**: no combat, skill trainers, merchant, skill-bar/attribute
  editing, party formation before entering an explorable zone. Safe to
  freely respec here, matching original rules.
- **Explorable**: the zone the current party is actually fighting in;
  loaded fresh (monster spawns, boss placement, loot table) each time the
  party enters, discarded on exit — mirrors the original's per-party
  instancing model, which conveniently also means we never need
  save/restore of mid-combat monster state.

Transition is a simple `enum GameMode { MODE_OUTPOST, MODE_EXPLORABLE }`
switched by a loading-screen state, with `World_Load(zoneId)` re-populating
the entity array from `data/zones/<zone>.json`.

## 8. Build system

`CMakeLists.txt` uses raylib's own recommended `FetchContent` integration
(pinned to a tagged release, not a floating branch) so the project builds
from a clean checkout with only a C toolchain and CMake — no manual
raylib install step, no committed binaries.

```cmake
cmake_minimum_required(VERSION 3.15)
project(sotw_demake C)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(
    raylib
    GIT_REPOSITORY https://github.com/raysan5/raylib.git
    GIT_TAG        5.5
)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(raylib)

add_executable(sotw_demake
    src/main.c src/world.c src/entity.c src/attributes.c
    src/skill.c src/effect.c src/combat.c src/ai_hero.c
    src/input.c src/ui_skillbar.c src/ui_attributes.c src/render.c)
target_link_libraries(sotw_demake PRIVATE raylib)
target_include_directories(sotw_demake PRIVATE src)
```

## 9. Why C (not C++) for this project

raylib is a C API and the effect-VM/data-driven approach above doesn't
need classes, templates, or RAII to stay clean — a flat struct + enum
dispatch table is exactly the right level of abstraction for "interpret a
small number of effect primitives over a flat entity array." C also keeps
the barrier to hot-reloading skill JSON and iterating on data low, which
matters more here than in a typical game because the *content* (skills)
is the deliverable, not just the engine.

## 10. Build/verification note for this environment

This sandboxed research session does not have outbound access to fetch
raylib's source (`github.com` archive downloads are blocked by the
network policy here), so the prototype in `src/` has **not** been
compiled or run in this session. It targets raylib 5.x's stable,
long-unchanged 2D API (`InitWindow`, `BeginDrawing`/`EndDrawing`,
`DrawRectangleRec`, `DrawCircleV`, `Camera2D`, `GetMousePosition`,
`CheckCollisionPointCircle`, etc.) deliberately conservatively to
maximize the odds it builds cleanly on a machine with normal internet
access; run `cmake -B build && cmake --build build` there to verify, and
treat this as a first pass to compile-check and iterate on, not
guaranteed-working code.
