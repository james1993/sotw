#ifndef ENTITY_H
#define ENTITY_H

#include "raylib.h"
#include "attributes.h"
#include <stdbool.h>

// Sized for a GW1-scale party (8) plus a zone's worth of camps and
// patrols, with headroom for spawned reinforcements.
#define MAX_ENTITIES 32

// Radius of the player's drawn "danger bubble" (the world-space ring in
// render.c and the compass ring in ui_compass.c - one constant so the
// two can't drift). Monster aggroRange values in zone data stay at or
// below this, so the bubble never under-promises who can notice you.
#define AGGRO_RING_RADIUS 130.0f
#define SKILL_BAR_SIZE 8
#define MAX_ACTIVE_EFFECTS 8

typedef enum {
    ENT_PLAYER,
    ENT_HERO,
    ENT_MONSTER,
    ENT_NPC      // outpost service NPCs: quest giver, merchant, henchman
} EntityKind;

// What the sprite renderer draws for this entity (sprite.c).
// Pre-Searing's bestiary. Each one gets its own body in sprite.c: a
// Skale that looks like a Charr tells the player the wrong thing about
// what it is and how hard it hits.
typedef enum {
    SPECIES_HUMAN = 0,   // also bandits
    SPECIES_CHARR,
    SPECIES_DEVOURER,    // also the giant spiders of Regent Valley
    SPECIES_SKALE,       // amphibian; the first thing you ever kill
    SPECIES_GRAWL,       // hunched ape-men off the hills
    SPECIES_MOA,         // flightless bird; harmless unless provoked
    SPECIES_UNDEAD,      // the Catacombs
    SPECIES_ALOE         // rooted plant; never moves
} Species;

typedef enum {
    NPC_NONE = 0,
    NPC_QUEST_GIVER,
    NPC_MERCHANT,
    NPC_HENCHMAN,
    NPC_CRAFTER, // armorer: crafts armor for gold + materials (GW1: armor is craft-only)
    NPC_SKILL_TRAINER, // sells non-elite skills for a skill point + gold
    NPC_PROFESSION_CHANGER, // grants, then later re-chooses, your second profession
    NPC_COLLECTOR  // trades a fixed count of one trophy for a fixed item
} NpcRole;

typedef enum {
    COND_NONE = 0,
    COND_BLEEDING,
    COND_BURNING,
    COND_POISON,     // -4 pips, and it overrides Bleeding's bar tint
    COND_CRIPPLED,
    COND_WEAKNESS,
    // Blind: your melee and missile ATTACKS have a 90% chance to miss
    // (GW1's exact number). It does no degeneration - like Crippled and
    // Weakness it impairs rather than wears down - and it only touches
    // attacks, never spells, which is why an Air Elementalist's Blinding
    // Flash shuts down a Warrior but does nothing to a caster.
    COND_BLIND,
    // Deep Wound is the odd one out: it does no degeneration at all.
    // It takes 20% off maximum health and 20% off healing received,
    // which is why a Warrior applies it before a spike rather than as
    // damage in its own right.
    COND_DEEP_WOUND,
    // Dazed: spells take twice as long to cast (and, in GW1, interrupt
    // far more easily - the demake models the cast-time half). It does no
    // degeneration; it just shuts a caster down.
    COND_DAZED,
    // Disease: -4 pips like Poison, and contagious - when a diseased
    // creature dies, others of its kind nearby catch it.
    COND_DISEASE,
    COND_COUNT
} ConditionKind;

// Hexes: the magical counterpart to conditions. GW1 keeps them strictly
// separate - a condition-removal skill cannot touch a hex and vice
// versa - and that separation is the whole point, because it forces a
// build to carry answers for both rather than one catch-all cleanse.
//
// These are named for the MECHANIC, not for a skill. GW1 has dozens of
// hexes that all do "your attacks come slower" (Faintheartedness,
// Shadow of Fear, Clumsiness...) and the demake models the effect once
// so a Necromancer's Curses and a Mesmer's Domination skills can reach
// for the same behaviour without a new enum per skill.
typedef enum {
    HEX_NONE = 0,
    HEX_FALTERING,  // attacks come slower, and the hex bleeds you slowly
    HEX_BACKLASH,   // attacking costs the hexed target health
    HEX_PHANTASM,   // pure health degeneration (Illusion: Conjure Phantasm)
    HEX_SLOWED,     // movement halved (Illusion: Imagined Burden)
    HEX_DIVERSION,  // the target's NEXT skill gets a big recharge penalty,
                    // stored in the effect's magnitude; consumed on use
    HEX_COUNT
} HexKind;

typedef enum {
    EFFECT_CONDITION = 0,
    EFFECT_HEX,
    // The third category GW1 builds its counterplay around: mostly
    // beneficial, always removable (by enchantment removal, which cannot
    // touch a condition or a hex, and vice versa). Modelled by MECHANIC,
    // like the others - one ENCH kind per behaviour, shared across every
    // skill that does that thing.
    EFFECT_ENCHANTMENT
} EffectCategory;

typedef enum {
    ENCH_NONE = 0,
    ENCH_REGEN,       // health regeneration, in pips (Mending, Healing Breeze)
    ENCH_BLOCK,       // a chance to block attacks (Guardian)
    ENCH_DAMAGE_CAP,  // caps each hit at magnitude * max health (Protective Spirit)
    ENCH_ARMOR,       // flat bonus armor while it holds (Armor of Earth)
    ENCH_REVERSAL,    // converts the next damage packet (up to magnitude)
                      // into healing, then ends (Reversal of Fortune)
    ENCH_COUNT
} EnchantKind;

typedef struct {
    bool active;
    EffectCategory category;
    // ConditionKind or HexKind depending on `category`. One slot array
    // holds both so a character has a single, bounded effect budget.
    int kind;
    float remaining;
    // Health degeneration in GW1's PIPS, not health per second - one pip
    // is 2 health a second, and the whole point of the unit is that
    // every source stacks into one capped total (see Combat_UpdateEntity).
    // A regenerating enchantment carries a NEGATIVE value here, so degen
    // and regen net against each other in the same summation - exactly
    // how GW1's health-drift arrows work.
    float degenPips;
    // The mechanic's parameter - enchantment block chance, damage-cap
    // fraction, bonus armor or reversal cap; and, for the Diversion hex,
    // the recharge penalty it will inflict. Unused effects leave it 0.
    float magnitude;
    // Maintained enchantments (Mending) drain this many energy-regen pips
    // while active - GW1's upkeep. 0 for everything else.
    int upkeepPips;
} ActiveEffect;


// Generational entity handle. A bare slot index stays "valid" after the
// entity in that slot dies AND after the slot is reused by a different
// entity - the second case silently retargets whoever moved in. The
// generation counter (bumped every time a slot is respawned) catches
// exactly that: a stale ref resolves to NULL instead of the wrong
// entity. Take refs with Entity_RefOf, read them with Entity_Resolve.
typedef struct {
    int idx;      // slot in g_entities, -1 = no entity
    unsigned gen; // g_entityGen[idx] at the time the ref was taken
} EntityRef;


// How long a melee swing animation runs. Long enough that the wind-up
// reads as a telegraph before the blow lands (sprite.c AttackSwing),
// short enough to stay inside the fastest weapon's attack interval.
#define ATTACK_ANIM_DURATION 0.42f

typedef struct Entity {
    bool alive;
    EntityKind kind;
    NpcRole npcRole; // only meaningful for ENT_NPC
    char name[32];
    int team; // 0 = player party, 1 = hostile

    Vector2 pos;
    Vector2 moveTarget;
    bool hasMoveTarget;
    float moveSpeed;
    float radius;
    Color color;

    int hp, maxHp;
    int energy, maxEnergy;
    // Death penalty (GW1's DP): each death costs 15% of max health and
    // energy, stacking to -60%, cleared by rezoning. maxHp/maxEnergy are
    // the *penalized* values; base* hold the real stats.
    int baseMaxHp, baseMaxEnergy;
    int deathPenalty; // percent, 0-60
    // Energy regeneration in GW1 pips. Everyone has 3 naturally; the
    // number exists as a field because pips are what skills, stances and
    // weapon mods actually move in GW1, not a flat "energy per second".
    int energyRegenPips;
    float energyRegenAccum;
    float hpRegenAccum;
    float degenAccum; // fractional health owed to degeneration
    // Soul Reaping's rolling throttle: at most 3 triggers per 15s.
    float soulReapingWindow;
    int soulReapingTriggers;
    float timeSinceCombat; // seconds since this entity last dealt or took damage
    // Adrenaline, per bar slot, in GW1's points (25 = one strike). One
    // shared pool is the wrong shape: GW1 charges every adrenal skill
    // together and drains the others when one is spent, and that
    // cross-drain is what stops a bar of adrenal skills firing at once.
    int adrenaline[SKILL_BAR_SIZE];

    bool isHenchman; // hired help - dismissible in outposts, unlike heroes

    // GW1-style armor level (AL): incoming damage is scaled by
    // 2^((60 - AL) / 40), GW1's actual armor formula against the AL 60
    // caster baseline. 60 = neutral.
    int armor;

    // Bonuses folded in from item upgrades (Items_RecomputeEquipped):
    // runes add to attributes and health, insignias to armour, weapon
    // mods to health, armour penetration and life-steal. Kept separate
    // from the base stats so re-equipping recomputes cleanly.
    int gearAttrBonus[ATTR_COUNT];
    int gearHealthBonus;   // runes (Vigor +, attribute runes -) + weapon Fortitude
    int gearArmorBonus;    // insignias
    int weaponArmorPen;    // Sundering: % pen on basic attacks
    int weaponLifesteal;   // Vampiric: health per basic hit
    // Armour dye, applied to the sprite. dyed=false keeps the default.
    Color dyeColor;
    bool dyed;

    int level;
    int xp;              // toward the next level
    int attributePoints; // earned but unspent

    Profession primaryProfession;
    Profession secondaryProfession;
    // Profession trainers (ENT_NPC, NPC_PROFESSION_CHANGER): the one
    // profession this NPC teaches.
    Profession teachesProfession;
    int attributeRank[ATTR_COUNT];

    int skillBar[SKILL_BAR_SIZE];      // index into g_skillDB, -1 = empty
    float skillRecharge[SKILL_BAR_SIZE];

    int castingSlot;        // -1 if not casting
    float castTimeRemaining;
    float castTimeTotal;
    EntityRef castTargetRef; // resolved target of the cast in progress -
                             // separate from targetRef so a self-fallback
                             // heal doesn't stomp your selected target

    EntityRef targetRef;    // current target, Entity_Resolve to read
    // Selecting a foe and FIGHTING it are two different things, exactly
    // as they are in GW1. targetRef alone only puts a foe on the HUD;
    // nothing closes the distance or swings until `engaged` is set,
    // which takes an attack order or a skill cast. Any movement order
    // clears it - that is how you call off a charge.
    bool engaged;
    float attackTimer;
    float attackInterval;
    int attackDamageMin, attackDamageMax;
    float attackRange;

    float interruptFlashTimer; // > 0 briefly after being interrupted, for UI feedback
    float dodgeFlashTimer;     // > 0 briefly after dodging a projectile
    float blindMissFlashTimer; // > 0 briefly after an attacker's Blind made them miss us
    float blockFlashTimer;     // > 0 briefly after a defensive stance blocked an attack

    // Defensive stance state. A stance is neither a condition nor a hex -
    // it is a self-buff a Warrior presses to survive a spike, with its
    // own timer. While it holds, incoming ATTACKS (not spells) are rolled
    // against blockChance and armor gets stanceArmorBonus. Disciplined
    // Stance's real drawback is that it ends the moment you use an
    // adrenal skill, so this is cleared there too.
    float stanceTimer;
    float blockChance;      // 0..1 chance to block an incoming attack
    int   stanceArmorBonus; // added to armor while the stance holds

    // Knockdown: while this is > 0 the entity can't move, attack or cast -
    // GW1's knockdown is a real timed lockout, not just a cancelled action.
    float knockdownTimer;
    // Exhaustion (Elementalist): each overcast skill adds to this and it
    // lowers the energy ceiling until it slowly recovers. In energy points.
    float exhaustion;
    // The recharge penalty a Diversion hex has armed for this entity's
    // next skill use; added to that skill's recharge, then cleared.
    float divPenalty;

    // Target-panel display: which skill to show as "currently/recently
    // used" (see ui_target.c). Set whenever a skill is activated; the
    // post-cast window keeps it visible for a few seconds after a cast
    // resolves or gets interrupted, matching how a GW1 target bar shows
    // what your target just did, not their whole skill bar.
    int lastCastSkillSlot;
    float postCastDisplayTimer;
    bool lastCastInterrupted;

    // Aggro/leash (monsters only - see docs/research/gw1-mechanics.md #7
    // and ai_hero.c). A monster is passive until something enters
    // aggroRange or hits it; if it or its target strays more than
    // leashRange from spawnPos, it gives up, walks home, and resets.
    Vector2 spawnPos;
    float aggroRange;
    float leashRange;
    bool aggroed;
    // Spawn group (monsters): > 0 links campmates together so they
    // aggro as one - pull any member and the whole group joins, like a
    // GW1 mob group. 0 = ungrouped (patrols hunt alone).
    int groupId;

    // Boss (monsters): tougher, visually marked, and the only source of
    // elite skills. capturedSkill is the SkillId this boss teaches when
    // killed (-1 = nothing), standing in for GW1's Signet of Capture.
    bool isBoss;
    int capturedSkill;

    // --- Sprite animation state (sprite.c) ---
    Species species;
    // Appearance chosen at character creation (character.h). Only the
    // player sets these; everyone else keeps the defaults.
    int sex;
    int skinTone;
    int hairColor;
    int hairStyle;
    Vector2 prevPos;        // last frame's position - drives the walk cycle
    Vector2 facing;         // unit vector of last movement direction
    float animTime;         // advances with distance walked
    float moveBlend;        // 0..1, eases the walk cycle in and out
    float attackAnimTimer;  // > 0 during the swing animation

    // Patrol route: monsters with hasPatrol ping-pong between patrolA
    // and patrolB while idle, scanning for foes the whole way - the
    // GW1 patrols that punish a badly timed pull by wandering into it.
    bool hasPatrol;
    Vector2 patrolA, patrolB;
    int patrolDir; // +1 toward B, -1 toward A

    ActiveEffect effects[MAX_ACTIVE_EFFECTS];
} Entity;

extern Entity g_entities[MAX_ENTITIES];
extern int g_entityCount;
extern unsigned g_entityGen[MAX_ENTITIES]; // per-slot generation counters

int Entity_Spawn(EntityKind kind, const char *name, int team, Vector2 pos, Color color);
Entity *Entity_Get(int index);
bool Entity_IsCasting(const Entity *e);

// Generational handles (see EntityRef above).
EntityRef Entity_NoRef(void);
EntityRef Entity_RefOf(int index);        // ref to a current slot, or NoRef
Entity *Entity_Resolve(EntityRef ref);    // NULL when none or stale
int Entity_RefIndex(EntityRef ref);       // slot index while valid, else -1

// Applies armor-scaled damage. `attacker` may be NULL (e.g. condition
// ticks). Monster deaths award party XP and roll loot drops here, so
// every damage source shares one death path.
void Entity_ApplyDamage(Entity *e, int amount, Entity *attacker);

// As above, but ignoring `armorPenetration` (0..1) of the target's
// armor - Strength on attack skills, and anything else that penetrates.
void Entity_ApplyDamagePen(Entity *e, int amount, Entity *attacker, float armorPenetration);

// Seconds between whole points of energy, from this entity's pip count.
// GW1's clock exactly: one pip is 1 energy per 3 seconds and everyone
// has 3 pips, so the baseline is 1 energy per second.
float Entity_EnergyRegenInterval(const Entity *e);

// Recomputes what GW1 derives from level and attributes - maximum health
// from level, maximum energy from Energy Storage - then reapplies the
// death penalty. Call after changing either. Only meaningful for
// characters; monsters carry hand-authored stat blocks.
void Entity_RecomputeAttributeStats(Entity *e);

// Resets the out-of-combat regen timer. Called whenever an entity deals
// or takes damage, matching GW1's "recent combat activity blocks fast
// regen" rule (see docs/research/gw1-mechanics.md - health here isn't a
// GW1 resource with its own pips, but the in/out-of-combat regen split
// is a faithful simplification of the same idea).
void Entity_MarkInCombat(Entity *e);

// Wakes a sleeping monster onto a foe and, when it belongs to a spawn
// group, wakes every groupmate onto the same foe - GW1 mobs aggro as a
// group: pull one and its campmates all come. This is THE aggro entry
// point; every path that used to flip `aggroed` directly (proximity
// scan, melee hit, spell damage, projectile impact) goes through here
// so no path can forget the group.
void Entity_WakeMonsterGroup(Entity *monster, EntityRef foe);

// Recomputes penalized maxHp/maxEnergy from base stats and the current
// death penalty. Call after changing baseMax*, deathPenalty, or both.
void Entity_RecomputePenalizedStats(Entity *e);

// --- Conditions and hexes -------------------------------------------
//
// Effects are only worth applying if they change what a character can
// do, so every one of these is read from the place that governs the
// behaviour it impairs, rather than being checked ad hoc at call sites.

bool Entity_HasCondition(const Entity *e, ConditionKind kind);
bool Entity_HasHex(const Entity *e, HexKind kind);
bool Entity_IsEnchanted(const Entity *e);

// Applies a condition directly (refreshing if already present). The
// effect VM has its own richer path; this is for code outside it - the
// disease-contagion spread on death.
void Entity_InflictCondition(Entity *e, ConditionKind kind, float duration);

// The best block chance from any source right now: a defensive stance
// (Disciplined Stance) or a block enchantment (Guardian), whichever is
// higher. Attacks roll against this in Entity_ResolveAttack.
float Entity_BlockChance(const Entity *e);

// Sum of ENCH_ARMOR bonuses currently on the entity - added to worn AL
// in the damage formula.
int Entity_BonusArmor(const Entity *e);

// Total energy-regen pips drained by maintained enchantments right now.
int Entity_UpkeepPips(const Entity *e);

// The tightest per-hit damage cap as a fraction of max health (Protective
// Spirit is 0.10), or 1.0 when nothing caps damage.
float Entity_DamageCapFraction(const Entity *e);

// The outcome of an ATTACK (a basic swing/shot or an attack skill) once
// Blind and block are accounted for. Spells never go through this - GW1's
// Blind and block only ever touch attacks.
typedef enum {
    ATTACK_LANDS,
    ATTACK_MISS_BLIND,  // the attacker was Blind and rolled the 90% miss
    ATTACK_BLOCKED      // the defender's stance blocked it
} AttackOutcome;

// Rolls Blind (attacker) then block (defender) for one attack. On a
// miss or block it sets the matching flash timer on `defender` for the
// on-screen callout and returns why; callers apply damage only on
// ATTACK_LANDS. Pure attacks only - do not call this for spells.
AttackOutcome Entity_ResolveAttack(const Entity *attacker, Entity *defender);

// Ends any defensive stance immediately (Disciplined Stance's "ends if
// you use an adrenal skill" rule, and zone/respawn cleanup).
void Entity_BreakStance(Entity *e);

// How many conditions / hexes are currently on this character - drives
// both the nameplate pips and what a removal skill has to work with.
int Entity_CountEffects(const Entity *e, EffectCategory category);

// Strips up to `maxCount` effects of one category, oldest-expiring
// first so a cleanse takes the affliction you'd otherwise wait longest
// on. Returns how many actually came off.
int Entity_RemoveEffects(Entity *e, EffectCategory category, int maxCount);

// Movement speed after Crippled. Used by every mover (click-to-move,
// gamepad stick, AI chase) so being crippled slows you the same way no
// matter how you're steering.
float Entity_MoveSpeed(const Entity *e);

// Seconds between auto-attacks after any attack-slowing hex.
float Entity_AttackInterval(const Entity *e);

// Outgoing attack damage after Weakness.
int Entity_ScaleOutgoingDamage(const Entity *e, int damage);

// An attribute rank as it is USED, after Weakness. GW1's Weakness drops
// every attribute by 1 - except ranks already at 0, which stay there -
// and that is half of what makes the condition worth applying.
//
// Deliberately NOT used for the derived maximums (Energy Storage's
// energy pool, Soul Reaping's return): those are recomputed only when
// ranks actually change, and driving them from a condition would make a
// character's maximum energy flicker as Weakness came and went. Every
// USE-time read goes through here.
// Healing received, after Deep Wound.
int Entity_ScaleIncomingHeal(const Entity *e, int amount);

int Entity_EffectiveRank(const Entity *e, AttributeKind attr);

// One landed strike: every adrenal skill on the bar gains 25 points.
void Entity_GainAdrenalineStrike(Entity *e);

// Spending: the fired slot empties and every other adrenal skill loses
// a strike's worth. GW1's rule, and the reason a bar of adrenal skills
// can't all be charged at once.
void Entity_SpendAdrenaline(Entity *e, int slot);

// Raw points onto every adrenal slot - what a skill that "grants
// adrenaline" does. Negative strips it.
void Entity_AddAdrenalinePoints(Entity *e, int points);

// Display name / color for an active effect, shared by every readout.
const char *Entity_EffectName(const ActiveEffect *fx);
Color Entity_EffectColor(const ActiveEffect *fx);

#endif
