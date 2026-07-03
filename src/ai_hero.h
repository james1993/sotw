#ifndef AI_HERO_H
#define AI_HERO_H

// Drives both hero and monster entities through the same
// Combat_ActivateSkill() path the player's input uses - see
// docs/design/raylib-architecture.md #6.
void AI_Update(float dt);

#endif
