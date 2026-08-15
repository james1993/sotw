#ifndef TRIAL_H
#define TRIAL_H

#include <stdbool.h>

// --- The Trial: a wave arena -----------------------------------------
//
// The campaign hands a new character ONE combat skill and trickles the
// rest out over hours of errands. That is faithful to pre-Searing, and
// it is also why the game has nothing to play with: Guild Wars' whole
// idea is an eight-skill bar under pressure, and the campaign withholds
// it. The Trial is the inversion - a full bar in the first ten seconds,
// and foes built to punish you for ignoring any of it.
//
// Waves escalate forever. Enemies come with ROLES, so every wave asks
// the GW1 question: who do I kill first, and what do I interrupt?

void Trial_Start(void);   // reset to wave 1; called on entering the arena
void Trial_Stop(void);    // leaving the arena; the campaign is unaffected
void Trial_Update(float dt);
void Trial_Draw(int screenWidth, int screenHeight);

bool Trial_IsActive(void);
int  Trial_Wave(void);
int  Trial_BestWave(void);

// True once the player has fallen and the run summary is showing.
bool Trial_RunOver(void);

#endif
