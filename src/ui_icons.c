#include "ui_icons.h"
#include "assets.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static Texture2D g_atlas;
static bool g_ready = false;
static int g_cell = 128;
static int g_cols = 8;
static int g_count = 0;

// The atlas geometry is written by the build script rather than
// hardcoded here, so changing the cell size or column count is a one-
// line edit in one place instead of a silent mismatch.
static void ReadMeta(void) {
    char path[512];
    if (!Assets_ResolvePath("assets/icons/skills.atlas", path, sizeof(path))) return;
    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[128];
    while (fgets(line, sizeof(line), f)) {
        int v;
        if (sscanf(line, "cell=%d", &v) == 1) g_cell = v;
        else if (sscanf(line, "cols=%d", &v) == 1) g_cols = v;
        else if (sscanf(line, "count=%d", &v) == 1) g_count = v;
    }
    fclose(f);
}

void UIIcons_Init(void) {
    ReadMeta();

    char path[512];
    if (!Assets_ResolvePath("assets/icons/skills.png", path, sizeof(path))) {
        TraceLog(LOG_WARNING, "ICONS: skills.png not found - skill tiles will be plain");
        return;
    }

    g_atlas = LoadTexture(path);
    if (g_atlas.id == 0) return;

    // Bilinear + mipmaps: a 128px cell is drawn at anything from ~28px
    // in a list row to ~90px on the 4K skill bar, and nearest-neighbour
    // downscaling of a hard-edged silhouette crawls badly in motion.
    GenTextureMipmaps(&g_atlas);
    SetTextureFilter(g_atlas, TEXTURE_FILTER_TRILINEAR);

    if (g_cols > 0 && g_cell > 0) {
        int capacity = (g_atlas.width / g_cell) * (g_atlas.height / g_cell);
        if (g_count <= 0 || g_count > capacity) g_count = capacity;
    }
    g_ready = true;
    TraceLog(LOG_INFO, "ICONS: %d skill icons loaded (%dx%d, %dpx cells)",
             g_count, g_atlas.width, g_atlas.height, g_cell);
}

void UIIcons_Unload(void) {
    if (g_atlas.id != 0) UnloadTexture(g_atlas);
    g_atlas.id = 0;
    g_ready = false;
}

bool UIIcons_Ready(void) { return g_ready; }

void UIIcons_Draw(int index, Rectangle dst, Color tint) {
    if (!g_ready || index < 0 || index >= g_count) return;
    Rectangle src = { (float)((index % g_cols) * g_cell),
                      (float)((index / g_cols) * g_cell),
                      (float)g_cell, (float)g_cell };
    DrawTexturePro(g_atlas, src, dst, (Vector2){ 0, 0 }, 0.0f, tint);
}
