#!/usr/bin/env python3
"""Float reference for VOCODER: the law in the form the DSP computes it.

A ten-band channel vocoder after the Roland VP-330 (service notes, 21 Sept 1979: ten high-Q
band-pass filters, centres 200 Hz to 6 kHz, pre-emphasis before analysis and a de-emphasised
synthesis response, a high-consonant circuit). modules/vocoder/DESIGN.md has the reasons.

  modulator m = (L + R)/2 (MODE INT) or L (MODE EXT); carrier c = the internal 8' and 4'
    sawtooths at NOTE (INT) or R/2 (EXT)
  each band k: two Chamberlin band-pass sections, at fc x 0.91 and fc x 1.10, Q 7, the input
    scaled by q = 1/7 so each section peaks at the input's level; the same pair on m and on c
  envelope: v = (W_k / 2) |band_k(m)|; e = max(v, e (1 - ad)), ad for 10 ms: a peak
    detector, instant attack, as the VP-330's diodes; 48-bit
  out = LEVL limit(2^8 sum_k e_k band_k(c) + (CONS/32) hp6(m/2) + (DRY/128) m)
  W_k folds in the VP-330's pre-emphasis (1 - 0.9 z^-1 at fc), its de-emphasis (a 9 dB fall
    across the bands) and the pair's gain at fc (1/g^2), so the bass bands are not starved
    of level before the 24-bit filters
  hp6: the modulator at half level through three Chamberlin high-pass sections at 4 kHz
    (a 6th-order Butterworth: Q 0.518, 0.707, 1.932)
  sawtooths: polyBLEP, branch-free: corr = (1 - min(u/du, 1))^2 - (1 - min((1-u)/du, 1))^2

    python3 modules/vocoder/vocoder_ref.py      # self-check
"""
import math
import os
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from vocoder_law import (FS, CENTRES, Q_BAND, EMPH, TILT_DB, RELEASE_MS,  # noqa: F401
                         MAKEUP_SHIFT, NOTE_LO, NOTE_HI, K_BLEP, HP_FC, HP_Q, NOTE_NAMES,
                         band_coefs, note_name, note_inc, note_idt,
                         GLD, LCG_SEED, WHSP_G, pitch_target, inc_of, idt_of, gend_half, lcg, q23, s24)


def _saw(n, inc, idt, octave):
    """The DSP's sawtooth: signed 24-bit phase (a cycle is 2^24), polyBLEP, x2 for the 4' octave."""
    out = np.empty(n)
    p = 0
    for i in range(n):
        s = p << octave & 0xFFFFFF
        s = (s - (1 << 24) if s & 0x800000 else s) / (1 << 23)          # [-1, 1)
        u = s / 2 + 0.5
        it = idt / (1 << octave)
        ta = min(u * it * (1 << K_BLEP), 1.0)
        tb = min((1 - u) * it * (1 << K_BLEP), 1.0)
        out[i] = s + (1 - ta) ** 2 - (1 - tb) ** 2
        p = (p + inc) & 0xFFFFFF
    return out


def carrier_int(step, n):
    inc, idt = note_inc(step), note_idt(step)
    return 0.3 * _saw(n, inc, idt, 0) + 0.2 * _saw(n, inc, idt, 1)


def _svf_bp(x, f, q):
    low = band = 0.0
    y = np.empty_like(x)
    for i, v in enumerate(x):
        low += f * band
        band += f * (q * v - low - q * band)
        y[i] = band
    return y


def _svf_hp(x, f, q):
    low = band = 0.0
    y = np.empty_like(x)
    for i, v in enumerate(x):
        low += f * band
        h = v - low - q * band
        band += f * h
        y[i] = h
    return y


def _knob(v, nb):
    """A knob per call: a scalar, or one value per call (a p-lock or a fader move)."""
    a = np.asarray(v)
    return np.full(nb, int(a)) if a.ndim == 0 else a[:nb].astype(int)


def _blep(s, idt):
    """The DSP's polyBLEP sawtooth sample at signed phase s in [-1, 1), idt = 1/(du 2^12) as a float."""
    u = s / 2 + 0.5
    ta = min(u * idt, 2.0 ** -K_BLEP) * (1 << K_BLEP)
    tb = min((1 - u) * idt, 2.0 ** -K_BLEP) * (1 << K_BLEP)
    return s + (1 - ta) ** 2 - (1 - tb) ** 2


def carrier_02(n, note, bend, glide, whsp, frames=15):
    """The 0.2 INT carrier, per call of `frames` samples as dsp_host makes them: NOTE + BEND as
    a log pitch, glided in 48 bits, to a phase increment (bit-exact: the phase must not drift),
    then the 8' and 4' polyBLEP sawtooths; WHSP blends to the LCG's noise. Returns (c, incs)."""
    nb = -(-n // frames)
    nt, bd, gl, wh = (_knob(k, nb) for k in (note, bend, glide, whsp))
    c = np.empty(n); incs = np.empty(nb, int)
    p = 0; ps48 = None; x = LCG_SEED; G = WHSP_G
    for b in range(nb):
        P = pitch_target(nt[b] << 16, bd[b] << 16)
        if ps48 is None:
            ps48 = P << 24
        else:
            k = frames * GLD[min(gl[b], 4)]
            d = P - (ps48 >> 24)
            ps48 += 2 * d * k                              # mac: 48 bits
        inc = inc_of(ps48 >> 24); idt = idt_of(inc)
        incs[b] = inc
        i8, i4 = idt / (1 << 23), (idt >> 1) / (1 << 23)
        w = (wh[b] << 16) / (1 << 23)
        for i in range(b * frames, min(n, (b + 1) * frames)):
            s8 = s24(p) / (1 << 23)
            s4 = s24(p << 1) / (1 << 23)
            mix = 0.3 * _blep(s8, i8) + 0.2 * _blep(s4, i4)
            x = lcg(x)
            c[i] = mix + w * (G * s24(x) / (1 << 23) - mix)
            p = (p + inc) & 0xFFFFFF
    return c, incs


def _svf_bp_k(x, fk, frames):
    """A Chamberlin band-pass whose coefficient may change per call (fk: one per call)."""
    q = 1.0 / Q_BAND
    low = band = 0.0
    y = np.empty_like(x)
    for i, v in enumerate(x):
        f = fk[i // frames]
        low += f * band
        band += f * (q * v - low - q * band)
        y[i] = band
    return y


def analysis_coefs(gend_raw):
    """Per band (f1a, f2a) as the DSP writes them to its Y slot: the synthesis coefficient x A,
    floor(4 f (A/2) / 2^24), limited to the largest Q23."""
    ah = gend_half(gend_raw)
    return [tuple(min((4 * q23(f) * ah) >> 24, 0x7FFFFF) / (1 << 23) for f in (f1, f2))
            for f1, f2, _ in band_coefs()]


def vocode(L, R, mode=0, note=24, bend=64, gend=64, whsp=0, cons=48, dry=0, glide=0, frames=15):
    """0.2: MODE 0 INT, 1 EXT; knob values as the panel gives them (a scalar, or one per call of
    `frames` samples). LEVL is gone: the level is the track's AMP. Returns the mono output."""
    L = np.asarray(L, float); R = np.asarray(R, float); n = len(L)
    nb = -(-n // frames)
    m = (L + R) / 2 if mode == 0 else L
    c = carrier_02(n, note, bend, glide, whsp, frames)[0] if mode == 0 else R / 2
    q = 1.0 / Q_BAND
    ad = 1 - math.exp(-1 / (FS * RELEASE_MS * 1e-3))
    g = _knob(gend, nb)
    per_call = [analysis_coefs(v << 16) for v in g]
    acc = np.zeros(n)
    for k, (f1, f2, w) in enumerate(band_coefs()):
        a = _svf_bp_k(_svf_bp_k(m, [pc[k][0] for pc in per_call], frames), [pc[k][1] for pc in per_call], frames)
        b = _svf_bp(_svf_bp(c, f1, q), f2, q)
        e = np.empty(n); ev = 0.0
        for i, v in enumerate(w * np.abs(a)):
            ev = max(v, ev * (1 - ad))               # a peak detector: instant attack
            e[i] = ev
        acc += e * b
    hf = 2 * math.sin(math.pi * HP_FC / FS)
    hp = m / 2
    for qq in HP_Q:
        hp = _svf_hp(hp, hf, 1 / qq)
    cs, dr = _knob(cons, nb), _knob(dry, nb)
    csn = np.repeat(cs, frames)[:n] / 32.0
    drn = np.repeat(dr, frames)[:n] / 128.0
    return np.clip(acc * (1 << MAKEUP_SHIFT) + csn * hp + drn * m, -1.0, 1.0 - 2 ** -23)


if __name__ == "__main__":
    ok = True
    cf = band_coefs()
    print("f1", [round(x, 5) for x, _, _ in cf]); print("f2", [round(x, 5) for _, x, _ in cf])
    print("W/2", [round(w, 4) for _, _, w in cf]); ok &= all(0 < w < 1 for _, _, w in cf)
    print("NOTE", note_name(0), "..", note_name(60), "; C3 is step", [s for s in range(61) if note_name(s) == "C3"][0])
    ok &= all(note_idt(s) < 1 for s in range(61))
    # the sawtooth's aliasing: a C6 saw against an ideal band-limited one
    n = 1 << 15; s = _saw(n, note_inc(60), note_idt(60), 0)
    X = np.abs(np.fft.rfft(s * np.blackman(n))); fr = np.fft.rfftfreq(n, 1 / FS); f0 = note_inc(60) * FS / (1 << 24)
    harm = np.zeros_like(X, bool)
    for h in range(1, int(FS / 2 / f0) + 1):
        harm |= np.abs(fr - h * f0) < 15
    band = (fr > 100) & (fr < 7000)
    alias = 10 * math.log10(np.sum(X[band & ~harm] ** 2) / np.sum(X[band & harm] ** 2))
    print(f"C6 polyBLEP saw: aliased power 100 Hz-7 kHz {alias:.1f} dB under its harmonics"); ok &= alias < -40
    # a silent modulator gives silence; a steady vowel-like modulator gives the carrier
    t = np.arange(int(FS)) / FS
    y0 = vocode(np.zeros(len(t)), np.zeros(len(t)), cons=48)
    _, incs = carrier_02(4410, 24, 64, 0, 0)
    print("C3 at BEND centre: inc", incs[0], "=", note_inc(24), "(0.1's table)"); ok &= incs[0] == note_inc(24)
    _, incs = carrier_02(15 * 3000, 24, np.r_[[64] * 10, [127] * 2990], 2, 0)
    st = 12 * np.log2(incs / incs[0])                  # the glide runs on log pitch
    k63 = int(np.argmax(st > 0.632 * st[-1])) - 10
    print(f"GLIDE 80 ms: 63 % of a bend reached after {k63 * 15 / FS * 1000:.1f} ms"); ok &= abs(k63 * 15 / FS * 1000 - 80) < 4
    print("silence in, INT: peak", float(np.max(np.abs(y0)))); ok &= not np.any(y0)
    print("SELF-CHECK", "OK" if ok else "FAILED")
