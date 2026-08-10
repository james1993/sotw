// Unit test for the title-track logic in titles.c.
//
// The Survivor title is the interesting case: it is earned by reaching the
// level cap, but a single death loses it for good, and it must fall off the
// earnable list the moment the death flag is set - including when that flag
// is restored from a save. Legendary Defender of Ascalon shares the level
// gate but ignores deaths, so testing them together pins the difference.
//
// titles.c only reaches outside itself through Entity_Get (in SetDisplayed),
// which is stubbed here, so this links against titles.c alone.
//
//   cc -I src tests/test_titles.c src/titles.c -o /tmp/t && /tmp/t

#include "titles.h"
#include "entity.h"
#include <stdio.h>

// --- Stub ------------------------------------------------------------
// SetDisplayed validates against the live player; the test drives the
// player's level through this one slot.
static Entity g_player;
Entity *Entity_Get(int index) { (void)index; return &g_player; }

// --- Harness ---------------------------------------------------------
static int g_failures = 0;
#define CHECK(cond, msg, ...) do { \
    if (!(cond)) { printf("  FAIL: " msg "\n", ##__VA_ARGS__); g_failures++; } \
} while (0)

static Entity AtLevel(int lvl) { Entity e = (Entity){0}; e.level = lvl; return e; }

int main(void) {
    printf("titles\n");

    Titles_Reset();

    // Below the cap neither title is earned.
    Entity l19 = AtLevel(19);
    CHECK(!Titles_IsEarned(TITLE_DEFENDER_OF_ASCALON, &l19), "LDoA not earned at 19");
    CHECK(!Titles_IsEarned(TITLE_SURVIVOR, &l19), "Survivor not earned at 19");

    // At the cap, with a clean run, both are earned.
    Entity l20 = AtLevel(20);
    CHECK(Titles_IsEarned(TITLE_DEFENDER_OF_ASCALON, &l20), "LDoA earned at 20");
    CHECK(Titles_IsEarned(TITLE_SURVIVOR, &l20), "Survivor earned at 20 with no death");

    // A death loses Survivor for good, but never touches LDoA.
    Titles_NotifyDeath();
    CHECK(Titles_EverDied(), "death is recorded");
    CHECK(Titles_IsEarned(TITLE_DEFENDER_OF_ASCALON, &l20), "LDoA survives a death");
    CHECK(!Titles_IsEarned(TITLE_SURVIVOR, &l20), "Survivor is lost after a death");
    CHECK(Titles_Progress(TITLE_SURVIVOR, &l20) == 0.0f, "dead Survivor reads as 0 progress");

    // Reveal rule: Survivor shows while the run is alive even before 12,
    // then falls off display once the run is dead and still under 12.
    Titles_Reset();
    Entity l5 = AtLevel(5);
    CHECK(Titles_IsRevealed(TITLE_SURVIVOR, &l5), "Survivor shown early while alive");
    CHECK(!Titles_IsRevealed(TITLE_DEFENDER_OF_ASCALON, &l5), "LDoA hidden before 12");
    Titles_NotifyDeath();
    CHECK(!Titles_IsRevealed(TITLE_SURVIVOR, &l5), "dead low-level Survivor hides");

    // Wearing a title you haven't earned is refused; wearing an earned one
    // sticks. g_player is what SetDisplayed validates against.
    Titles_Reset();
    g_player = AtLevel(20);
    Titles_SetDisplayed(TITLE_SURVIVOR);
    CHECK(Titles_Displayed() == TITLE_SURVIVOR, "earned Survivor can be worn");
    Titles_NotifyDeath();
    CHECK(Titles_DisplayedText(&g_player) == NULL, "a worn Survivor blanks out after death");

    Titles_Reset();
    g_player = AtLevel(10);
    Titles_SetDisplayed(TITLE_DEFENDER_OF_ASCALON);
    CHECK(Titles_Displayed() == -1, "unearned LDoA is refused");

    // Reset clears the death flag, so a fresh character starts clean.
    Titles_NotifyDeath();
    Titles_Reset();
    CHECK(!Titles_EverDied(), "reset clears the death flag");

    printf(g_failures ? "  %d FAILED\n" : "  all passed\n", g_failures);
    return g_failures != 0;
}
