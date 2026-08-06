#ifndef ASSETS_H
#define ASSETS_H

#include <stdbool.h>

// Where a bundled asset actually lives at runtime.
//
// CMake copies assets/ next to the executable, so that's checked first;
// the repo-relative path is the fallback for running straight out of a
// build tree. Every loader goes through here so "assets/x" means the
// same thing whichever way the game was launched.
//
// Returns false and leaves `out` empty when nothing was found - callers
// are expected to degrade rather than fail, the way a missing font falls
// back to raylib's built-in.
bool Assets_ResolvePath(const char *relative, char *out, int outSize);

#endif
