#ifndef FX_H
#define FX_H

#include "raylib.h"
#include "attributes.h"

// World-space combat effects: short-lived, purely visual, pooled.
// Slash arcs on melee, impact bursts colored by the skill's school,
// rising heal sparkles, expanding AoE rings, knockdown stars. Spawned
// from combat/effect/projectile code, updated once per frame, drawn
// inside the world camera, cleared on zone loads.

void Fx_Slash(Vector2 pos, float angle);            // melee swing connecting
void Fx_Burst(Vector2 pos, Color color);            // damage impact
void Fx_Heal(Vector2 pos);                          // green rising sparkles
void Fx_Ring(Vector2 pos, float radius, Color color); // AoE footprint
void Fx_Stars(Vector2 pos);                         // knockdown daze

// School color shared by FX and cast glows.
Color Fx_AttrColor(AttributeKind a);

void Fx_Update(float dt);
void Fx_Draw(void);
void Fx_Clear(void);

#endif
