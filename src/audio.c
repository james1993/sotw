#include "audio.h"
#include "assets.h"
#include "skill.h"
#include "raylib.h"
#include <stddef.h>
#include <string.h>

// One sound may be asked to play several times in the same instant -
// four party members landing hits on the same frame is normal. A raylib
// Sound can't overlap itself, so each one keeps a small ring of aliases
// that share the sample data and take turns.
#define VOICES_PER_SOUND 4

// Minimum gap between two plays of the SAME sound. Without it, an AoE
// landing on five foes fires five identical impacts a millisecond apart,
// which doesn't read as five hits - it reads as one loud clipped one.
#define RETRIGGER_GAP 0.045f

// Past this distance a sound is inaudible; below it, volume falls off
// linearly. Roughly the width of the visible field.
#define AUDIBLE_RANGE 900.0f

typedef struct {
    Sound base;
    Sound voices[VOICES_PER_SOUND];
    int next;
    bool loaded;
    float sinceLastPlay;
} SoundSlot;

static SoundSlot g_bank[SFX_COUNT];
static bool g_ready = false;
static int g_volume = 70;

// File per SoundId, in enum order. OGG for everything sourced from
// Kenney (unmodified, so the CC0 provenance stays obvious), WAV for the
// synthesised clips (tools/synth_sfx.py).
static const char *g_files[SFX_COUNT] = {
    "assets/audio/ui_move.ogg",
    "assets/audio/ui_click.ogg",
    "assets/audio/ui_open.ogg",
    "assets/audio/ui_close.ogg",
    "assets/audio/ui_deny.ogg",
    "assets/audio/ui_confirm.ogg",
    "assets/audio/loot.ogg",
    "assets/audio/level_up.ogg",
    "assets/audio/quest_done.ogg",
    "assets/audio/swing.ogg",
    "assets/audio/bow_release.wav",
    "assets/audio/hit.ogg",
    "assets/audio/hit_heavy.ogg",
    "assets/audio/death.ogg",
    "assets/audio/cast_fire.wav",
    "assets/audio/cast_heal.wav",
    "assets/audio/cast_hex.wav",
    "assets/audio/cast_arcane.wav",
};

// Per-sound trim, because the sources are mastered independently and a
// coin pickup has no business being as loud as a death. Tuned by ear is
// the right way to set these; these are a starting point.
static const float g_gain[SFX_COUNT] = {
    0.30f, // ui move - fires on every step of a D-pad; must be a whisper
    0.45f, // ui click - fires constantly, must sit under everything
    0.50f, // ui open
    0.50f, // ui close
    0.60f, // ui deny
    0.65f, // ui confirm
    0.70f, // loot
    0.85f, // level up
    0.85f, // quest done
    0.55f, // swing - every attack, every second
    0.60f, // bow
    0.70f, // hit
    0.85f, // heavy hit
    0.80f, // death
    0.75f, // cast fire
    0.70f, // cast heal
    0.75f, // cast hex
    0.70f, // cast arcane
};

void Audio_Init(void) {
    InitAudioDevice();
    if (!IsAudioDeviceReady()) {
        // No sound hardware (a headless box, a container, a machine with
        // no card). Everything below no-ops; the game is unchanged.
        TraceLog(LOG_WARNING, "AUDIO: no playback device - running silent");
        return;
    }

    int loaded = 0;
    for (int i = 0; i < SFX_COUNT; i++) {
        char path[512];
        if (!Assets_ResolvePath(g_files[i], path, sizeof(path))) continue;
        g_bank[i].base = LoadSound(path);
        if (g_bank[i].base.frameCount == 0) continue;
        for (int v = 0; v < VOICES_PER_SOUND; v++) {
            g_bank[i].voices[v] = LoadSoundAlias(g_bank[i].base);
        }
        g_bank[i].loaded = true;
        g_bank[i].sinceLastPlay = RETRIGGER_GAP;
        loaded++;
    }

    g_ready = true;
    Audio_SetVolume(g_volume);
    TraceLog(LOG_INFO, "AUDIO: %d/%d sounds loaded, volume %d%%",
             loaded, (int)SFX_COUNT, g_volume);
}

void Audio_Unload(void) {
    if (g_ready) {
        for (int i = 0; i < SFX_COUNT; i++) {
            if (!g_bank[i].loaded) continue;
            // Aliases first: they borrow the base sound's sample data.
            for (int v = 0; v < VOICES_PER_SOUND; v++) {
                UnloadSoundAlias(g_bank[i].voices[v]);
            }
            UnloadSound(g_bank[i].base);
            g_bank[i].loaded = false;
        }
        CloseAudioDevice();
    }
    g_ready = false;
}

void Audio_Update(float dt) {
    if (!g_ready) return;
    for (int i = 0; i < SFX_COUNT; i++) {
        if (g_bank[i].sinceLastPlay < RETRIGGER_GAP) g_bank[i].sinceLastPlay += dt;
    }
}

int Audio_GetVolume(void) { return g_volume; }

void Audio_SetVolume(int percent) {
    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;
    g_volume = percent;
    if (g_ready) SetMasterVolume((float)g_volume / 100.0f);
}

// The one place a sound actually starts. Everything else routes here so
// the retrigger guard and the voice rotation can't be bypassed.
static void PlayVoice(SoundId id, float volume, float pan) {
    if (!g_ready || id < 0 || id >= SFX_COUNT) return;
    SoundSlot *slot = &g_bank[id];
    if (!slot->loaded) return;
    if (slot->sinceLastPlay < RETRIGGER_GAP) return;
    slot->sinceLastPlay = 0.0f;

    // Suppressed at the default log level. Kept because audio is the one
    // subsystem you cannot verify by looking at a screenshot, and being
    // able to see WHICH sound fired is the whole debugging story.
    TraceLog(LOG_DEBUG, "AUDIO: play %d", (int)id);
    Sound s = slot->voices[slot->next];
    slot->next = (slot->next + 1) % VOICES_PER_SOUND;

    SetSoundVolume(s, g_gain[id] * volume);
    SetSoundPan(s, pan);
    PlaySound(s);
}

void Audio_Play(SoundId id) {
    PlayVoice(id, 1.0f, 0.5f); // 0.5 is centre in raylib
}

void Audio_PlayAt(SoundId id, float dxFromPlayer) {
    float dist = dxFromPlayer < 0 ? -dxFromPlayer : dxFromPlayer;
    if (dist > AUDIBLE_RANGE) return;

    float volume = 1.0f - (dist / AUDIBLE_RANGE) * 0.75f;
    // raylib's pan is 0 = right, 1 = left. Kept partial (0.15..0.85) so a
    // sound never abandons one ear entirely, which is disorienting on
    // headphones for something happening on screen.
    float pan = 0.5f - (dxFromPlayer / AUDIBLE_RANGE) * 0.35f;
    if (pan < 0.15f) pan = 0.15f;
    if (pan > 0.85f) pan = 0.85f;
    PlayVoice(id, volume, pan);
}

void Audio_PlaySkill(const Skill *skill) {
    if (!skill) return;

    if (skill->type == SKILLTYPE_ATTACK_SKILL) {
        // A ranged ATTACK SKILL is a bow shot - Marksmanship is the only
        // line that has them, so unlike auto-attacks there's no caster
        // case to get wrong here.
        Audio_Play(skill->range > 60.0f ? SFX_BOW : SFX_SWING);
        return;
    }

    // What a skill DOES beats what it belongs to: a heal sounds like a
    // heal whether it came from Healing Prayers or Blood Magic, and the
    // check falls out of the effect steps that are already there.
    bool heals = false, hexes = false;
    for (int i = 0; i < skill->stepCount; i++) {
        if (skill->steps[i].kind == FX_APPLY_HEX) hexes = true;
        if (skill->steps[i].kind == FX_HEAL) heals = true;
    }
    if (hexes)      Audio_Play(SFX_CAST_HEX);
    else if (heals) Audio_Play(SFX_CAST_HEAL);
    else {
        // Damage-dealing magic reads as elemental; everything else -
        // signets, shouts, energy management - gets the neutral cast.
        bool damages = false;
        for (int i = 0; i < skill->stepCount; i++) {
            if (skill->steps[i].kind == FX_DAMAGE) damages = true;
        }
        Audio_Play(damages ? SFX_CAST_FIRE : SFX_CAST_ARCANE);
    }
}
