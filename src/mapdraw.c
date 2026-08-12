#include "mapdraw.h"
#include <string.h>

#define TRAIL_MAX   256
#define STROKE_MAX  512
#define STROKE_TTL  25.0f   // seconds a drawing lasts before it fades out
#define TRAIL_MIN_STEP2 (18.0f * 18.0f) // don't record moves smaller than this

static Vector2 g_trail[TRAIL_MAX];
static int g_trailCount = 0;

static MapStrokePt g_strokes[STROKE_MAX];
static int g_strokeCount = 0;
static int g_curStroke = 0;

void MapDraw_Reset(void) {
    g_trailCount = 0;
    g_strokeCount = 0;
    g_curStroke = 0;
}

void MapDraw_Update(float dt) {
    // Age drawings and drop the expired ones, keeping order.
    int w = 0;
    for (int i = 0; i < g_strokeCount; i++) {
        g_strokes[i].age += dt;
        if (g_strokes[i].age < STROKE_TTL) g_strokes[w++] = g_strokes[i];
    }
    g_strokeCount = w;
}

void MapDraw_RecordStep(Vector2 worldPos) {
    if (g_trailCount > 0) {
        Vector2 last = g_trail[g_trailCount - 1];
        float dx = worldPos.x - last.x, dy = worldPos.y - last.y;
        if (dx * dx + dy * dy < TRAIL_MIN_STEP2) return;
    }
    if (g_trailCount < TRAIL_MAX) {
        g_trail[g_trailCount++] = worldPos;
    } else {
        // Drop the oldest breadcrumb and keep walking.
        memmove(&g_trail[0], &g_trail[1], sizeof(Vector2) * (TRAIL_MAX - 1));
        g_trail[TRAIL_MAX - 1] = worldPos;
    }
}

int MapDraw_TrailCount(void) { return g_trailCount; }
Vector2 MapDraw_TrailAt(int i) {
    if (i < 0 || i >= g_trailCount) return (Vector2){ 0, 0 };
    return g_trail[i];
}

void MapDraw_BeginStroke(void) { g_curStroke++; }

void MapDraw_AddPoint(Vector2 worldPos) {
    if (g_strokeCount >= STROKE_MAX) {
        // Full: shed the oldest point so a fresh scribble still shows.
        memmove(&g_strokes[0], &g_strokes[1], sizeof(MapStrokePt) * (STROKE_MAX - 1));
        g_strokeCount--;
    }
    g_strokes[g_strokeCount++] = (MapStrokePt){ worldPos, g_curStroke, 0.0f };
}

int MapDraw_StrokeCount(void) { return g_strokeCount; }
const MapStrokePt *MapDraw_StrokeAt(int i) {
    if (i < 0 || i >= g_strokeCount) return NULL;
    return &g_strokes[i];
}
float MapDraw_StrokeTTL(void) { return STROKE_TTL; }
