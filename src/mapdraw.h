#ifndef MAPDRAW_H
#define MAPDRAW_H

#include "raylib.h"

// GW1's two map annotations, both in WORLD space so the map and the
// compass can draw them at their own scales:
//
//   - The trail: a breadcrumb of where you've walked this instance, so
//     you can retrace your steps. Reset on every zone load, like GW1's
//     per-instance map.
//   - Drawings: freehand strokes you scribble on the map, which fade on
//     their own after a while. One "stroke" is a connected drag.
//
// The module owns the buffers; the map and compass only read them, and
// feed the player position (trail) and pointer drags (drawings) in.

void MapDraw_Reset(void);              // called when a zone loads
void MapDraw_Update(float dt);         // ages out old drawings

// Breadcrumb: record the player's position; ignores tiny moves so the
// buffer covers real ground rather than jitter.
void MapDraw_RecordStep(Vector2 worldPos);
int  MapDraw_TrailCount(void);
Vector2 MapDraw_TrailAt(int i);

// Freehand drawing: begin a stroke on press, then add points as the drag
// moves. Points carry an age so the renderer can fade them out.
void MapDraw_BeginStroke(void);
void MapDraw_AddPoint(Vector2 worldPos);
typedef struct { Vector2 pos; int stroke; float age; } MapStrokePt;
int  MapDraw_StrokeCount(void);
const MapStrokePt *MapDraw_StrokeAt(int i);
float MapDraw_StrokeTTL(void);

#endif
