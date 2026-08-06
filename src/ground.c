#include "ground.h"
#include "assets.h"

static Texture2D g_cobble; // laid stone: outposts
static Texture2D g_rough;  // broken ground: explorables
static bool g_ready = false;

// World units per repeat, per surface. The two patterns have different
// natural feature sizes, so one number can't serve both: laid stone
// wants flagstones a little wider than a character, broken ground wants
// its cells tighter than that or it stops reading as ground and starts
// reading as a pattern. Both are multiples of the 40-unit grid so the
// lattice and the surface never beat against each other.
#define GROUND_TILE_OUTPOST 160.0f
#define GROUND_TILE_WILD    120.0f

// How strongly the pattern shows through. Ground is meant to be felt,
// not read - past about 40 the tiling starts announcing itself.
#define GROUND_ALPHA 30

static bool LoadTile(Texture2D *out, const char *relative) {
    char path[512];
    if (!Assets_ResolvePath(relative, path, sizeof(path))) return false;
    *out = LoadTexture(path);
    if (out->id == 0) return false;
    // REPEAT is what makes one draw call cover the whole view: the
    // source rectangle below is deliberately larger than the texture.
    SetTextureWrap(*out, TEXTURE_WRAP_REPEAT);
    GenTextureMipmaps(out);
    SetTextureFilter(*out, TEXTURE_FILTER_TRILINEAR);
    return true;
}

void Ground_Init(void) {
    bool a = LoadTile(&g_cobble, "assets/ground/cobble.png");
    bool b = LoadTile(&g_rough, "assets/ground/rough.png");
    g_ready = a && b;
    if (!g_ready) {
        TraceLog(LOG_WARNING, "GROUND: tiles missing - zones keep the flat fill");
    }
}

void Ground_Unload(void) {
    if (g_cobble.id != 0) UnloadTexture(g_cobble);
    if (g_rough.id != 0) UnloadTexture(g_rough);
    g_cobble.id = g_rough.id = 0;
    g_ready = false;
}

void Ground_Draw(Rectangle view, Color tint, bool outpost) {
    if (!g_ready) return;
    const Texture2D *tex = outpost ? &g_cobble : &g_rough;
    float tileWorld = outpost ? GROUND_TILE_OUTPOST : GROUND_TILE_WILD;

    // Source in texels, destination in world units. Anchoring the source
    // to the view's own world position is what keeps the pattern nailed
    // to the ground instead of sliding with the camera.
    float texels = (float)tex->width;
    float perUnit = texels / tileWorld;
    Rectangle src = { view.x * perUnit, view.y * perUnit,
                      view.width * perUnit, view.height * perUnit };

    DrawTexturePro(*tex, src, view, (Vector2){ 0, 0 }, 0.0f,
                   (Color){ tint.r, tint.g, tint.b, GROUND_ALPHA });
}
