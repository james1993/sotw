#include "skill.h"
#include "entity.h"
#include "raylib.h"
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
    s.aoeRadius = 64.0f; // sensible default; AoE skills may override
    s.stepCount = 0;
    return s;
}

// Registers a skill and verifies it landed on the SkillId slot the rest
// of the code will reference it by.
static void RegisterAs(SkillId id, Skill s) {
    int got = SkillDB_Register(s);
    if (got != (int)id) {
        TraceLog(LOG_WARNING, "SKILL: '%s' registered at %d, expected SkillId %d - bars will be wrong",
                 s.name, got, (int)id);
    }
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

// Registration order below must match the SkillId enum in skill.h.
void SkillDB_Init(void) {
    g_skillCount = 0;
    Skill s;

    // --- Warrior-like ("Brute") skills: adrenaline, melee, no cast time ---
    s = MakeSkill("Gash", SKILLTYPE_ATTACK_SKILL, ATTR_STRENGTH,
                  0, 25, 0.0f, 4.0f, 28.0f, false, TARGET_SINGLE_FOE);
    AddStep(&s, FX_DAMAGE, 8, 1.5f, 0, 0);
    AddStep(&s, FX_APPLY_CONDITION, 0, 0, COND_BLEEDING, 8.0f);
    RegisterAs(SK_GASH, s);

    s = MakeSkill("Rush Strike", SKILLTYPE_ATTACK_SKILL, ATTR_STRENGTH,
                  0, 25, 0.0f, 6.0f, 28.0f, false, TARGET_SINGLE_FOE);
    AddStep(&s, FX_DAMAGE, 12, 2.0f, 0, 0);
    RegisterAs(SK_RUSH_STRIKE, s);

    s = MakeSkill("Battle Cry", SKILLTYPE_SHOUT, ATTR_TACTICS,
                  0, 0, 0.0f, 20.0f, 0.0f, false, TARGET_SELF);
    AddStep(&s, FX_ADRENALINE_DELTA, 15, 0, 0, 0);
    RegisterAs(SK_BATTLE_CRY, s);

    s = MakeSkill("Deathblow", SKILLTYPE_ATTACK_SKILL, ATTR_STRENGTH,
                  0, 40, 0.0f, 10.0f, 28.0f, true, TARGET_SINGLE_FOE);
    AddStep(&s, FX_DAMAGE, 20, 3.0f, 0, 0);
    AddStep(&s, FX_KNOCKDOWN, 0, 0, 0, 2.0f);
    RegisterAs(SK_DEATHBLOW, s);

    // --- Elementalist-like ("Pyromancer") skills: energy, spells, cast time ---
    s = MakeSkill("Fire Bolt", SKILLTYPE_SPELL, ATTR_FIRE_MAGIC,
                  10, 0, 1.0f, 2.0f, 220.0f, false, TARGET_SINGLE_FOE);
    AddStep(&s, FX_DAMAGE, 5, 3.0f, 0, 0);
    RegisterAs(SK_FIRE_BOLT, s);

    s = MakeSkill("Cinder Storm", SKILLTYPE_SPELL, ATTR_FIRE_MAGIC,
                  15, 0, 2.0f, 8.0f, 200.0f, false, TARGET_AOE_FOES);
    AddStep(&s, FX_DAMAGE, 10, 2.5f, 0, 0);
    AddStep(&s, FX_APPLY_CONDITION, 0, 0, COND_BURNING, 3.0f);
    RegisterAs(SK_CINDER_STORM, s);

    s = MakeSkill("Mind Sear", SKILLTYPE_SPELL, ATTR_ENERGY_STORAGE,
                  5, 0, 0.25f, 1.0f, 999.0f, false, TARGET_SELF);
    AddStep(&s, FX_ENERGY_DELTA, 8, 1.0f, 0, 0);
    RegisterAs(SK_MIND_SEAR, s);

    s = MakeSkill("Meteor", SKILLTYPE_SPELL, ATTR_FIRE_MAGIC,
                  25, 0, 3.0f, 15.0f, 200.0f, true, TARGET_AOE_FOES);
    AddStep(&s, FX_DAMAGE, 25, 4.0f, 0, 0);
    RegisterAs(SK_METEOR, s);

    // --- Monster skills ---
    s = MakeSkill("Claw Swipe", SKILLTYPE_ATTACK_SKILL, ATTR_STRENGTH,
                  0, 25, 0.0f, 3.0f, 28.0f, false, TARGET_SINGLE_FOE);
    AddStep(&s, FX_DAMAGE, 10, 0, 0, 0);
    RegisterAs(SK_CLAW_SWIPE, s);

    // --- Interrupt: demonstrates cast time actually meaning something -
    // this only has a target to punish because Fire Bolt/Cinder Storm/
    // Feral Howl all have real cast times a player can watch and react to.
    s = MakeSkill("Distracting Blow", SKILLTYPE_ATTACK_SKILL, ATTR_STRENGTH,
                  0, 25, 0.0f, 8.0f, 28.0f, false, TARGET_SINGLE_FOE);
    AddStep(&s, FX_DAMAGE, 5, 1.0f, 0, 0);
    AddStep(&s, FX_INTERRUPT, 0, 0, 0, 0);
    RegisterAs(SK_DISTRACTING_BLOW, s);

    // Gives the monster a cast-time skill worth interrupting - without
    // this its only skill (Claw Swipe) is instant and has nothing an
    // interrupt could punish.
    s = MakeSkill("Feral Howl", SKILLTYPE_SPELL, ATTR_STRENGTH,
                  5, 0, 1.5f, 10.0f, 0.0f, false, TARGET_SELF);
    AddStep(&s, FX_HEAL, 30, 0, 0, 0);
    RegisterAs(SK_FERAL_HOWL, s);

    // --- Monk skills: energy spells, Divine Favor bonus healing applies
    // to FX_HEAL (see effect.c), no adrenaline anywhere on this bar ---
    // Ally-targeted like the real Orison: heals your selected party
    // member, or yourself when no valid ally is targeted (see
    // Combat_ActivateSkill's self-fallback).
    s = MakeSkill("Orison of Healing", SKILLTYPE_SPELL, ATTR_HEALING_PRAYERS,
                  5, 0, 1.0f, 2.0f, 220.0f, false, TARGET_SINGLE_ALLY);
    AddStep(&s, FX_HEAL, 20, 3.0f, 0, 0);
    RegisterAs(SK_ORISON_OF_HEALING, s);

    s = MakeSkill("Banish", SKILLTYPE_SPELL, ATTR_SMITING_PRAYERS,
                  5, 0, 1.0f, 4.0f, 220.0f, false, TARGET_SINGLE_FOE);
    AddStep(&s, FX_DAMAGE, 15, 2.5f, 0, 0);
    RegisterAs(SK_BANISH, s);

    s = MakeSkill("Smite", SKILLTYPE_SPELL, ATTR_SMITING_PRAYERS,
                  10, 0, 1.0f, 6.0f, 220.0f, false, TARGET_SINGLE_FOE);
    AddStep(&s, FX_DAMAGE, 25, 3.0f, 0, 0);
    RegisterAs(SK_SMITE, s);

    // Signet: free, but pays for it in cast time and a long recharge -
    // the type's whole identity in GW1.
    s = MakeSkill("Bane Signet", SKILLTYPE_SIGNET, ATTR_SMITING_PRAYERS,
                  0, 0, 2.0f, 15.0f, 220.0f, false, TARGET_SINGLE_FOE);
    AddStep(&s, FX_DAMAGE, 20, 2.0f, 0, 0);
    AddStep(&s, FX_KNOCKDOWN, 0, 0, 0, 2.0f);
    RegisterAs(SK_BANE_SIGNET, s);

    // The Monk elite. Elites can't be bought at any price - the only way
    // onto your bar is capturing one off a boss that uses it, which is
    // what makes hunting bosses worth the risk (GW1's Signet of Capture,
    // condensed into "kill the boss, learn the skill").
    s = MakeSkill("Healing Light", SKILLTYPE_SPELL, ATTR_HEALING_PRAYERS,
                  10, 0, 1.0f, 4.0f, 220.0f, true, TARGET_SINGLE_ALLY);
    AddStep(&s, FX_HEAL, 45, 5.0f, 0, 0);
    AddStep(&s, FX_ENERGY_DELTA, 3, 0.5f, 0, 0);
    RegisterAs(SK_HEALING_LIGHT, s);
}
