#ifndef SKILL_H
#define SKILL_H

#include <stdbool.h>
#include "attributes.h"

#define MAX_SKILLS 80
#define MAX_STEPS_PER_SKILL 3

// Stable names for every skill in the DB. SkillDB_Init registers in
// exactly this order (and warns loudly if the two ever drift), so skill
// bars reference these instead of bare numbers - inserting a skill
// mid-table can no longer silently renumber every bar in the game.
typedef enum {
    SK_GASH = 0,
    SK_RUSH_STRIKE,
    SK_BATTLE_CRY,
    SK_DEATHBLOW,
    SK_FIRE_BOLT,
    SK_CINDER_STORM,
    SK_MIND_SEAR,
    SK_METEOR,
    SK_CLAW_SWIPE,
    SK_DISTRACTING_BLOW,
    SK_FERAL_HOWL,
    SK_ORISON_OF_HEALING,
    SK_BANISH,
    SK_SMITE,
    SK_BANE_SIGNET,
    SK_HEALING_LIGHT,
    SK_SHROUD_OF_DOUBT,
    SK_PRICE_OF_FAITH,
    SK_MEND_AILMENT,
    SK_SMITE_HEX,
    SK_RENDING_CLAWS,
    SK_HOBBLING_STRIKE,
    // Ranger
    SK_POWER_SHOT,
    SK_PIN_DOWN,
    SK_TROLL_UNGUENT,
    SK_APPLY_POISON,
    // Necromancer
    SK_VAMPIRIC_GAZE,
    SK_FAINTHEARTEDNESS,
    SK_BARBED_SIGNET,
    // Mesmer
    SK_ETHER_FEAST,
    SK_EMPATHY,
    SK_SHATTER_DELUSIONS,
    // Appended, not inserted, so every existing SkillId keeps its number
    // and old saved skill bars still point where they meant to.
    // Warrior Tactics defensive stance - the home of the block mechanic.
    SK_DISCIPLINED_STANCE,
    // Elementalist Air Magic - Blinding Flash, the game's iconic Blind.
    SK_BLINDING_FLASH,
    // Warrior Axe Mastery
    SK_EVISCERATE,          // elite: heavy damage + Deep Wound
    SK_EXECUTIONERS_STRIKE,
    SK_CYCLONE_AXE,         // hits all adjacent foes
    SK_PENETRATING_BLOW,    // inherent armor penetration
    // Warrior Hammer Mastery
    SK_HAMMER_BASH,         // knockdown
    SK_MIGHTY_BLOW,
    SK_CRUSHING_BLOW,       // Deep Wound
    // The empty lines, filled out
    SK_GUARDIAN,            // Protection Prayers: grants an ally block
    SK_DEATHLY_SWARM,       // Death Magic damage
    SK_CONJURE_PHANTASM,    // Illusion Magic degen hex
    SK_SHARD_STORM,         // Water Magic damage + Cripple
    SK_STONING,             // Earth Magic damage + knockdown
    SK_FEROCIOUS_STRIKE,    // Beast Mastery attack
    // Enchantments
    SK_MENDING,             // Healing Prayers: sustained health regen
    SK_HEALING_BREEZE,      // Healing Prayers: strong short regen
    SK_PROTECTIVE_SPIRIT,   // Protection: caps each hit at 10% max health
    SK_ARMOR_OF_EARTH,      // Earth Magic: big armor bonus
    SK_SHATTER_ENCHANTMENT, // Domination: strips an enchantment + damage
    // Effect-system extensions (B)
    SK_ROTTING_FLESH,       // Death Magic: Disease
    SK_IMAGINED_BURDEN,     // Illusion: movement-slow hex
    SK_DIVERSION,           // Domination: next-skill recharge hex
    SK_REVERSAL_OF_FORTUNE, // Protection: next damage -> healing enchantment
    SK_CONCUSSION_SHOT,     // Marksmanship: interrupt + Dazed
    SK_COUNT
} SkillId;

// Mirrors GW1's skill-type hierarchy at a reduced scale (see
// docs/research/gw1-mechanics.md #5). What matters for the demake is
// that TYPE, not just effect, is what removal/interrupt skills key off.
typedef enum {
    SKILLTYPE_SPELL,
    SKILLTYPE_ATTACK_SKILL,
    SKILLTYPE_STANCE,
    SKILLTYPE_SHOUT,
    SKILLTYPE_SIGNET
} SkillType;

typedef enum {
    TARGET_SELF,
    TARGET_SINGLE_FOE,
    TARGET_SINGLE_ALLY,
    TARGET_AOE_FOES
} TargetKind;

// The fixed vocabulary of effect primitives every skill is composed from.
// See docs/design/raylib-architecture.md #3 - adding a skill should mean
// adding data, not adding a new EffectKind.
typedef enum {
    FX_DAMAGE,
    FX_HEAL,
    FX_APPLY_CONDITION,
    // Hexes are a separate category on purpose: FX_REMOVE_CONDITION
    // cannot touch them and FX_REMOVE_HEX cannot touch a condition, so
    // a build has to answer both rather than packing one cure-all.
    FX_APPLY_HEX,
    // Enchantments are the third affliction category. conditionKind
    // carries an EnchantKind, baseValue/perAttributeRank the mechanic's
    // magnitude (regen pips, block chance, damage-cap fraction, armor),
    // duration the length. Removal is its own step, and - like the other
    // two categories - cannot touch a condition or a hex.
    FX_APPLY_ENCHANTMENT,
    FX_REMOVE_CONDITION,
    FX_REMOVE_HEX,
    FX_REMOVE_ENCHANTMENT,
    FX_ENERGY_DELTA,
    FX_ADRENALINE_DELTA,
    FX_KNOCKDOWN,
    // A defensive stance on the caster: baseValue is the block chance
    // (0..1), conditionKind carries a flat armor bonus, duration is how
    // long it holds. Blocking is a genuine GW1 mechanic in its own right,
    // not a condition or a hex, so it needs its own primitive.
    FX_STANCE_BLOCK,
    // Cancels the target's current cast (if any) and applies an extra
    // recharge penalty to the interrupted skill, same as GW1's interrupt
    // skills - the whole reason cast time exists as a distinct mechanic
    // from instant skills is so interrupts have something to punish.
    FX_INTERRUPT
} EffectKind;

typedef struct {
    EffectKind kind;
    float baseValue;
    float perAttributeRank; // linear scaling term against skill->attribute
    // FX_APPLY_CONDITION: a ConditionKind. FX_APPLY_HEX: a HexKind.
    // FX_APPLY_ENCHANTMENT: an EnchantKind. FX_REMOVE_*: how many to strip.
    int conditionKind;
    float duration;
    // FX_APPLY_ENCHANTMENT only: energy-regen pips this enchantment
    // maintains (drains) while active - GW1 upkeep. 0 = not maintained.
    int upkeep;
    // Applies to the CASTER rather than the skill's target. This is what
    // life-stealing and energy-stealing skills are made of: one step
    // takes from the foe, the next gives to you, in a single skill.
    bool selfTarget;
} EffectStep;

typedef struct {
    char name[32];
    SkillType type;
    AttributeKind attribute;
    int energyCost;
    int adrenalineCost; // 0-100 scale, simplified from per-skill adrenaline strikes
    float castTime;
    float recharge;
    float range;
    float aoeRadius; // TARGET_AOE_FOES: blast radius around the target
    bool isElite;
    // Overcast Elementalist skills (Meteor) add Exhaustion when used,
    // temporarily lowering the caster's energy ceiling.
    bool exhausting;
    // Inherent armor penetration (0..1) this skill carries regardless of
    // Strength - Penetrating Blow and the like. Strength's penetration is
    // added on top of this in the effect VM.
    float armorPen;
    TargetKind targeting;
    EffectStep steps[MAX_STEPS_PER_SKILL];
    int stepCount;
} Skill;

extern Skill g_skillDB[MAX_SKILLS];
extern int g_skillCount;

void SkillDB_Init(void);
int SkillDB_Register(Skill skill);

#endif
