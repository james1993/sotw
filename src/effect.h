#ifndef EFFECT_H
#define EFFECT_H

#include "entity.h"
#include "skill.h"

// Runs every EffectStep in `skill` against the resolved target(s),
// scaling by the caster's rank in skill->attribute. This one function is
// the entire "skill execution engine" - see
// docs/design/raylib-architecture.md #3.
void Effect_Execute(Entity *caster, const Skill *skill, Entity *target);

#endif
