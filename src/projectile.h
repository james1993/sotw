#ifndef PROJECTILE_H
#define PROJECTILE_H

#include "raylib.h"
#include "entity.h"
#include <stdbool.h>

#define MAX_PROJECTILES 64

// Ranged auto-attacks fire one of these instead of dealing instant
// damage. The landing point is locked to the target's position at fire
// time, so a target that moves far enough before impact dodges the shot
// outright - the 2D demake making GW1's projectile travel time visible
// and skill-expressive (docs/design/demake-design.md #2).
typedef struct {
    bool active;
    Vector2 pos;
    Vector2 landingPoint;  // target's position when the shot left
    EntityRef targetRef;   // generational: a stale target means the shot
    EntityRef shooterRef;  // fizzles instead of hitting a slot-reuse victim
    int damage;
    float speed;
    Color color;
} Projectile;

extern Projectile g_projectiles[MAX_PROJECTILES];

void Projectile_Spawn(int shooterIndex, int targetIndex, int damage);
void Projectile_Update(float dt);
void Projectile_ClearAll(void); // rezoning discards shots in flight
void Projectile_Draw(void);    // world-space, call inside the 2D camera

#endif
