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
    step->selfTarget = false;
}

// Same, but the step lands on the caster instead of the skill's target -
// the "and you gain..." half of a life-steal or energy-steal.
static void AddSelfStep(Skill *s, EffectKind kind, float base, float perRank) {
    AddStep(s, kind, base, perRank, 0, 0);
    if (s->stepCount > 0) s->steps[s->stepCount - 1].selfTarget = true;
}

// Registration order below must match the SkillId enum in skill.h.
void SkillDB_Init(void) {
    g_skillCount = 0;
    Skill s;

    // --- Warrior-like ("Brute") skills: adrenaline, melee, no cast time ---
    // GW1's Gash is the Deep Wound skill: it is what a Warrior lands
    // before a spike, because it drops the target's health ceiling and
    // their healer's numbers at the same moment. Applying Bleeding
    // instead made it just another damage-over-time press.
    s = MakeSkill("Gash", SKILLTYPE_ATTACK_SKILL, ATTR_SWORDSMANSHIP,
                  0, 25, 0.0f, 4.0f, 28.0f, false, TARGET_SINGLE_FOE);
    AddStep(&s, FX_DAMAGE, 8, 1.5f, 0, 0);
    AddStep(&s, FX_APPLY_CONDITION, 0, 0, COND_DEEP_WOUND, 10.0f);
    RegisterAs(SK_GASH, s);

    s = MakeSkill("Rush Strike", SKILLTYPE_ATTACK_SKILL, ATTR_SWORDSMANSHIP,
                  0, 25, 0.0f, 6.0f, 28.0f, false, TARGET_SINGLE_FOE);
    AddStep(&s, FX_DAMAGE, 12, 2.0f, 0, 0);
    RegisterAs(SK_RUSH_STRIKE, s);

    s = MakeSkill("Battle Cry", SKILLTYPE_SHOUT, ATTR_TACTICS,
                  0, 0, 0.0f, 20.0f, 0.0f, false, TARGET_SELF);
    AddStep(&s, FX_ADRENALINE_DELTA, 15, 0, 0, 0);
    RegisterAs(SK_BATTLE_CRY, s);

    s = MakeSkill("Deathblow", SKILLTYPE_ATTACK_SKILL, ATTR_SWORDSMANSHIP,
                  0, 50, 0.0f, 10.0f, 28.0f, true, TARGET_SINGLE_FOE); // 2 strikes
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
    s = MakeSkill("Claw Swipe", SKILLTYPE_ATTACK_SKILL, ATTR_MONSTROUS,
                  0, 25, 0.0f, 3.0f, 28.0f, false, TARGET_SINGLE_FOE);
    AddStep(&s, FX_DAMAGE, 10, 0, 0, 0);
    RegisterAs(SK_CLAW_SWIPE, s);

    // --- Interrupt: demonstrates cast time actually meaning something -
    // this only has a target to punish because Fire Bolt/Cinder Storm/
    // Feral Howl all have real cast times a player can watch and react to.
    s = MakeSkill("Distracting Blow", SKILLTYPE_ATTACK_SKILL, ATTR_TACTICS,
                  0, 25, 0.0f, 8.0f, 28.0f, false, TARGET_SINGLE_FOE);
    AddStep(&s, FX_DAMAGE, 5, 1.0f, 0, 0);
    AddStep(&s, FX_INTERRUPT, 0, 0, 0, 0);
    RegisterAs(SK_DISTRACTING_BLOW, s);

    // Gives the monster a cast-time skill worth interrupting - without
    // this its only skill (Claw Swipe) is instant and has nothing an
    // interrupt could punish.
    s = MakeSkill("Feral Howl", SKILLTYPE_SPELL, ATTR_MONSTROUS,
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

    // --- Hexes (Smiting Prayers) ---
    // Both punish what the target DOES rather than simply ticking
    // damage, which is what keeps them distinct from conditions: a
    // condition wears you down, a hex makes your own actions expensive.
    s = MakeSkill("Shroud of Doubt", SKILLTYPE_SPELL, ATTR_SMITING_PRAYERS,
                  10, 0, 1.5f, 20.0f, 220.0f, false, TARGET_SINGLE_FOE);
    AddStep(&s, FX_APPLY_HEX, 0, 0, HEX_FALTERING, 10.0f);
    RegisterAs(SK_SHROUD_OF_DOUBT, s);

    s = MakeSkill("Price of Faith", SKILLTYPE_SPELL, ATTR_SMITING_PRAYERS,
                  15, 0, 2.0f, 25.0f, 220.0f, false, TARGET_SINGLE_FOE);
    AddStep(&s, FX_APPLY_HEX, 0, 0, HEX_BACKLASH, 12.0f);
    RegisterAs(SK_PRICE_OF_FAITH, s);

    // --- Cleanses ---
    // Deliberately one skill per category. Carrying an answer to
    // conditions does nothing about a hex, which is the counterplay web
    // GW1 builds its bars around (docs/research/gw1-mechanics.md #5).
    s = MakeSkill("Mend Ailment", SKILLTYPE_SPELL, ATTR_HEALING_PRAYERS,
                  5, 0, 0.75f, 4.0f, 220.0f, false, TARGET_SINGLE_ALLY);
    AddStep(&s, FX_REMOVE_CONDITION, 15, 2.0f, 1, 0); // strip 1, heal on success
    RegisterAs(SK_MEND_AILMENT, s);

    s = MakeSkill("Smite Hex", SKILLTYPE_SPELL, ATTR_SMITING_PRAYERS,
                  10, 0, 1.0f, 8.0f, 220.0f, false, TARGET_SINGLE_ALLY);
    AddStep(&s, FX_REMOVE_HEX, 10, 2.0f, 1, 0);
    RegisterAs(SK_SMITE_HEX, s);

    // --- Monster skills that make cleanses worth a bar slot ---
    // Without something applying afflictions to the party, removal is
    // dead weight; these are what put Crippled and the hexes on YOU.
    s = MakeSkill("Rending Claws", SKILLTYPE_ATTACK_SKILL, ATTR_MONSTROUS,
                  0, 25, 0.0f, 6.0f, 28.0f, false, TARGET_SINGLE_FOE); // 1 strike
    AddStep(&s, FX_DAMAGE, 8, 0, 0, 0);
    AddStep(&s, FX_APPLY_CONDITION, 0, 0, COND_BLEEDING, 10.0f);
    AddStep(&s, FX_APPLY_CONDITION, 0, 0, COND_WEAKNESS, 8.0f);
    RegisterAs(SK_RENDING_CLAWS, s);

    // The Devourer's pincers: Crippled is their whole threat, because a
    // halved walk speed is what stops you strolling out of the gully.
    s = MakeSkill("Hobbling Strike", SKILLTYPE_ATTACK_SKILL, ATTR_MONSTROUS,
                  0, 25, 0.0f, 8.0f, 28.0f, false, TARGET_SINGLE_FOE); // 1 strike
    AddStep(&s, FX_DAMAGE, 6, 0, 0, 0);
    AddStep(&s, FX_APPLY_CONDITION, 0, 0, COND_CRIPPLED, 8.0f);
    RegisterAs(SK_HOBBLING_STRIKE, s);

    // --- Ranger ------------------------------------------------------
    // Attack skills at bow range, and priced in ENERGY rather than
    // adrenaline - that's the whole reason Expertise exists as a primary
    // attribute, and a Ranger bar that cost nothing would make it inert.
    s = MakeSkill("Power Shot", SKILLTYPE_ATTACK_SKILL, ATTR_MARKSMANSHIP,
                  10, 0, 0.0f, 5.0f, 240.0f, false, TARGET_SINGLE_FOE);
    AddStep(&s, FX_DAMAGE, 10, 2.5f, 0, 0);
    RegisterAs(SK_POWER_SHOT, s);

    s = MakeSkill("Pin Down", SKILLTYPE_ATTACK_SKILL, ATTR_MARKSMANSHIP,
                  10, 0, 0.0f, 12.0f, 240.0f, false, TARGET_SINGLE_FOE);
    AddStep(&s, FX_DAMAGE, 6, 1.0f, 0, 0);
    AddStep(&s, FX_APPLY_CONDITION, 0, 0, COND_CRIPPLED, 10.0f);
    RegisterAs(SK_PIN_DOWN, s);

    s = MakeSkill("Troll Unguent", SKILLTYPE_SPELL, ATTR_WILDERNESS_SURVIVAL,
                  5, 0, 3.0f, 10.0f, 0.0f, false, TARGET_SELF);
    AddStep(&s, FX_HEAL, 30, 6.0f, 0, 0);
    RegisterAs(SK_TROLL_UNGUENT, s);

    // Poison is one of GW1's four core conditions and nothing in the
    // demake applied it, which left a whole quarter of the condition
    // vocabulary - and the health bar's green tint - unreachable. It
    // belongs on a Ranger: Apply Poison is the skill the profession is
    // built around.
    s = MakeSkill("Apply Poison", SKILLTYPE_SPELL, ATTR_WILDERNESS_SURVIVAL,
                  15, 0, 1.0f, 12.0f, 240.0f, false, TARGET_SINGLE_FOE);
    AddStep(&s, FX_APPLY_CONDITION, 0, 0, COND_POISON, 12.0f);
    RegisterAs(SK_APPLY_POISON, s);

    // --- Necromancer -------------------------------------------------
    // Life stealing rather than raw damage: the two-step "take from
    // them, give to you" shape is what a Blood Magic bar is built on.
    s = MakeSkill("Vampiric Gaze", SKILLTYPE_SPELL, ATTR_BLOOD_MAGIC,
                  10, 0, 1.0f, 5.0f, 220.0f, false, TARGET_SINGLE_FOE);
    AddStep(&s, FX_DAMAGE, 15, 2.5f, 0, 0);
    AddSelfStep(&s, FX_HEAL, 15, 2.5f);
    RegisterAs(SK_VAMPIRIC_GAZE, s);

    s = MakeSkill("Faintheartedness", SKILLTYPE_SPELL, ATTR_CURSES,
                  10, 0, 1.0f, 12.0f, 220.0f, false, TARGET_SINGLE_FOE);
    AddStep(&s, FX_APPLY_HEX, 0, 0, HEX_FALTERING, 12.0f);
    RegisterAs(SK_FAINTHEARTEDNESS, s);

    s = MakeSkill("Barbed Signet", SKILLTYPE_SIGNET, ATTR_CURSES,
                  0, 0, 2.0f, 12.0f, 220.0f, false, TARGET_SINGLE_FOE);
    AddStep(&s, FX_DAMAGE, 12, 1.5f, 0, 0);
    AddStep(&s, FX_APPLY_CONDITION, 0, 0, COND_BLEEDING, 15.0f);
    RegisterAs(SK_BARBED_SIGNET, s);

    // --- Mesmer ------------------------------------------------------
    // Nothing here out-damages an Elementalist. What a Mesmer does is
    // make the FOE'S turn cost them - drain their energy, punish their
    // attacks, break their cast - and do it faster than anyone else can
    // react, which is what Fast Casting pays for.
    s = MakeSkill("Ether Feast", SKILLTYPE_SPELL, ATTR_INSPIRATION_MAGIC,
                  5, 0, 1.0f, 8.0f, 220.0f, false, TARGET_SINGLE_FOE);
    AddStep(&s, FX_ENERGY_DELTA, -3, -0.5f, 0, 0);
    AddSelfStep(&s, FX_HEAL, 20, 4.0f);
    RegisterAs(SK_ETHER_FEAST, s);

    s = MakeSkill("Empathy", SKILLTYPE_SPELL, ATTR_DOMINATION_MAGIC,
                  10, 0, 1.0f, 10.0f, 220.0f, false, TARGET_SINGLE_FOE);
    AddStep(&s, FX_APPLY_HEX, 0, 0, HEX_BACKLASH, 12.0f);
    RegisterAs(SK_EMPATHY, s);

    // The Mesmer's answer to a cast bar. Short activation on purpose:
    // an interrupt you can't get out in time isn't an interrupt.
    s = MakeSkill("Shatter Delusions", SKILLTYPE_SPELL, ATTR_DOMINATION_MAGIC,
                  10, 0, 0.25f, 8.0f, 220.0f, false, TARGET_SINGLE_FOE);
    AddStep(&s, FX_INTERRUPT, 0, 0, 0, 0);
    AddStep(&s, FX_DAMAGE, 12, 2.0f, 0, 0);
    RegisterAs(SK_SHATTER_DELUSIONS, s);

    // --- Block: Warrior defensive stance -----------------------------
    // The engine had no way to block an attack, which meant a whole GW1
    // defensive layer - stances, shields, "block chance" - simply didn't
    // exist. Disciplined Stance is where it starts: for a few seconds you
    // block 75% of attacks and gain +10 armor, and it ends the instant
    // you use an adrenal skill (handled in Combat_ActivateSkill). Real
    // GW1 numbers: 5 energy, 15s recharge, 1..4s duration by Tactics.
    // baseValue carries the block fraction, conditionKind the armor
    // bonus, duration the length (scaled by Tactics rank at run time via
    // perAttributeRank).
    // The stance's duration is the one thing here that scales with the
    // attribute: baseValue is the block fraction, conditionKind the flat
    // armor bonus, duration the base seconds and perAttributeRank the
    // extra seconds per rank of Tactics (the effect handler reads both).
    s = MakeSkill("Disciplined Stance", SKILLTYPE_STANCE, ATTR_TACTICS,
                  5, 0, 0.0f, 15.0f, 0.0f, false, TARGET_SELF);
    AddStep(&s, FX_STANCE_BLOCK, 0.75f, 0.25f, 10, 1.0f);
    RegisterAs(SK_DISCIPLINED_STANCE, s);

    // --- Blind: Elementalist Air Magic -------------------------------
    // Nothing in the game applied Blind, so the 90% miss mechanic that
    // makes a physical attacker suddenly useless had no way to happen.
    // Blinding Flash is its canonical source. Real GW1 numbers: 10
    // energy, 8s recharge; GW1's Blind runs 3..8s with Air Magic, and
    // the demake fixes condition durations (as it does for every other
    // condition) at a representative 5s.
    s = MakeSkill("Blinding Flash", SKILLTYPE_SPELL, ATTR_AIR_MAGIC,
                  10, 0, 0.75f, 8.0f, 220.0f, false, TARGET_SINGLE_FOE);
    AddStep(&s, FX_APPLY_CONDITION, 0, 0, COND_BLIND, 5.0f);
    RegisterAs(SK_BLINDING_FLASH, s);

    // --- Warrior: Axe Mastery ----------------------------------------
    // These are what the Axe line was missing. Adrenal attack skills, no
    // recharge (adrenaline is their gate, GW1-style). Eviscerate is the
    // Axe elite and the other half of the Deep Wound answer to "many
    // Warrior skills inflict it" - Gash is Sword, this is Axe.
    s = MakeSkill("Eviscerate", SKILLTYPE_ATTACK_SKILL, ATTR_AXE_MASTERY,
                  0, 50, 0.0f, 0.0f, 28.0f, true, TARGET_SINGLE_FOE);
    AddStep(&s, FX_DAMAGE, 14, 2.2f, 0, 0);
    AddStep(&s, FX_APPLY_CONDITION, 0, 0, COND_DEEP_WOUND, 10.0f);
    RegisterAs(SK_EVISCERATE, s);

    s = MakeSkill("Executioner's Strike", SKILLTYPE_ATTACK_SKILL, ATTR_AXE_MASTERY,
                  0, 50, 0.0f, 0.0f, 28.0f, false, TARGET_SINGLE_FOE);
    AddStep(&s, FX_DAMAGE, 14, 2.0f, 0, 0);
    RegisterAs(SK_EXECUTIONERS_STRIKE, s);

    // Hits every foe adjacent to the swing - the Axe line's answer to a
    // mob, and the reason a Warrior can hold a chokepoint.
    s = MakeSkill("Cyclone Axe", SKILLTYPE_ATTACK_SKILL, ATTR_AXE_MASTERY,
                  5, 0, 0.0f, 4.0f, 28.0f, false, TARGET_AOE_FOES);
    s.aoeRadius = 44.0f; // "adjacent", not a Meteor-sized blast
    AddStep(&s, FX_DAMAGE, 5, 0.7f, 0, 0);
    RegisterAs(SK_CYCLONE_AXE, s);

    s = MakeSkill("Penetrating Blow", SKILLTYPE_ATTACK_SKILL, ATTR_AXE_MASTERY,
                  0, 25, 0.0f, 0.0f, 28.0f, false, TARGET_SINGLE_FOE);
    s.armorPen = 0.20f; // its whole point: 20% armor penetration, no Strength needed
    AddStep(&s, FX_DAMAGE, 8, 1.4f, 0, 0);
    RegisterAs(SK_PENETRATING_BLOW, s);

    // --- Warrior: Hammer Mastery -------------------------------------
    // The Hammer line trades the Axe's raw numbers for control: knockdown
    // is what a hammer does that nothing else can.
    s = MakeSkill("Hammer Bash", SKILLTYPE_ATTACK_SKILL, ATTR_HAMMER_MASTERY,
                  0, 25, 0.0f, 0.0f, 28.0f, false, TARGET_SINGLE_FOE);
    AddStep(&s, FX_DAMAGE, 4, 0.5f, 0, 0);
    AddStep(&s, FX_KNOCKDOWN, 0, 0, 0, 2.0f);
    RegisterAs(SK_HAMMER_BASH, s);

    s = MakeSkill("Mighty Blow", SKILLTYPE_ATTACK_SKILL, ATTR_HAMMER_MASTERY,
                  0, 50, 0.0f, 0.0f, 28.0f, false, TARGET_SINGLE_FOE);
    AddStep(&s, FX_DAMAGE, 14, 2.0f, 0, 0);
    RegisterAs(SK_MIGHTY_BLOW, s);

    // GW1's Crushing Blow inflicts Deep Wound only on a knocked-down
    // foe; the demake applies it outright, the same simplification Gash
    // already uses (it dropped Gash's "if Bleeding" rider).
    s = MakeSkill("Crushing Blow", SKILLTYPE_ATTACK_SKILL, ATTR_HAMMER_MASTERY,
                  5, 0, 0.0f, 10.0f, 28.0f, false, TARGET_SINGLE_FOE);
    AddStep(&s, FX_DAMAGE, 10, 1.2f, 0, 0);
    AddStep(&s, FX_APPLY_CONDITION, 0, 0, COND_DEEP_WOUND, 10.0f);
    RegisterAs(SK_CRUSHING_BLOW, s);

    // --- Monk: Protection Prayers ------------------------------------
    // The Protection line was empty. Guardian is a block ENCHANTMENT (not
    // a stance): it puts a block chance on an ally and, being an
    // enchantment, can be torn off by enchantment removal - the
    // counterplay that makes the whole category matter.
    s = MakeSkill("Guardian", SKILLTYPE_SPELL, ATTR_PROTECTION_PRAYERS,
                  5, 0, 1.0f, 6.0f, 220.0f, false, TARGET_SINGLE_ALLY);
    AddStep(&s, FX_APPLY_ENCHANTMENT, 0.5f, 0.0f, ENCH_BLOCK, 7.0f);
    RegisterAs(SK_GUARDIAN, s);

    // --- Necromancer: Death Magic ------------------------------------
    // Death Magic proper is minions, which the demake has no system for;
    // Deathly Swarm is its direct-damage face and needs none.
    s = MakeSkill("Deathly Swarm", SKILLTYPE_SPELL, ATTR_DEATH_MAGIC,
                  15, 0, 1.0f, 5.0f, 220.0f, false, TARGET_SINGLE_FOE);
    AddStep(&s, FX_DAMAGE, 10, 2.5f, 0, 0);
    RegisterAs(SK_DEATHLY_SWARM, s);

    // --- Mesmer: Illusion Magic --------------------------------------
    // A pure-degeneration hex - distinct from Faltering (which slows
    // attacks) and Backlash (which punishes them). This just wears the
    // target down, which is Illusion's whole idea.
    s = MakeSkill("Conjure Phantasm", SKILLTYPE_SPELL, ATTR_ILLUSION_MAGIC,
                  10, 0, 1.0f, 5.0f, 220.0f, false, TARGET_SINGLE_FOE);
    AddStep(&s, FX_APPLY_HEX, 0, 0, HEX_PHANTASM, 8.0f);
    RegisterAs(SK_CONJURE_PHANTASM, s);

    // --- Elementalist: Water Magic -----------------------------------
    // Water's identity is the snare: damage plus Crippled, so a kiting
    // caster can keep a melee foe off them.
    s = MakeSkill("Shard Storm", SKILLTYPE_SPELL, ATTR_WATER_MAGIC,
                  10, 0, 1.0f, 5.0f, 200.0f, false, TARGET_SINGLE_FOE);
    AddStep(&s, FX_DAMAGE, 8, 1.8f, 0, 0);
    AddStep(&s, FX_APPLY_CONDITION, 0, 0, COND_CRIPPLED, 5.0f);
    RegisterAs(SK_SHARD_STORM, s);

    // --- Elementalist: Earth Magic -----------------------------------
    // Earth's is the knockdown: less damage than Fire, but it buys time.
    s = MakeSkill("Stoning", SKILLTYPE_SPELL, ATTR_EARTH_MAGIC,
                  5, 0, 1.0f, 5.0f, 200.0f, false, TARGET_SINGLE_FOE);
    AddStep(&s, FX_DAMAGE, 7, 1.5f, 0, 0);
    AddStep(&s, FX_KNOCKDOWN, 0, 0, 0, 2.0f);
    RegisterAs(SK_STONING, s);

    // --- Ranger: Beast Mastery ---------------------------------------
    // Beast Mastery is really the pet line, which the demake has no
    // system for; this stands in as a melee strike that feeds the
    // Ranger energy, so the attribute is at least represented and
    // buyable. (GW1's Ferocious Strike is elite; kept non-elite here so
    // it isn't stranded behind a boss the demake never gives it to.)
    s = MakeSkill("Ferocious Strike", SKILLTYPE_ATTACK_SKILL, ATTR_BEAST_MASTERY,
                  0, 25, 0.0f, 0.0f, 28.0f, false, TARGET_SINGLE_FOE);
    AddStep(&s, FX_DAMAGE, 10, 2.0f, 0, 0);
    AddSelfStep(&s, FX_ENERGY_DELTA, 5, 0.0f);
    RegisterAs(SK_FEROCIOUS_STRIKE, s);

    // --- Enchantments ------------------------------------------------
    // The third affliction category. Each one exercises a different one
    // of the four enchantment mechanics, and Shatter Enchantment answers
    // all of them - which is the point of making them a removable class
    // rather than four ad-hoc buffs.

    // Regeneration: the magnitude IS the pip count, scaled by Healing
    // Prayers. Long duration stands in for GW1's maintained upkeep, which
    // the demake has no energy-drain system for.
    s = MakeSkill("Mending", SKILLTYPE_SPELL, ATTR_HEALING_PRAYERS,
                  10, 0, 2.0f, 2.0f, 220.0f, false, TARGET_SINGLE_ALLY);
    AddStep(&s, FX_APPLY_ENCHANTMENT, 2.0f, 0.2f, ENCH_REGEN, 30.0f);
    RegisterAs(SK_MENDING, s);

    s = MakeSkill("Healing Breeze", SKILLTYPE_SPELL, ATTR_HEALING_PRAYERS,
                  10, 0, 1.0f, 5.0f, 220.0f, false, TARGET_SINGLE_ALLY);
    AddStep(&s, FX_APPLY_ENCHANTMENT, 4.0f, 0.4f, ENCH_REGEN, 15.0f);
    RegisterAs(SK_HEALING_BREEZE, s);

    // Damage cap: magnitude is the fraction of max health a single hit is
    // clipped to. GW1's Protective Spirit is a flat 10%.
    s = MakeSkill("Protective Spirit", SKILLTYPE_SPELL, ATTR_PROTECTION_PRAYERS,
                  10, 0, 0.25f, 5.0f, 220.0f, false, TARGET_SINGLE_ALLY);
    AddStep(&s, FX_APPLY_ENCHANTMENT, 0.10f, 0.0f, ENCH_DAMAGE_CAP, 8.0f);
    RegisterAs(SK_PROTECTIVE_SPIRIT, s);

    // Armor: magnitude is the flat AL bonus. Self-cast, like the real
    // Armor of Earth (its movement penalty is not modelled).
    s = MakeSkill("Armor of Earth", SKILLTYPE_SPELL, ATTR_EARTH_MAGIC,
                  10, 0, 0.75f, 15.0f, 0.0f, false, TARGET_SELF);
    AddStep(&s, FX_APPLY_ENCHANTMENT, 24.0f, 3.0f, ENCH_ARMOR, 30.0f);
    RegisterAs(SK_ARMOR_OF_EARTH, s);

    // The counter: strips one enchantment off a foe and, only when it
    // took something, deals the removal damage (baseValue).
    s = MakeSkill("Shatter Enchantment", SKILLTYPE_SPELL, ATTR_DOMINATION_MAGIC,
                  10, 0, 1.0f, 20.0f, 220.0f, false, TARGET_SINGLE_FOE);
    AddStep(&s, FX_REMOVE_ENCHANTMENT, 14.0f, 6.0f, 1, 0.0f);
    RegisterAs(SK_SHATTER_ENCHANTMENT, s);
}
