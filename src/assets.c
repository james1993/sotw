#include "assets.h"
#include "raylib.h"
#include <stdio.h>

bool Assets_ResolvePath(const char *relative, char *out, int outSize) {
    if (!relative || !out || outSize <= 0) return false;

    // GetApplicationDirectory() includes the trailing separator.
    snprintf(out, outSize, "%s%s", GetApplicationDirectory(), relative);
    if (FileExists(out)) return true;

    snprintf(out, outSize, "%s", relative);
    if (FileExists(out)) return true;

    out[0] = '\0';
    return false;
}
