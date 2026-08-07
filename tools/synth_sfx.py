#!/usr/bin/env python3
"""Generates the sound effects no CC0 pack covers.

    python3 tools/synth_sfx.py

Kenney's audio packs are excellent Foley - footsteps, impacts, doors,
coins - and contain no magic and no bow whatsoever. Rather than take on
per-asset licence checking across OpenGameArt or Freesound for four
sounds, these are synthesised, which also keeps them in character with
sprite.c and fx.c: procedural where procedural is good enough.

Output is committed, so the build needs no Python. Re-run only when a
clip changes. Deterministic - the same seed gives the same waveform.
"""
import math, os, random, struct, wave

SR = 44100
OUT = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
                   "assets", "audio")


def env(n, attack, curve=2.0):
    """Attack then decay-to-zero over n samples. `attack` in seconds."""
    a = max(1, int(attack * SR))
    out = []
    for i in range(n):
        if i < a:
            out.append(i / a)
        else:
            t = (i - a) / max(1, n - a)
            out.append(max(0.0, (1.0 - t) ** curve))
    return out


def lowpass(buf, cutoff):
    """One-pole lowpass; `cutoff` may be per-sample, which is what turns a
    static filter into a sweep."""
    out, y = [], 0.0
    for i, x in enumerate(buf):
        fc = cutoff[i] if isinstance(cutoff, list) else cutoff
        a = 1.0 - math.exp(-2.0 * math.pi * fc / SR)
        y += a * (x - y)
        out.append(y)
    return out


def normalise(buf, peak=0.82):
    m = max(abs(v) for v in buf) or 1.0
    return [v * peak / m for v in buf]


def fire_bolt(dur=0.60):
    """Offensive elemental. Noise through a falling resonant sweep over a
    dropping body: fire reads as broadband energy moving downward."""
    n = int(dur * SR)
    e = env(n, 0.008, 2.4)
    noise = [random.uniform(-1, 1) for _ in range(n)]
    sweep = [5200 * (1.0 - 0.86 * (i / n)) + 180 for i in range(n)]
    filt = lowpass(noise, sweep)
    out, ph = [], 0.0
    for i in range(n):
        ph += 2 * math.pi * (190 * (1.0 - 0.55 * (i / n))) / SR
        out.append(math.tanh((filt[i] * 0.85 + math.sin(ph) * 0.55) * 1.7) * e[i])
    return normalise(out)


def heal(dur=0.85):
    """A soft major triad drifting slightly upward. Rising pitch is what
    makes a sound read as relief rather than as an event."""
    n = int(dur * SR)
    e = env(n, 0.035, 1.6)
    out = [0.0] * n
    for f, amp in ((523.25, 1.0), (659.25, 0.62), (783.99, 0.48), (1046.5, 0.24)):
        ph = 0.0
        for i in range(n):
            ph += 2 * math.pi * f * (1.0 + 0.018 * (i / n)) / SR
            out[i] += math.sin(ph) * amp * (1.0 + 0.05 * math.sin(2 * math.pi * 5.5 * i / SR))
    return normalise([out[i] * e[i] for i in range(n)], 0.68)


def hex_land(dur=0.75):
    """Two low tones a semitone apart, beating against each other under
    filtered noise. The dissonance does the work, not the volume."""
    n = int(dur * SR)
    e = env(n, 0.05, 1.9)
    noise = lowpass([random.uniform(-1, 1) for _ in range(n)], 900)
    out, p1, p2 = [], 0.0, 0.0
    for i in range(n):
        p1 += 2 * math.pi * 98.0 / SR
        p2 += 2 * math.pi * 103.8 / SR
        s = math.sin(p1) * 0.7 + math.sin(p2) * 0.55 + noise[i] * 0.35
        out.append((s + math.sin(p1 * 2.02) * 0.16) * e[i])
    return normalise(out, 0.74)


def arcane(dur=0.50):
    """The neutral cast, for spells that neither heal nor hex. Brighter and
    drier than the heal so the two never get confused mid-fight."""
    n = int(dur * SR)
    e = env(n, 0.012, 2.6)
    out, phases = [0.0] * n, [0.0, 0.0, 0.0]
    freqs = [880.0, 1174.7, 1567.98]
    for k, f in enumerate(freqs):
        ph = 0.0
        for i in range(n):
            ph += 2 * math.pi * f * (1.0 - 0.10 * (i / n)) / SR
            out[i] += math.sin(ph) * (0.9 - 0.22 * k)
    shimmer = lowpass([random.uniform(-1, 1) for _ in range(n)], 4200)
    return normalise([(out[i] * 0.55 + shimmer[i] * 0.30) * e[i] for i in range(n)], 0.72)


def bow_release(dur=0.26):
    """String snap plus air off the rest. Short and dry - a bow shot with
    a tail sounds like a harp."""
    n = int(dur * SR)
    e = env(n, 0.001, 4.0)
    noise = lowpass([random.uniform(-1, 1) for _ in range(n)],
                    [6000 * (1 - 0.9 * (i / n)) + 300 for i in range(n)])
    out, ph = [], 0.0
    for i in range(n):
        ph += 2 * math.pi * (240 * math.exp(-9.0 * i / n)) / SR
        out.append((math.sin(ph) * 0.5 + noise[i] * 0.8) * e[i])
    return normalise(out, 0.78)


def write(name, mono):
    """Mono 16-bit: these are point events, and the engine pans them."""
    path = os.path.join(OUT, name + ".wav")
    with wave.open(path, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(SR)
        w.writeframes(b"".join(
            struct.pack("<h", max(-32767, min(32767, int(v * 32767)))) for v in mono))
    print("  %-16s %5.2fs  %6.1f KB" % (name, len(mono) / SR,
                                        os.path.getsize(path) / 1024.0))


if __name__ == "__main__":
    os.makedirs(OUT, exist_ok=True)
    random.seed(7)
    for name, fn in (("cast_fire", fire_bolt), ("cast_heal", heal),
                     ("cast_hex", hex_land), ("cast_arcane", arcane),
                     ("bow_release", bow_release)):
        write(name, fn())
