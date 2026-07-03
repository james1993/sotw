#ifndef SKILL_H
#define SKILL_H

#include <stdbool.h>
#include "attributes.h"

#define MAX_SKILLS 32
#define MAX_STEPS_PER_SKILL 3

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
    int conditionKind;      // ConditionKind, only used by FX_APPLY_CONDITION
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
