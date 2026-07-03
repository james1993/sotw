#include "skill.h"
#include "entity.h"
#include <string.h>

// This is a small hand-authored slice standing in for what would be a
// data/skills/*.json load in the full design (see
// docs/design/raylib-architecture.md #3). Kept as inline C for the
// prototype to avoid pulling in a JSON dependency before the effect VM
// itself is proven out.

Skill g_skillDB[MAX_SKILLS];
int g_skillCount = 0;

int SkillDB_Register(Skill skill) {
    if (g_skillCount >= MAX_SKILLS) return -1;
    g_skillDB[g_skillCount] = skill;
    return g_skillCount++;
}

static Skill MakeSkill(const char *name, SkillType type, AttributeKind attr,
                        int energyCost, int adrenalineCost, float castTime,
                        float recharge, float range, bool elite, TargetKind targeting) {
    Skill s;
    memset(&s, 0, sizeof(Skill));
    strncpy(s.name, name, sizeof(s.name) - 1);
    s.type = type;
    s.attribute = attr;
    s.energyCost = energyCost;
    s.adrenalineCost = adrenalineCost;
    s.castTime = castTime;
    s.recharge = recharge;
    s.range = range;
    s.isElite = elite;
    s.targeting = targeting;
    s.stepCount = 0;
    return s;
}

static void AddStep(Skill *s, EffectKind kind, float base, float perRank, int cond, float duration) {
    if (s->stepCount >= MAX_STEPS_PER_SKILL) return;
    EffectStep *step = &s->steps[s->stepCount++];
    step->kind = kind;
    step->baseValue = base;
    step->perAttributeRank = perRank;
    step->conditionKind = cond;
    step->duration = duration;
}

// Skill DB indices, in registration order below:
//  0 Gash            4 Fire Bolt      8 Claw Swipe (monster)
//  1 Rush Strike      5 Cinder Storm    9 Distracting Blow
//  2 Battle Cry        6 Mind Sear      10 Feral Howl (monster)
//  3 Deathblow (elite)  7 Meteor (elite)
void SkillDB_Init(void) {
    g_skillCount = 0;
    Skill s;

    // --- Warrior-like ("Brute") skills: adrenaline, melee, no cast time ---
    s = MakeSkill("Gash", SKILLTYPE_ATTACK_SKILL, ATTR_STRENGTH,
                  0, 25, 0.0f, 4.0f, 28.0f, false, TARGET_SINGLE_FOE);
    AddStep(&s, FX_DAMAGE, 8, 1.5f, 0, 0);
    AddStep(&s, FX_APPLY_CONDITION, 0, 0, COND_BLEEDING, 8.0f);
    SkillDB_Register(s);

    s = MakeSkill("Rush Strike", SKILLTYPE_ATTACK_SKILL, ATTR_STRENGTH,
                  0, 25, 0.0f, 6.0f, 28.0f, false, TARGET_SINGLE_FOE);
    AddStep(&s, FX_DAMAGE, 12, 2.0f, 0, 0);
    SkillDB_Register(s);

    s = MakeSkill("Battle Cry", SKILLTYPE_SHOUT, ATTR_TACTICS,
                  0, 0, 0.0f, 20.0f, 0.0f, false, TARGET_SELF);
    AddStep(&s, FX_ADRENALINE_DELTA, 15, 0, 0, 0);
    SkillDB_Register(s);

    s = MakeSkill("Deathblow", SKILLTYPE_ATTACK_SKILL, ATTR_STRENGTH,
                  0, 40, 0.0f, 10.0f, 28.0f, true, TARGET_SINGLE_FOE);
    AddStep(&s, FX_DAMAGE, 20, 3.0f, 0, 0);
    AddStep(&s, FX_KNOCKDOWN, 0, 0, 0, 2.0f);
    SkillDB_Register(s);

    // --- Elementalist-like ("Pyromancer") skills: energy, spells, cast time ---
    s = MakeSkill("Fire Bolt", SKILLTYPE_SPELL, ATTR_FIRE_MAGIC,
                  10, 0, 1.0f, 2.0f, 220.0f, false, TARGET_SINGLE_FOE);
    AddStep(&s, FX_DAMAGE, 5, 3.0f, 0, 0);
    SkillDB_Register(s);

    s = MakeSkill("Cinder Storm", SKILLTYPE_SPELL, ATTR_FIRE_MAGIC,
                  15, 0, 2.0f, 8.0f, 200.0f, false, TARGET_AOE_FOES);
    AddStep(&s, FX_DAMAGE, 10, 2.5f, 0, 0);
    AddStep(&s, FX_APPLY_CONDITION, 0, 0, COND_BURNING, 3.0f);
    SkillDB_Register(s);

    s = MakeSkill("Mind Sear", SKILLTYPE_SPELL, ATTR_ENERGY_STORAGE,
                  5, 0, 0.25f, 1.0f, 999.0f, false, TARGET_SELF);
    AddStep(&s, FX_ENERGY_DELTA, 8, 1.0f, 0, 0);
    SkillDB_Register(s);

    s = MakeSkill("Meteor", SKILLTYPE_SPELL, ATTR_FIRE_MAGIC,
                  25, 0, 3.0f, 15.0f, 200.0f, true, TARGET_AOE_FOES);
    AddStep(&s, FX_DAMAGE, 25, 4.0f, 0, 0);
    SkillDB_Register(s);

    // --- Monster skills ---
    s = MakeSkill("Claw Swipe", SKILLTYPE_ATTACK_SKILL, ATTR_STRENGTH,
                  0, 25, 0.0f, 3.0f, 28.0f, false, TARGET_SINGLE_FOE);
    AddStep(&s, FX_DAMAGE, 10, 0, 0, 0);
    SkillDB_Register(s);

    // --- Interrupt: demonstrates cast time actually meaning something -
    // this only has a target to punish because Fire Bolt/Cinder Storm/
    // Feral Howl all have real cast times a player can watch and react to.
    s = MakeSkill("Distracting Blow", SKILLTYPE_ATTACK_SKILL, ATTR_STRENGTH,
                  0, 25, 0.0f, 8.0f, 28.0f, false, TARGET_SINGLE_FOE);
    AddStep(&s, FX_DAMAGE, 5, 1.0f, 0, 0);
    AddStep(&s, FX_INTERRUPT, 0, 0, 0, 0);
    SkillDB_Register(s);

    // Gives the monster a cast-time skill worth interrupting - without
    // this its only skill (Claw Swipe) is instant and has nothing an
    // interrupt could punish.
    s = MakeSkill("Feral Howl", SKILLTYPE_SPELL, ATTR_STRENGTH,
                  5, 0, 1.5f, 10.0f, 0.0f, false, TARGET_SELF);
    AddStep(&s, FX_HEAL, 30, 0, 0, 0);
    SkillDB_Register(s);
}
