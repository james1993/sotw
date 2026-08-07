// Unit test for the focus index arithmetic in ui_focus.c.
//
// The input BINDING (which pad button) cannot be tested without hardware,
// but the index arithmetic can, and that's where the bugs actually live:
// wrap-around, clamping when a list shrinks, and the rule that an item
// which skips registration shifts every item after it.
//
// raylib and audio are stubbed, so this links against neither.
//
//   cc -I src tests/test_ui_focus.c src/ui_focus.c -o /tmp/t && /tmp/t

#include "ui_focus.h"
#include "audio.h"
#include <stdio.h>
#include <string.h>

// --- Stubs -----------------------------------------------------------
static int g_navQueued = 0;   // +1 down, -1 up, applied on next NavStep
static int g_confirm = 0;
int g_moveSounds = 0;

bool IsKeyPressed(int key) {
    if (key == KEY_DOWN && g_navQueued > 0) { g_navQueued--; return true; }
    if (key == KEY_UP && g_navQueued < 0) { g_navQueued++; return true; }
    if ((key == KEY_ENTER || key == KEY_KP_ENTER) && g_confirm) { g_confirm = 0; return true; }
    return false;
}
bool IsGamepadAvailable(int g) { (void)g; return false; }
bool IsGamepadButtonPressed(int g, int b) { (void)g; (void)b; return false; }
float GetGamepadAxisMovement(int g, int a) { (void)g; (void)a; return 0.0f; }
void Audio_Play(SoundId id) { if (id == SFX_UI_MOVE) g_moveSounds++; }
void Audio_PlayAt(SoundId id, float dx) { (void)id; (void)dx; }
void Audio_PlaySkill(const Skill *s) { (void)s; }

// --- Harness ---------------------------------------------------------
static int g_failures = 0;
#define CHECK(cond, msg, ...) do { \
    if (!(cond)) { printf("  FAIL: " msg "\n", ##__VA_ARGS__); g_failures++; } \
} while (0)

// Runs one frame with `count` items and returns which index was focused.
static int Frame(int count) {
    UIFocus_Begin();
    int focused = -1;
    for (int i = 0; i < count; i++) {
        if (UIFocus_Item()) focused = i;
    }
    UIFocus_End();
    return focused;
}

int main(void) {
    printf("ui_focus\n");

    // Focus starts at the first item.
    UIFocus_Clear();
    Frame(3); // first frame establishes the count
    int f = Frame(3);
    CHECK(f == 0, "starts on item 0, got %d", f);

    // Down steps forward.
    UIFocus_Clear(); Frame(3);
    g_navQueued = 1;
    CHECK(Frame(3) == 1, "down -> 1");
    g_navQueued = 1;
    CHECK(Frame(3) == 2, "down -> 2");

    // ...and wraps to the top rather than sticking at the end.
    g_navQueued = 1;
    CHECK(Frame(3) == 0, "down past the end wraps to 0");

    // Up wraps backwards, which is the case a naive modulo gets wrong
    // because C's % keeps the sign of the dividend.
    UIFocus_Clear(); Frame(3);
    g_navQueued = -1;
    CHECK(Frame(3) == 2, "up from 0 wraps to the last item");

    // A list that shrinks must not leave focus past the end - buying the
    // last skill a trainer had is exactly this.
    UIFocus_Clear(); Frame(5);
    g_navQueued = 1; Frame(5);
    g_navQueued = 1; Frame(5);
    g_navQueued = 1; Frame(5);
    g_navQueued = 1;
    f = Frame(5);
    CHECK(f == 4, "on the last of five, got %d", f);
    Frame(2);            // the frame the list shrinks in: clamp happens at End
    f = Frame(2);
    CHECK(f == 1, "after a shrink to 2 the highlight sits on 1, got %d", f);

    // Navigation with nothing on screen must not move or crash.
    UIFocus_Clear();
    g_navQueued = 1;
    f = Frame(0);
    CHECK(f == -1, "empty list focuses nothing, got %d", f);
    f = Frame(1);
    CHECK(f == 0, "and the next list starts at 0, got %d", f);

    // A step that doesn't change the index makes no sound: a single-item
    // dialog shouldn't tick every time the stick is nudged.
    UIFocus_Clear(); Frame(1);
    g_moveSounds = 0;
    g_navQueued = 1; Frame(1);
    CHECK(g_moveSounds == 0, "single-item list is silent on nav, got %d ticks", g_moveSounds);

    // ...but a real move does.
    UIFocus_Clear(); Frame(3);
    g_moveSounds = 0;
    g_navQueued = 1; Frame(3);
    CHECK(g_moveSounds == 1, "a real move ticks once, got %d", g_moveSounds);

    // Clear puts it back to the top, which is what opening a new
    // conversation relies on.
    UIFocus_Clear(); Frame(4);
    g_navQueued = 1; Frame(4);
    UIFocus_Clear();
    CHECK(Frame(4) == 0, "clear returns focus to item 0");

    printf(g_failures ? "  %d FAILED\n" : "  all passed\n", g_failures);
    return g_failures != 0;
}
