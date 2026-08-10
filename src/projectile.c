#include "projectile.h"
#include "entity.h"
#include "fx.h"
#include "area.h"
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
        p->targetRef = Entity_RefOf(targetIndex);
        p->shooterRef = Entity_RefOf(shooterIndex);
        p->damage = damage;
        p->speed = PROJECTILE_SPEED;
        p->color = (shooter->team == 0) ? (Color){ 230, 220, 160, 255 }
                                        : (Color){ 230, 120, 100, 255 };
        return;
    }
}

static void Impact(Projectile *p) {
    Entity *target = Entity_Resolve(p->targetRef);
    Entity *shooter = Entity_Resolve(p->shooterRef);
    if (!target || !target->alive) return;

    float dx = target->pos.x - p->landingPoint.x;
    float dy = target->pos.y - p->landingPoint.y;
    if (sqrtf(dx * dx + dy * dy) > DODGE_RADIUS) {
        // Moved off the landing point in time: clean dodge, GW1-style.
        target->dodgeFlashTimer = 1.0f;
        return;
    }

    // A missile is an attack, so Blind (on the shooter) and a block stance
    // (on the target) can still stop it at the last moment - a blocked
    // arrow does no damage and earns the shooter no adrenaline.
    if (shooter && Entity_ResolveAttack(shooter, target) != ATTACK_LANDS) return;

    // Weapon mods ride the shot too, read off the shooter at impact; a
    // Winnowing spirit adds to the hit if the target stands in its range.
    float pen = shooter ? shooter->weaponArmorPen / 100.0f : 0.0f;
    Entity_ApplyDamagePen(target, p->damage + Area_BonusAttackDamage(target), shooter, pen);
    Fx_Burst(target->pos, p->color);
    if (shooter && shooter->alive) {
        if (shooter->weaponLifesteal > 0) {
            shooter->hp += shooter->weaponLifesteal;
            if (shooter->hp > shooter->maxHp) shooter->hp = shooter->maxHp;
        }
        if (shooter->weaponEnergyGain > 0) { // Zealous
            shooter->energy += shooter->weaponEnergyGain;
            if (shooter->energy > shooter->maxEnergy) shooter->energy = shooter->maxEnergy;
        }
        Entity_GainAdrenalineStrike(shooter); // a landed shot is a strike too
        if (target->kind == ENT_MONSTER && !target->aggroed) {
            // A landed ranged hit wakes a sleeping monster - the wand
            // pull - and its whole group answers, GW1-style.
            Entity_WakeMonsterGroup(target, p->shooterRef);
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
