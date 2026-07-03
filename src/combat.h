#ifndef COMBAT_H
#define COMBAT_H

#include <stdbool.h>
#include "entity.h"

// Validates cost/recharge/range/target legality and either starts a cast
// (castTime > 0) or resolves instantly. Player input, hero AI, and
// monster AI all funnel through this one function - see
// docs/design/raylib-architecture.md #4.
bool Combat_ActivateSkill(int casterIndex, int slot, int targetIndex);

void Combat_UpdateEntity(Entity *e, float dt);
void Combat_TickTimers(float dt);

#endif
