#include "projectile.h"
#include "entity.h"
#include <math.h>
#include <string.h>

#define PROJECTILE_SPEED 300.0f
// How far the target must be from the locked landing point at impact to
// escape the hit. A bit over two body-radii: sidestepping works, twitching
// doesn't.
#define DODGE_RADIUS 28.0f

Projectile g_projectiles[MAX_PROJECTILES];

void Projectile_ClearAll(void) {
    memset(g_projectiles, 0, sizeof(g_projectiles));
}

void Projectile_Spawn(int shooterIndex, int targetIndex, int damage) {
    Entity *shooter = Entity_Get(shooterIndex);
    Entity *target = Entity_Get(targetIndex);
    if (!shooter || !target) return;

    for (int i = 0; i < MAX_PROJECTILES; i++) {
        Projectile *p = &g_projectiles[i];
        if (p->active) continue;
        p->active = true;
        p->pos = shooter->pos;
        p->landingPoint = target->pos; // locked NOW - this is what makes dodging possible
        p->targetIndex = targetIndex;
        p->shooterIndex = shooterIndex;
        p->damage = damage;
        p->speed = PROJECTILE_SPEED;
        p->color = (shooter->team == 0) ? (Color){ 230, 220, 160, 255 }
                                        : (Color){ 230, 120, 100, 255 };
        return;
    }
}

static void Impact(Projectile *p) {
    Entity *target = Entity_Get(p->targetIndex);
    Entity *shooter = Entity_Get(p->shooterIndex);
    if (!target || !target->alive) return;

    float dx = target->pos.x - p->landingPoint.x;
    float dy = target->pos.y - p->landingPoint.y;
    if (sqrtf(dx * dx + dy * dy) > DODGE_RADIUS) {
        // Moved off the landing point in time: clean dodge, GW1-style.
        target->dodgeFlashTimer = 1.0f;
        return;
    }

    Entity_ApplyDamage(target, p->damage, shooter);
    if (shooter && shooter->alive) {
        shooter->adrenaline += 4; // landed hits build adrenaline, as in melee
        if (shooter->adrenaline > 100) shooter->adrenaline = 100;
        if (target->kind == ENT_MONSTER && !target->aggroed) {
            // A landed ranged hit wakes a sleeping monster - the wand pull.
            target->aggroed = true;
            target->targetIndex = p->shooterIndex;
        }
    }
}

void Projectile_Update(float dt) {
    for (int i = 0; i < MAX_PROJECTILES; i++) {
        Projectile *p = &g_projectiles[i];
        if (!p->active) continue;

        float dx = p->landingPoint.x - p->pos.x;
        float dy = p->landingPoint.y - p->pos.y;
        float dist = sqrtf(dx * dx + dy * dy);
        float step = p->speed * dt;

        if (step >= dist) {
            Impact(p);
            p->active = false;
        } else {
            p->pos.x += dx / dist * step;
            p->pos.y += dy / dist * step;
        }
    }
}

void Projectile_Draw(void) {
    for (int i = 0; i < MAX_PROJECTILES; i++) {
        Projectile *p = &g_projectiles[i];
        if (!p->active) continue;
        // A short streak pointing along the flight path reads better at
        // speed than a dot.
        float dx = p->landingPoint.x - p->pos.x;
        float dy = p->landingPoint.y - p->pos.y;
        float d = sqrtf(dx * dx + dy * dy);
        if (d < 1.0f) d = 1.0f;
        Vector2 tail = { p->pos.x - dx / d * 10.0f, p->pos.y - dy / d * 10.0f };
        DrawLineEx(tail, p->pos, 3.0f, p->color);
        DrawCircleV(p->pos, 2.5f, RAYWHITE);
    }
}
