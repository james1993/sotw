#ifndef SKILL_H
#define SKILL_H

#include <stdbool.h>
#include "attributes.h"

#define MAX_SKILLS 32
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
    FX_REMOVE_CONDITION,
    FX_REMOVE_HEX,
    FX_ENERGY_DELTA,
    FX_ADRENALINE_DELTA,
    FX_KNOCKDOWN,
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
    // FX_REMOVE_*: how many effects to strip.
    int conditionKind;
    float duration;
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
    TargetKind targeting;
    EffectStep steps[MAX_STEPS_PER_SKILL];
    int stepCount;
} Skill;

extern Skill g_skillDB[MAX_SKILLS];
extern int g_skillCount;

void SkillDB_Init(void);
int SkillDB_Register(Skill skill);

#endif
