#include "input.h"
#include "entity.h"
#include "combat.h"
#include "ui_panels.h"
#include "ui_hit.h"
#include "ui_cursor.h"
#include "ui_map.h"
#include "world.h"
#include <math.h>
#include <stdbool.h>

#define PLAYER_INDEX 0
#define MAX_CLICK_MOVE_DISTANCE 2000.0f
#define GAMEPAD_ID 0
#define STICK_DEADZONE 0.25f
#define TRIGGER_AXIS_THRESHOLD 0.3f
// How close you must be for a talk action (click or the X/Square
// button) to open an NPC's dialog rather than walk toward them.
#define NPC_TALK_DISTANCE 90.0f

// True while the player's most recent input came from the gamepad. In
// gamepad mode the game auto-targets the nearest foe in attack range
// every frame (so auto-attack "just happens" when you walk up to
// something, per the stick-driven control scheme) - but that same
// auto-targeting would fight mouse click-targeting, so it only runs
// while the pad, not the mouse, was touched last.
static bool g_gamepadMode = false;

// Set when a target was explicitly chosen on the D-pad (GW1's official
// gamepad scheme: up/down walks the party list, left/right cycles
// enemies). While a manual selection is live, the proximity auto-target
// leaves it alone.
static bool g_manualGamepadTarget = false;

static bool g_userAdjustedZoom = false;

// The NPC the interact button would talk to right now: the nearest one
// inside NPC_TALK_DISTANCE, or -1. Resolved once per frame and shared
// with the renderer so the highlighted NPC and the one you actually
// talk to can never disagree - that mismatch was the whole reason
// "which NPC am I about to talk to?" was a question.
static int g_interactNpc = -1;
// The entity under the mouse this frame, or -1. Drives hover feedback.
static int g_hoverEntity = -1;

bool Input_UserAdjustedZoom(void) { return g_userAdjustedZoom; }
int Input_InteractNpcIndex(void) { return g_interactNpc; }
int Input_HoverEntityIndex(void) { return g_hoverEntity; }

// Nearest living NPC within talk range of the player, or -1.
static int FindInteractableNpc(const Entity *player) {
    int best = -1;
    float bestDist = NPC_TALK_DISTANCE;
    for (int i = 0; i < g_entityCount; i++) {
        Entity *e = &g_entities[i];
        if (!e->alive || e->kind != ENT_NPC) continue;
        float dx = e->pos.x - player->pos.x, dy = e->pos.y - player->pos.y;
        float d = sqrtf(dx * dx + dy * dy);
        if (d <= bestDist) { bestDist = d; best = i; }
    }
    return best;
}

static bool TriggerDown(int axisId, int buttonId) {
    if (IsGamepadButtonDown(GAMEPAD_ID, buttonId)) return true;
    // Analog triggers rest at -1 and reach +1 fully pressed on most
    // backends; anything meaningfully past the midpoint counts as held.
    return GetGamepadAxisMovement(GAMEPAD_ID, axisId) > TRIGGER_AXIS_THRESHOLD;
}

// D-pad up/down: walk the party list (player + heroes, entity order).
static void CyclePartyTarget(Entity *player, int direction) {
    int members[MAX_ENTITIES];
    int count = 0, currentPos = -1;
    int currentTarget = Entity_RefIndex(player->targetRef);
    for (int i = 0; i < g_entityCount; i++) {
        Entity *e = &g_entities[i];
        if (e->team != 0 || !e->alive) continue;
        if (e->kind != ENT_PLAYER && e->kind != ENT_HERO) continue;
        if (i == currentTarget) currentPos = count;
        members[count++] = i;
    }
    if (count == 0) return;
    int pos = (currentPos < 0) ? (direction > 0 ? 0 : count - 1)
                               : (currentPos + direction + count) % count;
    player->targetRef = Entity_RefOf(members[pos]);
    g_manualGamepadTarget = true;
}

// True when the entity is inside the current view - GW1's target
// cycling only walks foes you can actually see, and so does ours.
static bool OnScreen(const Entity *e, const Camera2D *camera) {
    Vector2 p = GetWorldToScreen2D(e->pos, *camera);
    float pad = e->radius * camera->zoom + 8.0f;
    return p.x >= -pad && p.x <= (float)GetScreenWidth() + pad &&
           p.y >= -pad && p.y <= (float)GetScreenHeight() + pad;
}

// L1/R1 (and D-pad left/right, and C/Tab): cycle living, VISIBLE foes,
// nearest first. HOSTILE MONSTERS ONLY - friendly NPCs are never part of
// the cycle. You reach them by walking up and pressing interact, which
// keeps "cycle" meaning one thing: pick something to fight.
static void CycleFoeTarget(Entity *player, int direction, const Camera2D *camera) {
    int foes[MAX_ENTITIES];
    float dists[MAX_ENTITIES];
    int count = 0;
    for (int i = 0; i < g_entityCount; i++) {
        Entity *e = &g_entities[i];
        if (!e->alive || e->kind != ENT_MONSTER || e->team == player->team) continue;
        if (!OnScreen(e, camera)) continue;
        float dx = e->pos.x - player->pos.x, dy = e->pos.y - player->pos.y;
        foes[count] = i;
        dists[count] = dx * dx + dy * dy;
        count++;
    }
    if (count == 0) return;
    // insertion sort by distance - the foe list is tiny
    for (int i = 1; i < count; i++) {
        int fi = foes[i]; float di = dists[i]; int j = i - 1;
        while (j >= 0 && dists[j] > di) { foes[j + 1] = foes[j]; dists[j + 1] = dists[j]; j--; }
        foes[j + 1] = fi; dists[j + 1] = di;
    }
    int currentPos = -1;
    int currentTarget = Entity_RefIndex(player->targetRef);
    for (int i = 0; i < count; i++) {
        if (foes[i] == currentTarget) currentPos = i;
    }
    int pos = (currentPos < 0) ? 0 : (currentPos + direction + count) % count;
    player->targetRef = Entity_RefOf(foes[pos]);
    g_manualGamepadTarget = true;
}

// True while a menu, panel or conversation owns the pad. The D-pad
// means "move the highlight" there, and must not simultaneously cycle
// world targets behind the window - which is what made choosing a
// dialogue reply also select a different NPC.
static bool MenuOwnsPad(void) {
    return UI_IsNpcDialogOpen() || UI_IsInventoryOpen() || UI_IsAttributesOpen() ||
           UI_IsEquipmentOpen() || UI_IsSkillsOpen() || UI_IsMapOpen();
}

static void UpdateGamepad(Entity *player, float dt, const Camera2D *camera) {
    if (!IsGamepadAvailable(GAMEPAD_ID)) return;

    bool menuOpen = MenuOwnsPad();

    // --- Left stick: direct movement (not click-to-move). While a
    // menu cursor is up (NPC dialog open), the stick steers the cursor
    // instead - ui_cursor.c reads it directly; we just stay out of the
    // way so browsing a shop doesn't also walk the player around. ---
    float lx = GetGamepadAxisMovement(GAMEPAD_ID, GAMEPAD_AXIS_LEFT_X);
    float ly = GetGamepadAxisMovement(GAMEPAD_ID, GAMEPAD_AXIS_LEFT_Y);
    float mag = sqrtf(lx * lx + ly * ly);
    if (mag > STICK_DEADZONE && !UICursor_Active()) {
        if (mag > 1.0f) { lx /= mag; ly /= mag; }
        // Casting roots the caster, and knockdown pins them - the stick
        // has to respect both, just like the click-to-move path does.
        if (!Entity_IsCasting(player) && player->knockdownTimer <= 0.0f) {
            // Crippled halves stick movement too, so the condition
            // costs you kiting whichever way you're steering.
            float speed = Entity_MoveSpeed(player);
            player->pos.x += lx * speed * dt;
            player->pos.y += ly * speed * dt;
            Rectangle b = World_GetBounds();
            if (player->pos.x < b.x) player->pos.x = b.x;
            if (player->pos.y < b.y) player->pos.y = b.y;
            if (player->pos.x > b.x + b.width) player->pos.x = b.x + b.width;
            if (player->pos.y > b.y + b.height) player->pos.y = b.y + b.height;
            World_ResolveBarriers(&player->pos, player->radius);
            player->hasMoveTarget = false; // stick overrides any pending click-move
            // Steering is a cancel: GW1 lets you break off a charge by
            // simply walking, and the target stays selected on the HUD.
            player->engaged = false;
        }
        g_gamepadMode = true;
    }

    // --- Skills: L2 + A/B/X/Y = slots 1-4, R2 + A/B/X/Y = slots 5-8 ---
    // Xbox-layout naming: A = face down, B = face right, X = face left,
    // Y = face up.
    int face = -1;
    if (IsGamepadButtonPressed(GAMEPAD_ID, GAMEPAD_BUTTON_RIGHT_FACE_DOWN)) face = 0;  // A
    else if (IsGamepadButtonPressed(GAMEPAD_ID, GAMEPAD_BUTTON_RIGHT_FACE_RIGHT)) face = 1; // B
    else if (IsGamepadButtonPressed(GAMEPAD_ID, GAMEPAD_BUTTON_RIGHT_FACE_LEFT)) face = 2;  // X
    else if (IsGamepadButtonPressed(GAMEPAD_ID, GAMEPAD_BUTTON_RIGHT_FACE_UP)) face = 3;    // Y

    // Skills and the attack order are world actions: while a window is
    // up, A is the menu cursor's click button and must not also swing a
    // sword behind it. B (back out) and X (talk) stay live because they
    // are how you leave the window in the first place.
    if (face == 0 && menuOpen) face = -1;

    if (face >= 0) {
        bool l2 = !menuOpen && TriggerDown(GAMEPAD_AXIS_LEFT_TRIGGER, GAMEPAD_BUTTON_LEFT_TRIGGER_2);
        bool r2 = !menuOpen && TriggerDown(GAMEPAD_AXIS_RIGHT_TRIGGER, GAMEPAD_BUTTON_RIGHT_TRIGGER_2);
        if (l2) {
            Combat_ActivateSkill(PLAYER_INDEX, face, Entity_RefIndex(player->targetRef));
            g_gamepadMode = true;
        } else if (r2) {
            Combat_ActivateSkill(PLAYER_INDEX, 4 + face, Entity_RefIndex(player->targetRef));
            g_gamepadMode = true;
        } else if (face == 1) {
            // Bare B (no trigger): close the map, then back out of an
            // open conversation, then drop the current target -
            // GW1-gamepad's escape hatch, layered.
            if (UI_IsMapOpen()) {
                // handled as a toggle in ui_map.c via Select; B mirrors
                // Escape here by just closing it
                UI_CloseMapOverlay();
            } else if (UI_IsNpcDialogOpen()) {
                // Deliberately not handled here. ui_panels.c owns backing
                // out of a conversation, because it is the only place
                // that knows whether a shop or trainer window is layered
                // on top and should close first. Closing the whole
                // dialog from here would skip that step.
            } else if (UI_IsInventoryOpen() || UI_IsAttributesOpen() ||
                       UI_IsEquipmentOpen() || UI_IsSkillsOpen()) {
                UI_ClosePanels();
            } else {
                player->targetRef = Entity_NoRef();
                g_manualGamepadTarget = false;
            }
            g_gamepadMode = true;
        } else if (face == 0) {
            // Bare A (Xbox) / Cross (PS): ATTACK the selected foe. This
            // is the pad's Space bar. Cycling with the D-pad or the
            // shoulders only puts a foe on the HUD; this is what
            // actually sends you (and the party) at it.
            Entity *t = Entity_Resolve(player->targetRef);
            if (t && t->alive && t->team != player->team) {
                player->engaged = true;
                player->hasMoveTarget = false;
            }
            g_gamepadMode = true;
        } else if (face == 2) {
            // Bare X (Xbox) / Square (PS): talk to the NPC you're
            // standing next to - the one the world overlay is already
            // highlighting with a "Talk" prompt. Deliberately does
            // nothing when nobody is in range rather than walking you
            // toward some distant NPC you didn't pick: interact means
            // "the one I can see is highlighted", never a guess. While a
            // dialog is already open this is a no-op here: ui_panels.c
            // treats the same button as "advance the conversation".
            if (!UI_IsNpcDialogOpen() && g_interactNpc >= 0) {
                UI_OpenNpcDialog(g_interactNpc);
            }
            g_gamepadMode = true;
        }
    }

    // No per-panel pad bindings. Start opens the menu and every screen
    // hangs off it - one button to remember instead of a stick-click for
    // the build editor and a face button for the bags.

    // --- Targeting. All of it is suppressed while a window is up: the
    // D-pad belongs to that window's highlight, and the shoulders would
    // otherwise be quietly re-aiming the world behind it. ---
    if (menuOpen) return;

    // Shoulder buttons: cycle visible enemies, L1 backward and R1
    // forward through the nearest-first order.
    if (IsGamepadButtonPressed(GAMEPAD_ID, GAMEPAD_BUTTON_LEFT_TRIGGER_1)) {
        CycleFoeTarget(player, -1, camera);
        g_gamepadMode = true;
    }
    if (IsGamepadButtonPressed(GAMEPAD_ID, GAMEPAD_BUTTON_RIGHT_TRIGGER_1)) {
        CycleFoeTarget(player, +1, camera);
        g_gamepadMode = true;
    }

    // --- D-pad targeting, matching GW1's official gamepad scheme:
    // up/down selects party members, left/right cycles enemies. ---
    if (IsGamepadButtonPressed(GAMEPAD_ID, GAMEPAD_BUTTON_LEFT_FACE_UP)) {
        CyclePartyTarget(player, -1);
        g_gamepadMode = true;
    }
    if (IsGamepadButtonPressed(GAMEPAD_ID, GAMEPAD_BUTTON_LEFT_FACE_DOWN)) {
        CyclePartyTarget(player, +1);
        g_gamepadMode = true;
    }
    if (IsGamepadButtonPressed(GAMEPAD_ID, GAMEPAD_BUTTON_LEFT_FACE_LEFT)) {
        CycleFoeTarget(player, -1, camera);
        g_gamepadMode = true;
    }
    if (IsGamepadButtonPressed(GAMEPAD_ID, GAMEPAD_BUTTON_LEFT_FACE_RIGHT)) {
        CycleFoeTarget(player, +1, camera);
        g_gamepadMode = true;
    }

    // A manual selection expires when the target dies.
    if (g_manualGamepadTarget) {
        Entity *t = Entity_Resolve(player->targetRef);
        if (!t || !t->alive) {
            g_manualGamepadTarget = false;
            player->targetRef = Entity_NoRef();
        }
    }

    // --- Auto-targeting: nearest foe within attack range ---
    // Convenience only. It picks what the HUD shows when you haven't
    // chosen anything yourself; it never sets `engaged`, so walking near
    // a monster no longer starts a fight on its own. Suspended while a
    // D-pad selection is live so cycling isn't instantly overwritten.
    if (g_gamepadMode && !g_manualGamepadTarget) {
        int best = -1;
        float bestDist = 1e9f;
        for (int i = 0; i < g_entityCount; i++) {
            Entity *e = &g_entities[i];
            if (!e->alive || e->kind != ENT_MONSTER || e->team == player->team) continue;
            float dx = e->pos.x - player->pos.x, dy = e->pos.y - player->pos.y;
            float d = sqrtf(dx * dx + dy * dy);
            if (d <= player->attackRange && d < bestDist) {
                bestDist = d;
                best = i;
            }
        }
        player->targetRef = Entity_RefOf(best);
    }
}

void Input_Update(Camera2D *camera, float dt) {
    // Scroll wheel zoom, independent of window size - this is the direct
    // fix for "things are too small to see" on a high-res display.
    float wheel = GetMouseWheelMove();
    if (wheel != 0.0f) {
        g_userAdjustedZoom = true;
        camera->zoom += wheel * 0.1f;
        if (camera->zoom < MIN_CAMERA_ZOOM) camera->zoom = MIN_CAMERA_ZOOM;
        if (camera->zoom > MAX_CAMERA_ZOOM) camera->zoom = MAX_CAMERA_ZOOM;
    }

    // The GW1-style menu cursor: exists while an NPC dialog is open and
    // a pad is present; the left stick steers it (see UpdateGamepad's
    // movement suppression) and A clicks. Updated here in the input
    // phase so the same frame's menu drawing sees fresh pointer state.
    UICursor_Update(dt, UI_IsNpcDialogOpen() || UI_IsInventoryOpen() ||
                        UI_IsAttributesOpen() || UI_IsEquipmentOpen() ||
                        UI_IsSkillsOpen());

    Entity *player = Entity_Get(PLAYER_INDEX);
    if (!player || !player->alive) {
        g_interactNpc = -1;
        g_hoverEntity = -1;
        return;
    }

    // Resolve, once per frame, who the interact button would talk to and
    // what the mouse is over. The world overlay draws both, so what's
    // highlighted is exactly what a press or click will act on.
    g_interactNpc = UI_IsNpcDialogOpen() ? -1 : FindInteractableNpc(player);
    g_hoverEntity = -1;
    {
        Vector2 mouseScreen = GetMousePosition();
        if (!UIHit_Contains(mouseScreen)) {
            Vector2 world = GetScreenToWorld2D(mouseScreen, *camera);
            for (int i = 0; i < g_entityCount; i++) {
                Entity *e = &g_entities[i];
                if (!e->alive || i == PLAYER_INDEX) continue;
                if (CheckCollisionPointCircle(world, e->pos, e->radius + 4.0f)) {
                    g_hoverEntity = i;
                    break;
                }
            }
        }
    }

    // Keyboard interact: same proximity rule as the pad's X/Square, so
    // the on-screen prompt means the same thing on both.
    if (IsKeyPressed(KEY_F) && !UI_IsNpcDialogOpen() && g_interactNpc >= 0) {
        UI_OpenNpcDialog(g_interactNpc);
    }

    // Explicit manual override: always clears the current target/chase
    // order, regardless of what a click resolves to. A guaranteed way to
    // stop auto-chasing an out-of-range target without needing to land a
    // click that's certain to miss every entity.
    if (IsKeyPressed(KEY_ESCAPE)) {
        player->targetRef = Entity_NoRef();
        player->hasMoveTarget = false;
        player->engaged = false;
    }

    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && !UIHit_Contains(GetMousePosition())) {
        g_gamepadMode = false; // mouse takes back targeting control

        Vector2 mouseScreen = GetMousePosition();
        Vector2 world = GetScreenToWorld2D(mouseScreen, *camera);

        int clickedEntity = -1;
        for (int i = 0; i < g_entityCount; i++) {
            Entity *e = &g_entities[i];
            if (!e->alive || i == PLAYER_INDEX) continue;
            if (CheckCollisionPointCircle(world, e->pos, e->radius + 4.0f)) {
                clickedEntity = i;
                break;
            }
        }

        if (clickedEntity >= 0 && g_entities[clickedEntity].team != player->team) {
            // Clicking a foe directly IS the attack order - the pointer
            // equivalent of pressing Space. Cycling with Tab or the pad
            // deliberately is not.
            player->targetRef = Entity_RefOf(clickedEntity);
            player->engaged = true;
        } else if (clickedEntity >= 0 && g_entities[clickedEntity].kind == ENT_HERO) {
            // Clicking a party member selects them, so ally-targeted
            // spells (Orison) land on them instead of self-falling back.
            player->targetRef = Entity_RefOf(clickedEntity);
        } else if (clickedEntity >= 0 && g_entities[clickedEntity].kind == ENT_NPC) {
            // Outpost NPC: talk if close enough, otherwise walk over to
            // them (click again on arrival to open the conversation).
            Entity *npc = &g_entities[clickedEntity];
            float dx = npc->pos.x - player->pos.x, dy = npc->pos.y - player->pos.y;
            if (sqrtf(dx * dx + dy * dy) <= NPC_TALK_DISTANCE) {
                UI_OpenNpcDialog(clickedEntity);
            } else {
                player->moveTarget = npc->pos;
                player->hasMoveTarget = true;
            }
        } else {
            // Defensive cap: bound how far a single click can send the
            // player. If screen-to-world coordinates are ever wrong (e.g.
            // a screen-size/mouse-position mismatch on some display
            // configurations), this stops it from becoming an
            // effectively endless, unrecoverable walk - worst case is an
            // obviously-too-far move you can immediately click to correct.
            Vector2 delta = { world.x - player->pos.x, world.y - player->pos.y };
            float dist = sqrtf(delta.x * delta.x + delta.y * delta.y);
            if (dist > MAX_CLICK_MOVE_DISTANCE) {
                float scale = MAX_CLICK_MOVE_DISTANCE / dist;
                world.x = player->pos.x + delta.x * scale;
                world.y = player->pos.y + delta.y * scale;
            }

            player->targetRef = Entity_NoRef();
            player->moveTarget = world;
            player->hasMoveTarget = true;
            player->engaged = false; // walking somewhere calls off the charge
        }
    }

    // Space: GW1's action key. Selecting a foe (Tab, C, the pad's D-pad)
    // only shows it on the HUD - this is the order that actually sends
    // you at it, and it is what a skill cast does implicitly.
    if (IsKeyPressed(KEY_SPACE)) {
        Entity *t = Entity_Resolve(player->targetRef);
        if (t && t->alive && t->team != player->team) {
            player->engaged = true;
            player->hasMoveTarget = false;
        }
    }

    // GW1's classic keyboard targeting: C = nearest foe, Tab = cycle foes.
    if (IsKeyPressed(KEY_C)) {
        g_manualGamepadTarget = false;
        EntityRef save = player->targetRef;
        player->targetRef = Entity_NoRef(); // force "nearest" rather than "next"
        CycleFoeTarget(player, +1, camera);
        if (Entity_RefIndex(player->targetRef) < 0) player->targetRef = save;
        g_manualGamepadTarget = false;
    }
    if (IsKeyPressed(KEY_TAB)) {
        CycleFoeTarget(player, +1, camera);
        g_manualGamepadTarget = false;
    }

    int keys[8] = { KEY_ONE, KEY_TWO, KEY_THREE, KEY_FOUR, KEY_FIVE, KEY_SIX, KEY_SEVEN, KEY_EIGHT };
    for (int i = 0; i < 8; i++) {
        if (IsKeyPressed(keys[i])) {
            Combat_ActivateSkill(PLAYER_INDEX, i, Entity_RefIndex(player->targetRef));
        }
    }

    UpdateGamepad(player, dt, camera);
}
