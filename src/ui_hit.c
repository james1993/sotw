#include "ui_hit.h"

#define MAX_UI_CLAIMS 64

static Rectangle g_current[MAX_UI_CLAIMS];
static int g_currentCount = 0;
static Rectangle g_previous[MAX_UI_CLAIMS];
static int g_previousCount = 0;

void UIHit_NewFrame(void) {
    for (int i = 0; i < g_currentCount; i++) g_previous[i] = g_current[i];
    g_previousCount = g_currentCount;
    g_currentCount = 0;
}

void UIHit_Claim(Rectangle rect) {
    if (g_currentCount < MAX_UI_CLAIMS) {
        g_current[g_currentCount++] = rect;
    }
}

bool UIHit_Contains(Vector2 point) {
    for (int i = 0; i < g_previousCount; i++) {
        if (CheckCollisionPointRec(point, g_previous[i])) return true;
    }
    return false;
}
