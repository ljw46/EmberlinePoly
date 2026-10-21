#!/usr/bin/env python3
"""Renders the small sample set the plugin's AUDITION sound uses, into Assets/audition/.

The samples are made here, from scratch (plucked string, FM keys, FM bell, additive bass and pad), so they carry no third-party licence: they
are covered by the same licence as the code. The plugin plays them back with a plain resampling sampler, so there is no synthesis at run time.

    python3 tools/make_audition_samples.py            # needs numpy

File names are <sound>_<midi note>.wav (16-bit mono, 24 kHz). Each sound has a few root notes an octave apart; the sampler picks the nearest one
and resamples. The pad is meant to loop: its loop runs from LOOP_START seconds to the end, with the join crossfaded into the file.
"""
import pathlib
import wave

import numpy as np

SR = 24000
OUT = pathlib.Path(__file__).resolve().parent.parent / "Assets" / "audition"
PAD_LOOP_START = 2.0      # seconds; keep in sync with Source/Audition.cpp
PAD_CROSSFADE = 0.5


def freq(note):
    return 440.0 * 2.0 ** ((note - 69) / 12.0)


def seconds(n):
    return np.arange(int(SR * n)) / SR


def finish(x, fade_out=0.015):
    x = x - np.mean(x)
    x = x / max(1e-9, np.max(np.abs(x))) * 0.8
    n = int(SR * fade_out)
    x[-n:] *= np.linspace(1.0, 0.0, n)
    return x


def write(name, x):
    OUT.mkdir(parents=True, exist_ok=True)
    pcm = (np.clip(x, -1.0, 1.0) * 32767.0).astype("<i2")
    with wave.open(str(OUT / name), "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(SR)
        w.writeframes(pcm.tobytes())


def bass(note):
    t = seconds(1.6)
    f = freq(note)
    x = np.zeros_like(t)
    for h in range(1, 14):
        if f * h > SR * 0.45:
            break
        # upper harmonics die away quickly: a filter closing on a saw
        x += np.sin(2 * np.pi * f * h * t) / (h ** 1.1) * np.exp(-t * (0.8 + h * 2.6))
    x += 0.6 * np.sin(2 * np.pi * f * t)
    env = 0.3 + 0.7 * np.exp(-t / 0.45)
    env *= np.minimum(1.0, t / 0.003)
    return finish(np.tanh(1.8 * x * env))


def pluck(note, seed):
    """Karplus-Strong string: a noise burst circulating in a delay line with a two-point averaging loss, tuned with an allpass."""
    rng = np.random.default_rng(seed)
    f = freq(note)
    period = SR / f
    n_delay = int(np.floor(period - 1.0))
    d = period - 0.5 - n_delay                      # leftover fractional delay, 0.5 .. 1.5 samples
    a = (1.0 - d) / (1.0 + d)
    rho = 0.1 ** (1.0 / (1.2 * f))                  # about -20 dB after 1.2 s whatever the pitch
    line = rng.uniform(-1.0, 1.0, n_delay)
    line -= np.mean(line)                           # no DC: it would never decay
    line = np.convolve(line, [0.25, 0.5, 0.25], mode="same")
    n = int(SR * 2.0)
    out = np.zeros(n)
    idx, prev_y, ap_x, ap_y = 0, 0.0, 0.0, 0.0
    for i in range(n):
        y = line[idx]
        filtered = rho * 0.5 * (y + prev_y)
        prev_y = y
        ap = a * filtered + ap_x - a * ap_y         # allpass for fine tuning
        ap_x, ap_y = filtered, ap
        line[idx] = ap
        idx = (idx + 1) % n_delay
        out[i] = y
    blocked = np.zeros(n)                           # one-pole DC blocker
    last_in = last_out = 0.0
    for i in range(n):
        last_out = out[i] - last_in + 0.995 * last_out
        last_in = out[i]
        blocked[i] = last_out
    blocked *= np.minimum(1.0, np.arange(n) / (SR * 0.001))
    return finish(blocked)


def keys(note):
    t = seconds(3.0)
    f = freq(note)
    index = 2.0 * np.exp(-t * 3.5) + 0.25
    tine = 0.35 * np.exp(-t * 9.0) * np.sin(2 * np.pi * f * 7.0 * t)
    x = np.sin(2 * np.pi * f * t + index * np.sin(2 * np.pi * f * t) + tine)
    x += 0.18 * np.sin(2 * np.pi * f * 2 * t) * np.exp(-t * 2.5)
    env = np.exp(-t / 1.7) * np.minimum(1.0, t / 0.002)
    return finish(x * env)


def bell(note):
    t = seconds(4.0)
    f = freq(note)
    x = np.zeros_like(t)
    for ratio, amp, decay in ((1.0, 1.0, 1.4), (2.76, 0.6, 1.8), (5.4, 0.35, 2.6), (8.93, 0.2, 3.4)):
        if f * ratio < SR * 0.45:
            x += amp * np.sin(2 * np.pi * f * ratio * t + 1.5 * np.exp(-t * 2.0) * np.sin(2 * np.pi * f * 3.5 * t)) * np.exp(-t * decay)
    x *= np.minimum(1.0, t / 0.001)
    return finish(x)


def pad(note):
    t = seconds(6.0)
    f = freq(note)
    x = np.zeros_like(t)
    for cents, phase in ((0, 0.0), (7, 1.1), (-7, 2.3), (13, 3.7), (-13, 5.1)):
        fc = f * 2.0 ** (cents / 1200.0)
        for h in range(1, 24):
            if fc * h > SR * 0.4:
                break
            x += np.sin(2 * np.pi * fc * h * t + phase * h) / (h ** 1.4)
    x *= 1.0 - 0.12 * np.sin(2 * np.pi * 0.35 * t)       # slow movement
    x *= np.minimum(1.0, t / 0.45)                       # soft attack
    # loop: blend the end of the file into the audio before the loop start, so playing from the end back to the start is seamless
    s = int(SR * PAD_LOOP_START)
    c = int(SR * PAD_CROSSFADE)
    e = len(x)
    w = np.linspace(0.0, 1.0, c)
    x[e - c:e] = x[e - c:e] * np.cos(w * np.pi / 2) + x[s - c:s] * np.sin(w * np.pi / 2)
    x = x - np.mean(x[s:e])
    x = x / np.max(np.abs(x)) * 0.8
    return x


def main():
    for note in (24, 36, 48):
        write(f"bass_{note}.wav", bass(note))
    for note in (48, 60, 72, 84):
        write(f"pluck_{note}.wav", pluck(note, 1000 + note))
    for note in (36, 48, 60, 72, 84):
        write(f"keys_{note}.wav", keys(note))
    for note in (36, 48, 60, 72):
        write(f"pad_{note}.wav", pad(note))
    for note in (60, 72, 84):
        write(f"bell_{note}.wav", bell(note))
    print("wrote", len(list(OUT.glob("*.wav"))), "samples,", sum(p.stat().st_size for p in OUT.glob("*.wav")) // 1024, "KiB")


if __name__ == "__main__":
    main()
