"""Synthesize Sparky and Leo's sound effects and music as WAV files (no samples, all maths).

    python3 Scripts/make_audio.py            # writes AudioSource/*.wav
    ./build_content.sh                       # imports them into Content/Audio

Needs numpy and scipy. Everything is seeded, so re-running gives identical files.
The synth building blocks come from Cheeto 64 2's make_audio.py: voices (meows, barks,
growls) are formant synthesis, a buzzing glottal pulse through band-pass "vocal tract"
resonances; strings are Karplus-Strong; the rest is oscillators, noise and filters. The
music is lowpassed to ~9 kHz, like the N64's sample-based sound chip.
"""
import os

import numpy as np
from scipy.io import wavfile
from scipy.signal import butter, fftconvolve, sosfilt

SR = 44100
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "AudioSource")
rng = np.random.default_rng(64)


# ---------------------------------------------------------------- building blocks

def t_axis(dur):
    return np.arange(int(dur * SR)) / SR


def noise(dur):
    return rng.uniform(-1, 1, int(dur * SR))


def filt(x, kind, freq, order=2):
    return sosfilt(butter(order, freq, btype=kind, fs=SR, output="sos"), x)


def sweep_lowpass(x, f0, f1, order=2, block=256):
    """Low-pass whose cutoff glides exponentially from f0 to f1 over the signal."""
    zi = np.zeros((butter(order, f0, fs=SR, output="sos").shape[0], 2))
    out = np.empty_like(x)
    n_blocks = max(1, len(x) // block)
    for b, fc in enumerate(np.geomspace(f0, f1, n_blocks + 1)):
        s = slice(b * block, (b + 1) * block if b < n_blocks else len(x))
        if s.start >= len(x):
            break
        out[s], zi = sosfilt(butter(order, min(fc, SR * 0.45), fs=SR, output="sos"), x[s], zi=zi)
    return out


def exp_env(dur, decay, attack=0.002):
    t = t_axis(dur)
    return np.minimum(t / attack, 1.0) * np.exp(-t / decay)


def adsr(n, a, d, s, r, sustain_len):
    """Sample-count ADSR: attack a, decay d (s), sustain level s for sustain_len, release r."""
    a, d, r, sl = (int(v * SR) for v in (a, d, r, sustain_len))
    env = np.concatenate([np.linspace(0, 1, max(a, 1), endpoint=False),
                          np.linspace(1, s, max(d, 1), endpoint=False),
                          np.full(max(sl - a - d, 0), s),
                          np.linspace(s, 0, max(r, 1))])
    return np.pad(env, (0, max(0, n - len(env))))[:n]


def osc(kind, freq, dur, phase=0.0):
    """Oscillators via phase accumulation; freq may be an array (glides)."""
    n = int(dur * SR)
    f = np.broadcast_to(np.asarray(freq, dtype=float), (n,))
    ph = phase + np.cumsum(f) / SR
    if kind == "sine":
        return np.sin(2 * np.pi * ph)
    frac = ph % 1.0
    if kind == "saw":
        return 2 * frac - 1
    if kind == "square":
        return np.where(frac < 0.5, 1.0, -1.0)
    if kind == "pulse25":
        return np.where(frac < 0.25, 1.0, -1.0)
    if kind == "tri":
        return 4 * np.abs(frac - 0.5) - 1
    raise ValueError(kind)


def make_ir(dur, decay, seed, lp=6000):
    r = np.random.default_rng(seed)
    t = t_axis(dur)
    return filt(r.uniform(-1, 1, len(t)) * np.exp(-t / decay), "low", lp) * 0.08


IR_L, IR_R = make_ir(2.0, 0.45, 1), make_ir(2.0, 0.45, 2)
IR_SHORT = make_ir(0.6, 0.12, 3, lp=7000)
IR_CANYON = make_ir(2.5, 0.7, 5, lp=4000)


def reverb(x, wet=0.25, ir=None):
    """Mono in. Returns mono with the given IR, or stereo (n, 2) with the hall."""
    if ir is not None:
        return x + wet * fftconvolve(x, ir)[: len(x)]
    return np.stack([x + wet * fftconvolve(x, IR_L)[: len(x)],
                     x + wet * fftconvolve(x, IR_R)[: len(x)]], axis=1)


def pad_tail(x, seconds):
    return np.concatenate([x, np.zeros(int(seconds * SR))])


def note_hz(n):
    """MIDI note number to Hz."""
    return 440.0 * 2 ** ((n - 69) / 12)


def save(name, x, peak=0.89):
    os.makedirs(OUT, exist_ok=True)
    x = sosfilt(butter(2, 25, btype="high", fs=SR, output="sos"), np.asarray(x, dtype=float), axis=0)  # no DC
    x = x / (np.max(np.abs(x)) + 1e-9) * peak
    fade = min(len(x), int(0.01 * SR))
    x[-fade:] *= np.linspace(1, 0, fade)[:, None] if x.ndim == 2 else np.linspace(1, 0, fade)
    wavfile.write(os.path.join(OUT, name + ".wav"), SR, (x * 32767).astype(np.int16))
    print(f"  {name}.wav  {len(x) / SR:5.2f} s  {'stereo' if x.ndim == 2 else 'mono'}")


def save_loop(name, x, length_s, peak=0.85):
    """x is longer than the loop (reverb/echo tail): fold the tail onto the start."""
    n = int(length_s * SR)
    loop = x[:n].copy()
    tail = x[n:]
    loop[: len(tail)] += tail[: n]
    loop = filt_stereo(loop, "low", 9000)
    os.makedirs(OUT, exist_ok=True)
    loop = loop / (np.max(np.abs(loop)) + 1e-9) * peak
    wavfile.write(os.path.join(OUT, name + ".wav"), SR, (loop * 32767).astype(np.int16))
    print(f"  {name}.wav  {n / SR:5.2f} s  loop")


def filt_stereo(x, kind, freq):
    return sosfilt(butter(2, freq, btype=kind, fs=SR, output="sos"), x, axis=0)


def layer(*parts):
    """parts: (signal, start seconds[, gain]) -> one buffer long enough for all of them."""
    n = max(int(p[1] * SR) + len(p[0]) for p in parts)
    out = np.zeros(n)
    for p in parts:
        put(out, p[0], p[1], p[2] if len(p) > 2 else 1.0)
    return out


def put(buf, x, start_s, gain=1.0):
    s = int(start_s * SR)
    if s >= len(buf):
        return
    x = x[: len(buf) - s]
    buf[s:s + len(x)] += x * gain


# ---------------------------------------------------------------- voices (formant synthesis)

VOWELS = {  # F1, F2, F3 (Hz), bandwidths are proportional
    "a": (780, 1240, 2500), "e": (520, 1850, 2550), "i": (300, 2250, 3000),
    "o": (520, 880, 2400), "u": (330, 820, 2300), "m": (280, 1000, 2300), "y": (360, 1800, 2700),
}


def formant_filter(src, vowels, block=256):
    """vowels: list of (vowel, seconds); glides linearly between them."""
    n = len(src)
    t_points, f_points = [0.0], [VOWELS[vowels[0][0]]]
    acc = 0.0
    for v, d in vowels:
        acc += d
        t_points.append(acc)
        f_points.append(VOWELS[v])
    t_points = np.array(t_points) / acc * (n / SR)
    f_points = np.array(f_points, dtype=float)
    out = np.zeros(n)
    for k, gain in enumerate((1.0, 0.7, 0.35)):
        zi = None
        for b in range(0, n, block):
            fc = np.interp(b / SR, t_points, f_points[:, k])
            bw = 60 + fc * 0.08
            sos = butter(1, [max(50, fc - bw), min(SR * 0.45, fc + bw)], btype="band", fs=SR, output="sos")
            if zi is None:
                zi = np.zeros((sos.shape[0], 2))
            seg, zi = sosfilt(sos, src[b:b + block], zi=zi)
            out[b:b + block] += seg * gain
    return out


def voice(vowels, f0, rasp=0.0, breath=0.05):
    """f0: array of pitch points spread over the duration."""
    dur = sum(d for _, d in vowels)
    n = int(dur * SR)
    pitch = np.interp(np.arange(n), np.linspace(0, n, len(f0)), f0)
    if rasp:
        pitch *= 1 + rasp * filt(rng.normal(0, 1, n), "low", 40) * 3
    glottal = osc("saw", pitch, dur) * 0.7 + osc("square", pitch, dur) * 0.3
    glottal = filt(glottal, "low", 3500)
    src = glottal + breath * noise(dur)
    x = formant_filter(src, vowels)
    return x * adsr(n, 0.02, 0.05, 0.9, 0.06, dur - 0.06)


def huff(dur, lo=800, hi=3500):
    """An 'h' sound: breathy noise."""
    return filt(noise(dur), "band", [lo, hi]) * adsr(int(dur * SR), 0.01, 0.02, 0.7, 0.03, dur - 0.03)


def meow(variant):
    f = [(520, 780, 640, 430), (640, 900, 820, 560), (430, 600, 520, 360)][variant]
    d = [(0.06, 0.12, 0.16, 0.14), (0.05, 0.09, 0.12, 0.1), (0.08, 0.16, 0.22, 0.2)][variant]
    x = voice([("m", d[0]), ("i", d[1]), ("a", d[2]), ("u", d[3])], f, rasp=0.004, breath=0.03)
    return reverb(pad_tail(x, 0.3), 0.15, IR_SHORT)


def hey():
    x = np.concatenate([huff(0.06), voice([("e", 0.14), ("y", 0.12)], (170, 210, 150), rasp=0.01)])
    return reverb(pad_tail(x, 0.3), 0.2, IR_SHORT)


def hurt_voice(f0=150):
    x = np.concatenate([huff(0.03), voice([("a", 0.08), ("u", 0.12)], (f0, f0 * 1.15, f0 * 0.75), rasp=0.03)])
    return reverb(pad_tail(x, 0.2), 0.15, IR_SHORT)


def death_voice(f0=170):
    x = voice([("a", 0.25), ("a", 0.25), ("o", 0.25), ("u", 0.15)], (f0, f0 * 1.1, f0 * 0.8, f0 * 0.5), rasp=0.05)
    return reverb(pad_tail(x, 0.4), 0.25, IR_SHORT)


def laugh():
    """Skid Mark: "mwa-ha-ha-ha-haaa"."""
    parts = [voice([("m", 0.07), ("a", 0.2)], (120, 135, 125), rasp=0.02)]
    pitches = [140, 132, 124, 118, 104]
    for i, p in enumerate(pitches):
        parts.append(np.zeros(int(0.03 * SR)))
        parts.append(huff(0.05, 600, 2500) * 0.6)
        d = 0.14 if i < len(pitches) - 1 else 0.45
        parts.append(voice([("a", d)], (p, p * 1.05, p * 0.85), rasp=0.025))
    return reverb(pad_tail(np.concatenate(parts), 0.8), 0.45, IR_CANYON)


def player_hurt():
    return voice([("u", 0.06), ("o", 0.1)], (130, 150, 110), rasp=0.03) * 0.9


def player_death():
    x = voice([("a", 0.3), ("o", 0.3), ("u", 0.3)], (190, 170, 120, 80), rasp=0.06)
    return reverb(pad_tail(x, 0.8), 0.4, IR_CANYON)



# ---------------------------------------------------------------- guns (2054 edition)

def gunshot(dur, body_lp, crack_amt, thump_f, decay, wet, ir):
    crack = filt(noise(dur), "high", 2500) * exp_env(dur, 0.006, 0.0003) * crack_amt
    body = sweep_lowpass(noise(dur), body_lp, 300) * exp_env(dur, decay, 0.0005)
    t = t_axis(dur)
    thump = osc("sine", thump_f * (1 + 2 * np.exp(-t / 0.01)), dur) * exp_env(dur, decay * 0.8, 0.0005)
    x = np.tanh(2.5 * (crack + body + thump))  # gritty
    return reverb(pad_tail(x, 0.6), wet, ir)


def laser(f0, f1, dur, decay, kind="square", body=0.5):
    """The sci-fi 'pew': a falling tone with a noisy crack and a thump of body."""
    t = t_axis(dur)
    f = f1 + (f0 - f1) * np.exp(-t / (dur * 0.25))
    tone = (osc(kind, f, dur) * 0.6 + osc("saw", f * 0.5, dur) * 0.4) * exp_env(dur, decay, 0.0005)
    crack = filt(noise(dur), "high", 3000) * exp_env(dur, 0.004, 0.0003)
    thump = osc("sine", 90 * (1 + 2 * np.exp(-t / 0.01)), dur) * exp_env(dur, 0.04, 0.0005) * body
    return filt(np.tanh(1.8 * (tone + 0.6 * crack + thump)), "low", 9000)


def blaster():
    return reverb(pad_tail(laser(2400, 300, 0.32, 0.09), 0.5), 0.4, IR_CANYON)


def pulse_shotgun():
    x = layer((laser(900, 60, 0.6, 0.16, "saw", body=1.2), 0), (gunshot(0.5, 3000, 0.6, 55, 0.12, 0.0, IR_SHORT) * 0.6, 0))
    t = t_axis(0.6)
    wub = osc("square", 70 + 30 * np.sin(2 * np.pi * 25 * t), 0.6) * exp_env(0.6, 0.15) * 0.4
    return reverb(pad_tail(np.tanh(x[: len(wub)] + wub), 0.6), 0.5, IR_CANYON)


def smg():
    return reverb(pad_tail(laser(1800, 500, 0.14, 0.035, "pulse25", body=0.4), 0.15), 0.15, IR_SHORT)


def rail():
    """Charge whine, then a great metallic crack that rings off down the street."""
    dur = 1.6
    t = t_axis(dur)
    crack = layer((laser(5000, 200, 0.5, 0.12, "saw", body=1.5), 0))
    ring = sum(np.sin(2 * np.pi * f * t) * np.exp(-t / d) for f, d in [(880, 0.5), (1320, 0.35), (2210, 0.2), (3530, 0.12)]) * 0.18
    sizzle = filt(noise(dur), "band", [3000, 9000]) * np.exp(-t / 0.3) * 0.25
    x = np.zeros(len(t))
    put(x, crack, 0)
    x += ring + sizzle
    return reverb(pad_tail(np.tanh(1.5 * x), 0.6), 0.55, IR_CANYON)


def rocket_launch():
    dur = 0.9
    t = t_axis(dur)
    thump = osc("sine", 50 + 120 * np.exp(-t / 0.05), dur) * exp_env(dur, 0.15, 0.002)
    whoosh = sweep_lowpass(noise(dur), 800, 5000) * adsr(len(t), 0.02, 0.2, 0.5, 0.4, 0.5)
    return reverb(np.tanh(1.5 * (thump + 0.7 * whoosh)), 0.4, IR_CANYON)


def enemy_shot():
    """SkidCorp plasma rifle: lower and buzzier than Cheeto's blaster."""
    x = laser(1400, 180, 0.3, 0.07, "saw", body=0.6)
    return reverb(pad_tail(filt(x, "low", 5000), 0.4), 0.35, IR_CANYON)


def ricochet(variant):
    dur = 0.6
    t = t_axis(dur)
    f0, f1 = [(3800, 1400), (3200, 900), (4400, 1800)][variant]
    f = f1 + (f0 - f1) * np.exp(-t / 0.25)
    whine = osc("sine", f * (1 + 0.01 * np.sin(2 * np.pi * 30 * t)), dur) * exp_env(dur, 0.25, 0.004)
    tick = filt(noise(dur), "high", 3000) * exp_env(dur, 0.004)
    return reverb(pad_tail(0.5 * whine + tick, 0.4), 0.4, IR_CANYON)


def impact():
    dur = 0.18
    x = filt(noise(dur), "band", [300, 2500]) * exp_env(dur, 0.025)
    x = np.round(x * 8) / 8
    return reverb(x, 0.2, IR_SHORT)


def flesh():
    dur = 0.15
    x = filt(noise(dur), "low", 900) * exp_env(dur, 0.03) + osc("sine", 110, dur) * exp_env(dur, 0.04) * 0.6
    return x


def click(freq=2500, dur=0.03):
    return filt(noise(dur), "band", [freq * 0.7, freq * 1.4]) * exp_env(dur, 0.005, 0.0005)


def reload():
    """Cell out, cell in, a rising charge whine, ready beep."""
    x = np.zeros(int(1.0 * SR))
    put(x, click(1500, 0.06) * 1.2, 0.0)
    put(x, click(900, 0.08) * 1.4, 0.3)
    t = t_axis(0.45)
    whine = osc("sine", 400 + 1800 * (t / 0.45) ** 1.5, 0.45) * adsr(len(t), 0.02, 0.1, 0.6, 0.05, 0.4) * 0.25
    put(x, whine, 0.38)
    put(x, chime([88], 0, kind="square", dur=0.06) * 0.5, 0.85)
    return reverb(x, 0.15, IR_SHORT)


def empty():
    buzz = osc("square", 110, 0.15) * exp_env(0.15, 0.05) * 0.5
    return reverb(pad_tail(layer((buzz, 0), (click(2200, 0.04), 0)), 0.1), 0.15, IR_SHORT)


def weapon_switch():
    x = np.zeros(int(0.35 * SR))
    put(x, click(1800, 0.05), 0.0)
    t = t_axis(0.2)
    put(x, osc("sine", 600 + 900 * t / 0.2, 0.2) * exp_env(0.2, 0.08) * 0.2, 0.05)
    put(x, click(2600, 0.05) * 1.2, 0.22)
    return x


def explosion():
    dur = 2.2
    body = sweep_lowpass(noise(dur), 4500, 100, order=2) * exp_env(dur, 0.5, 0.003)
    rumble = filt(np.cumsum(noise(dur)) * 0.02, "low", 140) * exp_env(dur, 0.8, 0.02)
    t = t_axis(dur)
    thump = osc("sine", 30 + 80 * np.exp(-t / 0.08), dur) * exp_env(dur, 0.3, 0.002)
    crackle = np.zeros(len(t))
    idx = rng.integers(int(0.05 * SR), int(1.4 * SR), 60)
    crackle[idx] = rng.uniform(-1, 1, 60)
    x = body + 1.5 * rumble + 1.2 * thump + filt(crackle, "band", [1500, 6000]) * 3 * np.exp(-t / 0.7)
    return reverb(pad_tail(np.tanh(1.5 * x), 0.6), 0.45, IR_CANYON)


def rocket():
    dur = 1.0
    t = t_axis(dur)
    x = sweep_lowpass(noise(dur), 6000, 800) * adsr(len(t), 0.01, 0.2, 0.6, 0.4, 0.6)
    x = x + osc("saw", 220 * np.exp(-t / 0.6) + 80, dur) * 0.15 * exp_env(dur, 0.4)
    return reverb(x, 0.3, IR_CANYON)


def zap():
    """Skid-Bot / drone laser bolt."""
    dur = 0.3
    t = t_axis(dur)
    f = 300 + 1700 * np.exp(-t / 0.04)
    x = (osc("square", f, dur) * 0.5 + osc("saw", f * 1.5, dur) * 0.5) * exp_env(dur, 0.08)
    return reverb(filt(x, "low", 6000), 0.25, IR_SHORT)


def bot_hurt():
    dur = 0.25
    t = t_axis(dur)
    x = osc("square", 180 + 60 * np.sin(2 * np.pi * 40 * t), dur) * exp_env(dur, 0.08)
    return layer((np.round(filt(x, "low", 3000) * 6) / 6, 0), (click(5000, 0.05), 0))


def bot_death():
    dur = 1.2
    t = t_axis(dur)
    down = osc("saw", 600 * np.exp(-t / 0.35) + 30, dur) * exp_env(dur, 0.5) * 0.5
    return layer((down, 0), (explosion(), 0, 0.6))


def drone_buzz():
    """Rotors whining past."""
    dur = 1.2
    t = t_axis(dur)
    f = 140 * (1 + 0.04 * np.sin(2 * np.pi * 1.3 * t))
    x = osc("saw", f, dur) * (0.6 + 0.4 * np.sin(2 * np.pi * 37 * t)) + 0.3 * filt(noise(dur), "band", [800, 3000])
    return filt(x, "low", 3000) * adsr(len(t), 0.25, 0.1, 0.8, 0.5, 0.7)


def moose_bellow():
    """A moose call through a busted speaker: a long nasal 'mwaaaaaoh', ring-modulated."""
    x = voice([("m", 0.12), ("a", 0.45), ("a", 0.35), ("o", 0.3), ("u", 0.2)], (95, 120, 128, 110, 70), rasp=0.03, breath=0.08)
    t = np.arange(len(x)) / SR
    robot = x * (0.65 + 0.35 * np.sin(2 * np.pi * 55 * t))
    return reverb(pad_tail(np.tanh(2.0 * robot), 0.8), 0.45, IR_CANYON)


def stomp():
    dur = 0.4
    t = t_axis(dur)
    thud = osc("sine", 45 + 60 * np.exp(-t / 0.03), dur) * exp_env(dur, 0.12, 0.002)
    clank = sum(np.sin(2 * np.pi * f * t) * np.exp(-t / 0.05) for f in (620, 1390, 2380)) * 0.15
    return reverb(layer((thud + clank, 0), (click(400, 0.05), 0)), 0.3, IR_SHORT)


def jetpack():
    dur = 2.0
    t = t_axis(dur)
    roar = sweep_lowpass(noise(dur), 400, 3500) * adsr(len(t), 0.15, 0.3, 0.7, 0.8, 1.2)
    hum = osc("saw", 70 + 40 * t, dur) * 0.2 * adsr(len(t), 0.2, 0.2, 0.7, 0.6, 1.4)
    return reverb(np.tanh(1.4 * (roar + hum)), 0.35, IR_CANYON)


def teleport():
    """Beam out: a shimmering rising arpeggio over a filter sweep."""
    dur = 1.4
    t = t_axis(dur)
    sweep = sweep_lowpass(noise(dur), 300, 9000) * adsr(len(t), 0.1, 0.3, 0.5, 0.6, 0.8) * 0.4
    x = np.zeros(len(t))
    put(x, sweep, 0)
    for i, m in enumerate([72, 76, 79, 84, 88, 91, 96]):
        put(x, osc("sine", note_hz(m), 0.3) * exp_env(0.3, 0.12) * 0.35, i * 0.09)
    return reverb(x, 0.45, IR_CANYON)


# ---------------------------------------------------------------- world

def footstep(variant):
    """A crunchy step (snow, slush, grating: it works for all of them)."""
    dur = 0.12
    lo, hi = [(300, 2400), (350, 2800)][variant]
    x = filt(noise(dur), "band", [lo, hi]) * exp_env(dur, 0.025, 0.004)
    x += osc("sine", 80, dur) * exp_env(dur, 0.02) * 0.5
    return x


def jump():
    return layer((filt(noise(0.15), "band", [300, 1500]) * exp_env(0.15, 0.03), 0, 0.6), (footstep(0), 0, 0.6))


def land():
    return layer((footstep(1), 0, 1.3), (footstep(0), 0.03))


def door():
    """A sci-fi door: pneumatic hiss and a servo whine."""
    dur = 0.8
    t = t_axis(dur)
    hiss = filt(noise(dur), "band", [1500, 7000]) * adsr(len(t), 0.01, 0.15, 0.4, 0.3, 0.4) * 0.6
    servo = osc("saw", 180 + 260 * np.minimum(t / 0.4, 1), dur) * adsr(len(t), 0.02, 0.1, 0.6, 0.2, 0.55) * 0.25
    x = hiss + filt(servo, "low", 2500)
    put(x, click(700, 0.08) * 1.5, 0.62)
    return reverb(x, 0.25, IR_SHORT)


def cage_open():
    """The stasis field dropping: a falling buzz and a soft thunk."""
    dur = 0.7
    t = t_axis(dur)
    buzz = osc("square", 300 * np.exp(-t / 0.25) + 60, dur) * exp_env(dur, 0.25) * 0.4
    x = filt(buzz, "low", 3500)
    put(x, click(1200, 0.06), 0.0)
    return reverb(x, 0.25, IR_SHORT)


def purr():
    """Purr power: a deep rumble that throbs about 25 times a second."""
    dur = 1.6
    t = t_axis(dur)
    rate = 24 + 2 * np.sin(2 * np.pi * 0.6 * t)
    am = 0.5 + 0.5 * np.sin(2 * np.pi * np.cumsum(rate) / SR) ** 2
    x = (filt(noise(dur), "low", 400) * 2 + osc("tri", 48, dur) * 0.4) * am
    x = x * adsr(len(t), 0.15, 0.2, 0.9, 0.4, 1.2)
    sparkle = chime([84, 88, 91], 0.08, kind="sine", dur=0.2) * 0.25
    return layer((x, 0), (sparkle, 0.05))


def chime(notes, step=0.07, kind="square", dur=0.25, lp=5000):
    total = step * len(notes) + dur + 0.3
    x = np.zeros(int(total * SR))
    for i, m in enumerate(notes):
        tone = osc(kind, note_hz(m), dur) * 0.6 + osc("sine", note_hz(m) * 2, dur) * 0.3
        put(x, tone * exp_env(dur, dur * 0.4), i * step)
    return reverb(filt(x, "low", lp), 0.3, IR_SHORT)


def pickup_health():
    return chime([72, 76, 79, 84], 0.06)


def pickup_ammo():
    x = np.zeros(int(0.4 * SR))
    put(x, click(1500, 0.06) * 1.2, 0)
    put(x, click(2500, 0.05), 0.1)
    put(x, chime([67, 74], 0.05, dur=0.12) * 0.5, 0.12)
    return x


def pickup_weapon():
    return layer((weapon_switch(), 0), (chime([60, 67, 72, 79], 0.05), 0.1, 0.6))


def objective():
    return chime([67, 71, 74, 79, 83], 0.08, kind="tri", dur=0.4)


def menu_move():
    return chime([84], 0.0, kind="pulse25", dur=0.06)


def menu_select():
    return chime([76, 88], 0.05, kind="pulse25", dur=0.1)


def pause():
    return chime([79, 72], 0.08, kind="tri", dur=0.15)


def talk():
    """The N64 text blip, one per letter group."""
    return chime([86], 0.0, kind="square", dur=0.035, lp=4000) * 0.7


def heartbeat():
    x = np.zeros(int(0.8 * SR))
    for t0, a in ((0, 1.0), (0.18, 0.7)):
        put(x, osc("sine", 55, 0.12) * exp_env(0.12, 0.04) * a, t0)
    return x


# ---------------------------------------------------------------- instruments

def pluck(freq, dur, decay=0.996, bright=0.6):
    """Karplus-Strong plucked string, vectorised one period at a time."""
    n = int(dur * SR)
    p = max(2, int(round(SR / freq)))
    first = rng.uniform(-1, 1, p)
    first = bright * first + (1 - bright) * np.convolve(first, np.ones(4) / 4, "same")
    blocks = [first]
    total = p
    prev_last = 0.0
    while total < n:
        b = blocks[-1]
        shifted = np.concatenate([[prev_last], b[:-1]])
        prev_last = b[-1]
        blocks.append(decay * 0.5 * (b + shifted))
        total += p
    return np.concatenate(blocks)[:n]


def guitar(m, dur, vol=1.0):
    x = pluck(note_hz(m), dur + 0.4, 0.9965) + 0.4 * pluck(note_hz(m) * 1.003, dur + 0.4, 0.995)
    return filt(x, "low", 5000) * adsr(len(x), 0.001, 0.05, 0.8, 0.3, dur) * vol


def whistle(m, dur):
    t = t_axis(dur)
    vib = 1 + 0.008 * np.sin(2 * np.pi * 6 * t) * np.minimum(t / 0.25, 1)
    glide = 1 - 0.03 * np.exp(-t / 0.04)
    x = osc("sine", note_hz(m) * vib * glide, dur) + 0.05 * filt(noise(dur), "band", [note_hz(m) * 0.8, note_hz(m) * 1.3])
    return x * adsr(len(t), 0.03, 0.1, 0.85, 0.08, dur - 0.08)


def trumpet(m, dur, bright=3500):
    t = t_axis(dur)
    vib = 1 + 0.006 * np.sin(2 * np.pi * 5.5 * t) * np.minimum(t / 0.3, 1)
    x = osc("saw", note_hz(m) * vib, dur) + 0.5 * osc("saw", note_hz(m) * vib * 1.004, dur)
    env = adsr(len(t), 0.04, 0.1, 0.8, 0.08, dur - 0.08)
    x = sweep_lowpass(x, 900, bright) if dur > 0.15 else filt(x, "low", bright)
    return x * env


def bass(m, dur, kind="tri"):
    x = osc(kind, note_hz(m), dur) + 0.3 * osc("sine", note_hz(m) * 0.5, dur)
    return filt(x, "low", 1200) * adsr(int(dur * SR), 0.005, 0.1, 0.6, 0.05, dur - 0.05)


def kick():
    t = t_axis(0.3)
    return osc("sine", 45 + 90 * np.exp(-t / 0.03), 0.3) * exp_env(0.3, 0.12, 0.001)


def snare():
    d = 0.22
    return filt(noise(d), "band", [1200, 7000]) * exp_env(d, 0.06) + osc("tri", 190, d) * exp_env(d, 0.04) * 0.5


def hat():
    return filt(noise(0.05), "high", 7000) * exp_env(0.05, 0.012)


def woodblock(m=79):
    d = 0.12
    return (osc("sine", note_hz(m), d) + 0.4 * osc("sine", note_hz(m) * 2.7, d)) * exp_env(d, 0.025, 0.0005)


def tambourine():
    d = 0.15
    return filt(noise(d), "high", 6000) * exp_env(d, 0.05) * (1 + 0.5 * np.sin(2 * np.pi * 50 * t_axis(d)))


def mix_stereo(parts):
    """parts: list of (mono, pan -1..1, gain)."""
    n = max(len(p) for p, _, _ in parts)
    out = np.zeros((n, 2))
    for x, pan, g in parts:
        out[: len(x), 0] += x * g * np.sqrt((1 - pan) / 2)
        out[: len(x), 1] += x * g * np.sqrt((1 + pan) / 2)
    return out


def with_hall(stereo, wet):
    mono = stereo.mean(axis=1)
    hall = reverb(mono, 1.0) - np.stack([mono, mono], axis=1)
    return stereo + wet * hall



# ---------------------------------------------------------------- 2054 instruments

def synth_lead(m, dur, bright=3200, vib=0.007):
    t = t_axis(dur)
    v = 1 + vib * np.sin(2 * np.pi * 5.5 * t) * np.minimum(t / 0.25, 1)
    f = note_hz(m) * v
    x = osc("saw", f, dur) + osc("saw", f * 1.006, dur) * 0.7 + osc("square", f * 0.5, dur) * 0.25
    x = sweep_lowpass(x, 700, bright) if dur > 0.15 else filt(x, "low", bright)
    return x * adsr(len(t), 0.02, 0.15, 0.75, 0.1, dur - 0.1)


def pad(notes, dur, lp=1600):
    t = t_axis(dur)
    x = np.zeros(len(t))
    for i, m in enumerate(notes):
        for d in (1.0, 1.004, 0.997):
            x += osc("saw", note_hz(m) * d, dur, phase=0.13 * i + d)
    x = filt(x / (3 * len(notes)), "low", lp)
    return x * adsr(len(t), 0.35, 0.3, 0.8, 0.5, dur - 0.4)


def blip(m, dur=0.12, kind="pulse25", lp=4000):
    t = t_axis(dur)
    return filt(osc(kind, note_hz(m), dur), "low", lp) * exp_env(dur, dur * 0.35, 0.002)


def synth_bass(m, dur, lp=900):
    t = t_axis(dur)
    x = osc("saw", note_hz(m), dur) + osc("square", note_hz(m) * 0.5, dur) * 0.6
    return sweep_lowpass(x, lp * 2.5, lp * 0.6) * adsr(len(t), 0.004, 0.08, 0.7, 0.04, dur - 0.04)


def fiddle(m, dur):
    """A bowed fiddle, near enough: saw with delayed vibrato through body resonances."""
    t = t_axis(dur)
    vib = 1 + 0.012 * np.sin(2 * np.pi * 6.2 * t) * np.clip((t - 0.08) / 0.15, 0, 1)
    x = osc("saw", note_hz(m) * vib, dur)
    body = filt(x, "band", [450, 3800]) + 0.5 * filt(x, "band", [2500, 3300])
    bow = 0.04 * filt(noise(dur), "band", [2000, 6000])
    return (body + bow) * adsr(len(t), 0.03, 0.08, 0.85, 0.05, dur - 0.05)


def horn(m, dur):
    return trumpet(m, dur, bright=1800)


def bell(m, dur=1.2, index=3.0):
    """FM bell."""
    t = t_axis(dur)
    f = note_hz(m)
    mod = index * np.exp(-t / 0.4) * np.sin(2 * np.pi * f * 3.5 * t)
    return np.sin(2 * np.pi * f * t + mod) * exp_env(dur, 0.45, 0.003)


def tom(f=110):
    d = 0.35
    t = t_axis(d)
    return osc("sine", f * (1 + 0.6 * np.exp(-t / 0.04)), d) * exp_env(d, 0.14, 0.001) + filt(noise(d), "low", 1500) * exp_env(d, 0.02) * 0.3


def anvil():
    d = 0.5
    t = t_axis(d)
    x = sum(np.sin(2 * np.pi * f * t) * np.exp(-t / dd) for f, dd in [(523, 0.3), (1187, 0.2), (1843, 0.12), (2910, 0.07)])
    return x * 0.3


def steam():
    d = 0.5
    return filt(noise(d), "band", [2500, 9000]) * adsr(int(d * SR), 0.02, 0.1, 0.5, 0.25, 0.3)


def clap():
    d = 0.2
    x = np.zeros(int(d * SR))
    for k in range(3):
        put(x, filt(noise(0.02), "band", [1000, 5000]) * exp_env(0.02, 0.006), k * 0.011)
    put(x, filt(noise(0.15), "band", [1000, 4000]) * exp_env(0.15, 0.05), 0.03)
    return x


def gated_snare():
    """The 80s snare: big, then cut off."""
    d = 0.3
    x = snare()[: int(d * SR)]
    x = np.pad(x, (0, max(0, int(d * SR) - len(x))))
    x = x + 0.6 * reverb(x, 1.0, IR_SHORT)[: len(x)]
    gate = np.ones(len(x))
    gate[int(0.18 * SR):] = np.linspace(1, 0, len(x) - int(0.18 * SR))
    return x * gate


def dist_guitar(m, dur, vol=1.0):
    x = pluck(note_hz(m), dur + 0.2, 0.998, 0.8) + pluck(note_hz(m + 7), dur + 0.2, 0.998, 0.8) * 0.6  # power chord
    x = np.tanh(6 * filt(x, "low", 4000))
    return filt(x, "low", 3500) * adsr(len(x), 0.002, 0.05, 0.8, 0.06, dur) * vol * 0.5


# ---------------------------------------------------------------- music

CHORDS = {
    "Am": [57, 60, 64], "F": [53, 57, 60], "C": [60, 64, 67], "G": [55, 59, 62], "Dm": [62, 65, 69], "E": [56, 59, 64],
    "Bb": [58, 62, 65], "A": [57, 61, 64], "Em": [52, 55, 59], "Cmaj7": [48, 52, 55, 59], "B": [54, 59, 63], "D": [62, 66, 69],
}
ROOT = {"Am": 45, "F": 41, "C": 48, "G": 43, "Dm": 38, "E": 40, "Bb": 46, "A": 45, "Em": 40, "Cmaj7": 36, "B": 47, "D": 38}


def organ(m, dur, vib=True):
    """A drawbar organ: fundamental, octave and twelfth, with a little Leslie wobble."""
    t = t_axis(dur)
    w = 1 + (0.004 * np.sin(2 * np.pi * 6.5 * t) if vib else 0)
    f = note_hz(m) * w
    x = osc("sine", f, dur) + 0.6 * osc("sine", f * 2, dur) + 0.35 * osc("sine", f * 3, dur) + 0.2 * osc("sine", f * 4, dur)
    return x * adsr(len(t), 0.01, 0.05, 0.9, 0.06, dur - 0.06) * (0.85 + 0.15 * np.sin(2 * np.pi * 5.8 * t))


def epiano(m, dur):
    """A soft electric piano: a gentle FM bell with a quick decay into a warm tail."""
    t = t_axis(dur)
    f = note_hz(m)
    mod = 1.2 * np.exp(-t / 0.15) * np.sin(2 * np.pi * f * t)
    return np.sin(2 * np.pi * f * t + mod) * exp_env(dur, 0.9, 0.004) * adsr(len(t), 0.004, 0.0, 1.0, 0.1, dur - 0.1)


def crowd(dur, swell=0.0):
    t = t_axis(dur)
    x = filt(noise(dur), "band", [300, 2500]) * (0.6 + 0.2 * np.sin(2 * np.pi * 0.3 * t))
    return x * (1 + swell * np.minimum(t / max(dur, 0.01), 1))


def fanfare(notes, beat, name, wet=0.5):
    """notes: (beat, beats, midi). Synth brass in fourths over an arpeggio."""
    total = max(b + l for b, l, _ in notes) * beat + 2.0
    n = int(total * SR)
    br, ar = np.zeros(n), np.zeros(n)
    for b, l, m in notes:
        put(br, synth_lead(m, l * beat * 0.9, bright=4000), b * beat)
        put(br, synth_lead(m - 5, l * beat * 0.9, bright=2500), b * beat, 0.5)
        put(br, trumpet(m - 12, l * beat * 0.9, bright=2500), b * beat, 0.4)
        for k in range(int(l * 4)):
            put(ar, blip(m + [0, 4, 7, 12][k % 4], beat * 0.25), b * beat + k * beat / 4, 0.4)
    mix = mix_stereo([(br, 0.1, 0.45), (ar, -0.3, 0.3)])
    save(name, with_hall(mix, wet))


def death_sting():
    beat = 0.3
    total = 3.5
    n = int(total * SR)
    tp = np.zeros(n)
    for i, m in enumerate([64, 63, 62]):
        put(tp, synth_lead(m, beat * 0.9, bright=1500), i * beat)
    t = t_axis(1.6)
    wah = osc("saw", note_hz(61) * (1 - 0.06 * t) * (1 + 0.01 * np.sin(2 * np.pi * 6 * t)), 1.6)
    wah = sweep_lowpass(wah, 2000, 300) * adsr(len(t), 0.05, 0.2, 0.8, 0.5, 1.2)
    put(tp, wah, 3 * beat)
    save("Death", with_hall(mix_stereo([(tp, 0, 0.6)]), 0.5))



# ================================================================ Sparky and Leo: effects

def whoosh(dur=0.22, f0=600, f1=5000):
    x = sweep_lowpass(noise(dur), f0, f1)
    return filt(x, "high", 300) * adsr(int(dur * SR), dur * 0.3, dur * 0.2, 0.6, dur * 0.4, dur * 0.5)


def swipe():
    return layer((whoosh(0.2, 900, 7000), 0), (click(3000, 0.02), 0, 0.3))


def pounce():
    return layer((whoosh(0.35, 500, 5000), 0), (meow(1) * 0.45, 0.0))


def double_jump():
    return layer((whoosh(0.3, 1500, 400) * 0.8, 0), (chime([72, 79, 84], 0.035, kind="tri", dur=0.1) * 0.35, 0.02))


def ground_pound():
    dur = 0.6
    t = t_axis(dur)
    thump = osc("sine", 40 + 80 * np.exp(-t / 0.04), dur) * exp_env(dur, 0.18, 0.001)
    crunch = filt(noise(dur), "low", 1800) * exp_env(dur, 0.08, 0.001) * 0.8
    return reverb(layer((whoosh(0.15, 3000, 600) * 0.4, 0), (thump + crunch, 0.12)), 0.3, IR_SHORT)


def hit():
    dur = 0.25
    t = t_axis(dur)
    body = osc("sine", 110 + 160 * np.exp(-t / 0.015), dur) * exp_env(dur, 0.06, 0.0005)
    smack = filt(noise(dur), "band", [800, 5000]) * exp_env(dur, 0.02, 0.0003)
    return reverb(body + smack * 0.9, 0.2, IR_SHORT)


def poof():
    dur = 0.6
    t = t_axis(dur)
    x = sweep_lowpass(noise(dur), 5000, 400) * adsr(len(t), 0.01, 0.1, 0.5, 0.3, 0.3)
    blip = osc("tri", 900 * np.exp(-t / 0.15) + 200, dur) * exp_env(dur, 0.12) * 0.3
    return reverb(x + blip, 0.35, IR_SHORT)


def cat_hurt():
    x = voice([("i", 0.05), ("a", 0.1), ("u", 0.08)], (900, 1150, 800, 600), rasp=0.01, breath=0.08)
    return reverb(pad_tail(x, 0.2), 0.15, IR_SHORT)


def hiss():
    dur = 0.6
    x = filt(noise(dur), "band", [2500, 9000]) * adsr(int(dur * SR), 0.03, 0.1, 0.7, 0.2, 0.4)
    return x * (1 + 0.2 * np.sin(2 * np.pi * 18 * t_axis(dur)))


def catnip_blip():
    return chime([84, 91], 0.04, kind="sine", dur=0.12)


def pickup_big():
    return chime([72, 76, 79, 84, 88], 0.05, kind="tri", dur=0.28)


def styro_squeak():
    """Styrofoam being rubbed: squeaky."""
    dur = 0.45
    t = t_axis(dur)
    f = 1300 + 400 * np.sin(2 * np.pi * 7 * t) + 300 * t
    x = osc("saw", f, dur) * (0.6 + 0.4 * (np.sin(2 * np.pi * 21 * t) > 0))
    x = filt(x, "band", [900, 4000]) * adsr(len(t), 0.02, 0.05, 0.8, 0.1, dur - 0.1)
    return layer((x * 0.6, 0), (chime([79, 84], 0.05, kind="sine", dur=0.15) * 0.5, 0.25))


def velcro_rip():
    dur = 0.4
    t = t_axis(dur)
    tear = (rng.random(len(t)) < 0.03 + 0.05 * np.exp(-t / 0.15)).astype(float)
    x = filt(np.convolve(tear, np.ones(40) / 4, "same") + noise(dur) * 0.3, "band", [700, 6000])
    return x * exp_env(dur, 0.15, 0.005)


def checkpoint_chime():
    return chime([67, 72, 76, 79, 84], 0.06, kind="tri", dur=0.35)


def bark():
    x = np.concatenate([huff(0.02, 500, 2000), voice([("a", 0.06), ("o", 0.07)], (240, 330, 200), rasp=0.06, breath=0.1)])
    x = np.concatenate([x, np.zeros(int(0.08 * SR)), x * 0.8])
    return reverb(pad_tail(x, 0.3), 0.2, IR_SHORT)


def growl():
    x = voice([("o", 0.25), ("u", 0.35)], (85, 95, 80, 70), rasp=0.12, breath=0.2)
    return reverb(pad_tail(x, 0.3), 0.2, IR_SHORT)


def laser_warn():
    x = np.zeros(int(1.4 * SR))
    for i, t0 in enumerate([0.0, 0.4, 0.7, 0.9, 1.05, 1.17, 1.27]):
        put(x, filt(osc("square", 1400 + i * 60, 0.06), "low", 4000) * exp_env(0.06, 0.04), t0, 0.5)
    return x


def laser_blast():
    return layer((laser(2600, 150, 0.7, 0.25, "saw", 0.7), 0), (explosion() * 0.6, 0.05))


def alarm():
    dur = 1.8
    t = t_axis(dur)
    f = np.where((t % 0.9) < 0.45, 700, 950)
    x = filt(osc("square", f, dur), "low", 2500) * 0.5
    return reverb(x * adsr(len(t), 0.01, 0.0, 1.0, 0.1, dur - 0.1), 0.3, IR_SHORT)


def weld():
    dur = 0.3
    t = t_axis(dur)
    crackle = (rng.random(len(t)) < 0.02).astype(float)
    x = filt(np.convolve(crackle, np.ones(20) / 3, "same"), "high", 1500) + filt(noise(dur), "band", [3000, 9000]) * 0.3
    return x * adsr(len(t), 0.005, 0.05, 0.8, 0.05, dur - 0.05)


def hyperspace():
    dur = 3.0
    t = t_axis(dur)
    f = 60 * np.exp(t * 1.2)
    x = filt(osc("saw", f, dur) + osc("saw", f * 1.5, dur) * 0.5, "low", 3000) * np.minimum(t / 2.2, 1) ** 2
    x += sweep_lowpass(noise(dur), 300, 9000) * np.minimum(t / 2.4, 1) ** 3 * 0.8
    x *= np.where(t < 2.4, 1.0, np.exp(-(t - 2.4) / 0.1))
    boom = explosion()
    return reverb(layer((x * 0.7, 0), (boom, 2.35)), 0.4, IR_CANYON)


def cheer():
    dur = 2.6
    x = crowd(dur, 1.0) * adsr(int(dur * SR), 0.3, 0.3, 0.8, 1.0, 1.4)
    for i in range(10):
        put(x, clap() * 0.5, 0.2 + i * 0.17 + rng.uniform(-0.03, 0.03))
    put(x, whistle(91, 0.4) * 0.3, 0.5)
    put(x, whistle(88, 0.3) * 0.25, 1.1)
    return reverb(x, 0.4, IR_CANYON)


def boing():
    dur = 0.5
    t = t_axis(dur)
    f = 180 + 420 * (1 - np.exp(-t / 0.06)) + 40 * np.sin(2 * np.pi * 16 * t) * np.exp(-t / 0.2)
    return osc("sine", f, dur) * exp_env(dur, 0.18, 0.002) * 0.8 + osc("tri", f * 2, dur) * exp_env(dur, 0.08) * 0.2


def splash():
    dur = 0.9
    t = t_axis(dur)
    x = sweep_lowpass(noise(dur), 6000, 500) * exp_env(dur, 0.25, 0.005)
    for i in range(8):
        f = rng.uniform(500, 1400)
        put(x, osc("sine", f * (1 + 0.6 * t_axis(0.06) / 0.06), 0.06) * exp_env(0.06, 0.02) * 0.3, 0.1 + i * 0.07)
    return reverb(x, 0.3, IR_SHORT)


def clang():
    return reverb(layer((anvil(), 0), (filt(noise(0.2), "high", 3000) * exp_env(0.2, 0.01), 0, 0.5)), 0.4, IR_SHORT)


def gun():
    return laser(1800, 500, 0.18, 0.05, "square", 0.3)


def one_up():
    return chime([76, 79, 88, 84, 86, 91], 0.07, kind="square", dur=0.15)


# ================================================================ instruments

def marimba(m, dur=0.4):
    t = t_axis(dur)
    f = note_hz(m)
    return (np.sin(2 * np.pi * f * t) + 0.35 * np.sin(2 * np.pi * f * 4 * t) * np.exp(-t / 0.03)) * exp_env(dur, 0.18, 0.002)


def gong():
    d = 3.0
    t = t_axis(d)
    x = sum(np.sin(2 * np.pi * f * t * (1 + 0.002 * np.sin(2 * np.pi * 0.7 * t))) * np.exp(-t / dd) * a
            for f, dd, a in [(110, 1.6, 1.0), (163, 1.2, 0.6), (231, 0.9, 0.5), (347, 0.6, 0.3), (520, 0.4, 0.2)])
    return x * np.minimum(t / 0.01, 1) * 0.4


def taiko():
    return layer((tom(70), 0, 1.2), (filt(noise(0.2), "low", 600) * exp_env(0.2, 0.03), 0, 0.4))


def accordion(notes, dur):
    t = t_axis(dur)
    x = np.zeros(len(t))
    for m in notes:
        for d in (1.0, 1.006):
            x += osc("pulse25", note_hz(m) * d, dur) * 0.5 + osc("saw", note_hz(m) * d * 2, dur) * 0.15
    x = filt(x / len(notes), "low", 2800)
    return x * adsr(len(t), 0.02, 0.05, 0.85, 0.04, dur - 0.04)


# ================================================================ music

def title_theme():
    """Space Cats Unite: dreamy and curious. C major, 92 bpm. Bells, pads, a soft e-piano tune."""
    bpm = 92
    beat = 60 / bpm
    bar = 4 * beat
    prog = ["C", "Am", "F", "G", "C", "Am", "Dm", "G", "F", "G", "Em", "Am", "F", "G", "C", "C"]
    length = len(prog) * bar
    n = int((length + 4) * SR)
    pd, bl, bs, dr, ld = (np.zeros(n) for _ in range(5))
    K, H = kick(), hat()
    for b, ch in enumerate(prog):
        t0 = b * bar
        put(pd, pad(CHORDS[ch] + [CHORDS[ch][0] + 12], bar * 1.05, lp=1300), t0)
        tones = CHORDS[ch] + [CHORDS[ch][0] + 12]
        for q in range(8):
            put(bl, bell(tones[[0, 1, 2, 3, 2, 1, 2, 3][q]] + 12, 0.8, 1.5), t0 + q * beat / 2, 0.35)
        put(bs, bass(ROOT[ch], beat * 1.8), t0)
        put(bs, bass(ROOT[ch] + 7, beat * 1.8), t0 + 2 * beat, 0.8)
        if b >= 4:
            put(dr, K * 0.7, t0)
            put(dr, K * 0.5, t0 + 2.5 * beat)
            for q in range(8):
                put(dr, H * (0.3 if q % 2 else 0.18), t0 + q * beat / 2)
            put(dr, clap() * 0.35, t0 + beat)
            put(dr, clap() * 0.35, t0 + 3 * beat)
    tune = [(4, 0, 1, 76), (4, 1, 1, 79), (4, 2, 2, 84), (5, 0, 1, 81), (5, 1, 1, 79), (5, 2, 2, 76),
            (6, 0, 1, 77), (6, 1, 1, 81), (6, 2, 2, 86), (7, 0, 3, 83), (7, 3, 1, 79),
            (8, 0, 1, 81), (8, 1, 1, 77), (8, 2, 2, 72), (9, 0, 1, 79), (9, 1, 1, 74), (9, 2, 2, 71),
            (10, 0, 1, 76), (10, 1, 1, 79), (10, 2, 2, 83), (11, 0, 4, 81),
            (12, 0, 1, 77), (12, 1, 1, 81), (12, 2, 2, 84), (13, 0, 1, 86), (13, 1, 1, 83), (13, 2, 2, 79), (14, 0, 4, 84)]
    for bb, bt, bl_, m in tune:
        put(ld, epiano(m, bl_ * beat * 0.95), bb * bar + bt * beat)
    mix = mix_stereo([(pd, 0, 0.35), (bl, -0.35, 0.25), (bs, 0, 0.4), (dr, 0.1, 0.4), (ld, 0.2, 0.45)])
    save_loop("MusicTitle", with_hall(mix, 0.6), length)


def cranberry_theme():
    """Cranberry Plasma: a bouncy, slightly odd island tune. F major, 118 bpm. Marimbas and whistles."""
    bpm = 118
    beat = 60 / bpm
    bar = 4 * beat
    prog = ["F", "F", "Bb", "C", "F", "Dm", "Bb", "C", "Dm", "Bb", "F", "C", "Bb", "C", "F", "F"]
    CH = dict(CHORDS, F=[53, 57, 60], Bb=[58, 62, 65])
    length = len(prog) * bar
    n = int((length + 4) * SR)
    mr, bs, dr, ld, ch = (np.zeros(n) for _ in range(5))
    for b, c in enumerate(prog):
        t0 = b * bar
        tones = CH[c]
        # marimba: a skippy offbeat pattern
        for q, k in enumerate([0, -1, 1, 2, -1, 1, 0, 2]):
            if k >= 0:
                put(mr, marimba(tones[k] + 12, 0.3), t0 + q * beat / 2, 0.55 if q % 2 else 0.4)
        # tuba-ish oom-pah bass
        put(bs, bass(ROOT[c] if c in ROOT else tones[0] - 12, beat * 0.8, "tri"), t0)
        put(bs, bass((ROOT[c] if c in ROOT else tones[0] - 12) + 7, beat * 0.8, "tri"), t0 + 2 * beat)
        put(ch, accordion([m + 12 for m in tones], beat * 0.35), t0 + beat, 0.5)
        put(ch, accordion([m + 12 for m in tones], beat * 0.35), t0 + 3 * beat, 0.5)
        put(dr, kick() * 0.6, t0)
        put(dr, kick() * 0.4, t0 + 2 * beat)
        for q in range(8):
            put(dr, (tambourine() if q % 2 else woodblock(84)) * 0.3, t0 + q * beat / 2)
        if b % 4 == 3:
            put(dr, woodblock(91) * 0.5, t0 + 3.5 * beat)
    tune = [(0, 0, 0.5, 72), (0, 0.5, 0.5, 74), (0, 1, 1, 77), (0, 2, 1, 81), (0, 3, 1, 79),
            (1, 0, 2, 77), (1, 2, 0.5, 74), (1, 2.5, 0.5, 72), (1, 3, 1, 74),
            (2, 0, 1, 77), (2, 1, 1, 82), (2, 2, 1, 81), (2, 3, 1, 79), (3, 0, 3, 79), (3, 3, 1, 72),
            (4, 0, 0.5, 72), (4, 0.5, 0.5, 74), (4, 1, 1, 77), (4, 2, 1, 81), (4, 3, 1, 84),
            (5, 0, 2, 81), (5, 2, 1, 77), (5, 3, 1, 74), (6, 0, 1, 74), (6, 1, 1, 77), (6, 2, 1, 82), (6, 3, 1, 81),
            (7, 0, 4, 79),
            (8, 0, 1, 81), (8, 1, 1, 77), (8, 2, 1, 74), (8, 3, 1, 77), (9, 0, 2, 82), (9, 2, 2, 77),
            (10, 0, 1, 81), (10, 1, 1, 84), (10, 2, 2, 81), (11, 0, 2, 79), (11, 2, 2, 76),
            (12, 0, 1, 77), (12, 1, 1, 82), (12, 2, 1, 86), (12, 3, 1, 84), (13, 0, 2, 84), (13, 2, 2, 79),
            (14, 0, 4, 77)]
    for bb, bt, bl, m in tune:
        put(ld, whistle(m, bl * beat * 0.92), bb * bar + bt * beat)
    mix = mix_stereo([(mr, -0.3, 0.4), (bs, 0, 0.45), (dr, 0.1, 0.4), (ld, 0.2, 0.32), (ch, 0.3, 0.2)])
    save_loop("MusicCranberry", with_hall(mix, 0.45), length)


def chinatown_theme():
    """The cage fight: A minor pentatonic, 132 bpm. Taiko, plucked zither, a bowed lead, gongs."""
    bpm = 132
    beat = 60 / bpm
    bar = 4 * beat
    roots = [45, 45, 43, 40, 45, 45, 48, 43, 41, 43, 45, 45, 41, 43, 40, 45]
    pent = [57, 60, 62, 64, 67, 69, 72, 74, 76]
    length = len(roots) * bar
    n = int((length + 4) * SR)
    zi, bs, dr, ld, gg = (np.zeros(n) for _ in range(5))
    S, H = snare(), hat()
    for b, r in enumerate(roots):
        t0 = b * bar
        if b % 8 == 0:
            put(gg, gong(), t0, 0.8)
        for q in range(16):
            k = [0, 2, 4, 2, 5, 4, 2, 1, 0, 2, 4, 6, 5, 4, 2, 4][q] + (b % 2)
            put(zi, pluck(note_hz(pent[k % len(pent)]), 0.3, 0.993, 0.7), t0 + q * beat / 4, 0.35)
        for q in range(8):
            put(bs, synth_bass(r + (0 if q % 4 != 3 else 7), beat * 0.45, lp=700), t0 + q * beat / 2)
        put(dr, taiko(), t0)
        put(dr, taiko() * 0.7, t0 + 1.5 * beat)
        put(dr, taiko(), t0 + 2 * beat)
        put(dr, S * 0.5, t0 + beat)
        put(dr, S * 0.5, t0 + 3 * beat)
        for q in range(8):
            put(dr, H * 0.25, t0 + q * beat / 2)
        if b % 4 == 3:
            for k in range(4):
                put(dr, tom(180 - 30 * k) * 0.6, t0 + 3 * beat + k * beat / 4)
    tune = [(4, 0, 1.5, 76), (4, 1.5, 0.5, 74), (4, 2, 2, 72), (5, 0, 1, 69), (5, 1, 1, 72), (5, 2, 2, 74),
            (6, 0, 1.5, 79), (6, 1.5, 0.5, 76), (6, 2, 2, 74), (7, 0, 4, 76),
            (8, 0, 1, 72), (8, 1, 1, 74), (8, 2, 2, 76), (9, 0, 1, 79), (9, 1, 1, 76), (9, 2, 2, 74),
            (10, 0, 1.5, 72), (10, 1.5, 0.5, 69), (10, 2, 2, 67), (11, 0, 4, 69),
            (12, 0, 1, 81), (12, 1, 1, 79), (12, 2, 2, 76), (13, 0, 1, 74), (13, 1, 1, 76), (13, 2, 2, 79),
            (14, 0, 2, 76), (14, 2, 2, 72), (15, 0, 4, 69)]
    for bb, bt, bl, m in tune:
        put(ld, fiddle(m, bl * beat * 0.95), bb * bar + bt * beat)
    mix = mix_stereo([(zi, -0.4, 0.3), (bs, 0, 0.42), (dr, 0, 0.5), (ld, 0.25, 0.3), (gg, 0, 0.35)])
    save_loop("MusicChinatown", with_hall(mix, 0.5), length)


def pirates_theme():
    """Pirates!: a space shanty in 6/8, D minor. Accordion, fiddle, toms, a synth lead overhead."""
    eighth = 0.27
    bar = 6 * eighth
    prog = ["Dm", "Dm", "A", "Dm", "Dm", "Bb", "C", "A", "Dm", "C", "Bb", "A", "Dm", "Bb", "A", "Dm"]
    CH = dict(CHORDS, Bb=[58, 62, 65], A=[57, 61, 64])
    RT = dict(ROOT, Dm=38, Bb=34, A=33, C=36)
    length = len(prog) * bar
    n = int((length + 4) * SR)
    ac, bs, dr, ld, sl = (np.zeros(n) for _ in range(5))
    for b, c in enumerate(prog):
        t0 = b * bar
        put(bs, bass(RT[c], eighth * 2.5, "tri"), t0)
        put(bs, bass(RT[c] + 7, eighth * 2.5, "tri"), t0 + 3 * eighth, 0.8)
        for q in (1, 2, 4, 5):
            put(ac, accordion(CH[c], eighth * 0.8), t0 + q * eighth, 0.55)
        put(dr, kick() * 0.7, t0)
        put(dr, tom(120) * 0.6, t0 + 3 * eighth)
        put(dr, snare() * 0.35, t0 + 3 * eighth)
        for q in range(6):
            put(dr, hat() * 0.2, t0 + q * eighth)
        if b % 4 == 3:
            for k in range(3):
                put(dr, tom(170 - 30 * k) * 0.6, t0 + (3 + k) * eighth)
    tune = [(0, 0, 2, 74), (0, 2, 1, 76), (0, 3, 2, 77), (0, 5, 1, 76), (1, 0, 3, 74), (1, 3, 3, 69),
            (2, 0, 2, 73), (2, 2, 1, 74), (2, 3, 2, 76), (2, 5, 1, 73), (3, 0, 6, 74),
            (4, 0, 2, 81), (4, 2, 1, 79), (4, 3, 2, 77), (4, 5, 1, 76), (5, 0, 3, 74), (5, 3, 3, 77),
            (6, 0, 2, 76), (6, 2, 1, 74), (6, 3, 2, 72), (6, 5, 1, 76), (7, 0, 6, 73),
            (8, 0, 2, 74), (8, 2, 1, 77), (8, 3, 2, 81), (8, 5, 1, 79), (9, 0, 3, 76), (9, 3, 3, 72),
            (10, 0, 2, 74), (10, 2, 1, 77), (10, 3, 2, 82), (10, 5, 1, 81), (11, 0, 6, 76),
            (12, 0, 2, 81), (12, 2, 1, 79), (12, 3, 2, 77), (12, 5, 1, 76), (13, 0, 3, 74), (13, 3, 3, 77),
            (14, 0, 2, 76), (14, 2, 1, 73), (14, 3, 3, 69), (15, 0, 6, 74)]
    for bb, bt, bl, m in tune:
        put(ld, fiddle(m, bl * eighth * 0.95), bb * bar + bt * eighth)
        if bb >= 8:
            put(sl, synth_lead(m + 12, bl * eighth * 0.9, bright=2500), bb * bar + bt * eighth, 0.4)
    mix = mix_stereo([(ac, -0.3, 0.3), (bs, 0, 0.45), (dr, 0, 0.45), (ld, 0.25, 0.32), (sl, 0.4, 0.15)])
    save_loop("MusicPirates", with_hall(mix, 0.5), length)


def crisis_theme():
    """Crisis and Repair: tense. E minor, 140 bpm. Pulsing bass, alarm stabs, a heartbeat kick."""
    bpm = 140
    beat = 60 / bpm
    bar = 4 * beat
    prog = ["Em", "Em", "C", "B", "Em", "Em", "Am", "B", "C", "Am", "Em", "B", "C", "D", "B", "B"]
    length = len(prog) * bar
    n = int((length + 4) * SR)
    pd, bs, dr, st, ld = (np.zeros(n) for _ in range(5))
    for b, c in enumerate(prog):
        t0 = b * bar
        put(pd, pad(CHORDS[c], bar * 1.05, lp=900), t0)
        for q in range(16):
            put(bs, synth_bass(ROOT[c] + (12 if q % 4 == 2 else 0), beat * 0.22, lp=600), t0 + q * beat / 4, 0.8)
        put(dr, kick(), t0)
        put(dr, kick() * 0.7, t0 + 0.35 * beat)
        put(dr, kick(), t0 + 2 * beat)
        put(dr, kick() * 0.7, t0 + 2.35 * beat)
        for q in range(16):
            put(dr, hat() * (0.28 if q % 2 else 0.15), t0 + q * beat / 4)
        if b >= 4:
            put(dr, gated_snare() * 0.4, t0 + beat)
            put(dr, gated_snare() * 0.4, t0 + 3 * beat)
        if b % 2 == 1:
            put(st, blip(CHORDS[c][2] + 24, beat * 0.25, "square"), t0 + 1.5 * beat, 0.5)
            put(st, blip(CHORDS[c][2] + 24, beat * 0.25, "square"), t0 + 3.5 * beat, 0.5)
    motif = [(8, 0, 0.5, 76), (8, 0.5, 0.5, 79), (8, 1, 1, 83), (8, 2, 2, 81), (9, 0, 1, 79), (9, 1, 1, 76), (9, 2, 2, 72),
             (10, 0, 0.5, 76), (10, 0.5, 0.5, 79), (10, 1, 1, 83), (10, 2, 2, 86), (11, 0, 4, 83),
             (12, 0, 1, 84), (12, 1, 1, 83), (12, 2, 2, 79), (13, 0, 1, 81), (13, 1, 1, 78), (13, 2, 2, 74), (14, 0, 4, 75)]
    for bb, bt, bl, m in motif:
        put(ld, synth_lead(m, bl * beat * 0.92, bright=2600), bb * bar + bt * beat)
    mix = mix_stereo([(pd, 0, 0.3), (bs, 0, 0.45), (dr, 0, 0.45), (st, -0.3, 0.2), (ld, 0.2, 0.3)])
    save_loop("MusicCrisis", with_hall(mix, 0.4), length)


SFX = {
    "Jump": jump, "DoubleJump": double_jump, "Land": land,
    "Footstep1": lambda: footstep(0) * 0.6, "Footstep2": lambda: footstep(1) * 0.6,
    "Swipe": swipe, "Pounce": pounce, "GroundPound": ground_pound, "Hit": hit, "Poof": poof, "Hurt": cat_hurt,
    "Meow1": lambda: meow(0), "Meow2": lambda: meow(1), "Meow3": lambda: meow(2), "Purr": purr, "Hiss": hiss,
    "Catnip": catnip_blip, "Pickup": pickup_big, "Styro": styro_squeak, "Velcro": velcro_rip, "Talk": talk,
    "MenuMove": menu_move, "MenuSelect": menu_select, "Pause": pause, "Objective": objective, "Checkpoint": checkpoint_chime,
    "Bark": bark, "Growl": growl, "LaserWarn": laser_warn, "LaserBlast": laser_blast, "Explosion": explosion,
    "Alarm": alarm, "Weld": weld, "Zap": zap, "Steam": steam, "Hyperspace": hyperspace, "Cheer": cheer,
    "Boing": boing, "Splash": splash, "Clang": clang, "Gun": gun, "OneUp": one_up,
}


# ================================================================ chapters 5-9: effects

def chirp(variant):
    f = [(900, 1500, 1200), (1100, 1700, 1400)][variant]
    x = voice([("i", 0.05), ("y", 0.06)], f, rasp=0.004, breath=0.02)
    x = np.concatenate([x, np.zeros(int(0.03 * SR)), voice([("i", 0.04)], (f[1], f[1] * 1.15), breath=0.02) * 0.8])
    return reverb(pad_tail(x, 0.2), 0.15, IR_SHORT)


def roar():
    x = voice([("a", 0.3), ("o", 0.4), ("u", 0.3)], (70, 95, 80, 60), rasp=0.15, breath=0.35)
    x = layer((x, 0), (filt(noise(1.0), "low", 600) * adsr(int(1.0 * SR), 0.1, 0.2, 0.6, 0.5, 0.6), 0, 0.5))
    return reverb(pad_tail(x, 0.6), 0.35, IR_CANYON)


def portal():
    dur = 0.9
    t = t_axis(dur)
    x = osc("sine", 300 * np.exp(t * 2.2), dur) * 0.5 + osc("sine", 450 * np.exp(t * 2.2), dur) * 0.3
    x = x * adsr(len(t), 0.05, 0.2, 0.6, 0.4, 0.5) + sweep_lowpass(noise(dur), 400, 7000) * adsr(len(t), 0.1, 0.2, 0.4, 0.4, 0.4) * 0.4
    return reverb(x, 0.45, IR_CANYON)


def correct():
    return chime([72, 76, 79, 84, 88, 91], 0.06, kind="tri", dur=0.35)


def wrong():
    dur = 0.6
    t = t_axis(dur)
    x = filt(osc("square", 220 * (1 - 0.3 * t), dur), "low", 1800) * adsr(len(t), 0.01, 0.1, 0.7, 0.2, 0.4)
    return x * (0.7 + 0.3 * np.sign(np.sin(2 * np.pi * 9 * t)))


SFX.update({"Chirp1": lambda: chirp(0), "Chirp2": lambda: chirp(1), "Roar": roar, "Portal": portal, "Correct": correct, "Wrong": wrong})


# ================================================================ chapters 5-9: music

def jungle_theme():
    """Fluffer-Nutter: a sunny jungle groove. G major, 112 bpm. Marimbas, bongos, a flute."""
    bpm = 112
    beat = 60 / bpm
    bar = 4 * beat
    prog = ["G", "C", "G", "D", "Em", "C", "G", "D", "C", "D", "G", "Em", "C", "D", "G", "G"]
    CH = dict(CHORDS, G=[55, 59, 62], D=[62, 66, 69])
    RT = dict(ROOT, G=43, C=36, D=38, Em=40)
    length = len(prog) * bar
    n = int((length + 4) * SR)
    mr, bs, dr, ld = (np.zeros(n) for _ in range(4))
    for b, c in enumerate(prog):
        t0 = b * bar
        tones = CH[c]
        for q, k in enumerate([0, 1, 2, 1, 0, 2, 1, 2]):
            put(mr, marimba(tones[k] + 12, 0.35), t0 + q * beat / 2, 0.5 if q % 2 == 0 else 0.35)
        put(bs, bass(RT[c], beat * 1.5, "tri"), t0)
        put(bs, bass(RT[c] + 7, beat * 0.9, "tri"), t0 + 1.5 * beat, 0.7)
        put(bs, bass(RT[c] + 12, beat * 0.9, "tri"), t0 + 3 * beat, 0.6)
        for q, (f, g) in enumerate([(240, 0.5), (180, 0.35), (240, 0.3), (300, 0.4), (180, 0.5), (240, 0.3), (300, 0.35), (180, 0.3)]):
            put(dr, tom(f) * g, t0 + q * beat / 2)
        for q in range(16):
            put(dr, tambourine() * 0.15, t0 + q * beat / 4)
        put(dr, kick() * 0.5, t0)
        put(dr, kick() * 0.4, t0 + 2 * beat)
    tune = [(0, 0, 1, 74), (0, 1, 1, 79), (0, 2, 2, 83), (1, 0, 1, 81), (1, 1, 1, 79), (1, 2, 2, 76),
            (2, 0, 1, 74), (2, 1, 1, 79), (2, 2, 1, 83), (2, 3, 1, 86), (3, 0, 4, 81),
            (4, 0, 1, 83), (4, 1, 1, 79), (4, 2, 2, 76), (5, 0, 1, 76), (5, 1, 1, 79), (5, 2, 2, 84),
            (6, 0, 1, 83), (6, 1, 1, 81), (6, 2, 2, 79), (7, 0, 4, 78),
            (8, 0, 1, 76), (8, 1, 1, 79), (8, 2, 2, 84), (9, 0, 1, 81), (9, 1, 1, 78), (9, 2, 2, 74),
            (10, 0, 1, 79), (10, 1, 1, 83), (10, 2, 2, 86), (11, 0, 4, 83),
            (12, 0, 1, 84), (12, 1, 1, 83), (12, 2, 2, 79), (13, 0, 2, 81), (13, 2, 2, 78), (14, 0, 4, 79)]
    for bb, bt, bl, m in tune:
        put(ld, whistle(m, bl * beat * 0.92), bb * bar + bt * beat)
    mix = mix_stereo([(mr, -0.3, 0.38), (bs, 0, 0.42), (dr, 0.15, 0.4), (ld, 0.2, 0.3)])
    save_loop("MusicJungle", with_hall(mix, 0.45), length)


def labyrinth_theme():
    """The Labyrinth of Time: mysterious, ticking. D minor, 84 bpm. Bells, pads, a clock."""
    bpm = 84
    beat = 60 / bpm
    bar = 4 * beat
    prog = ["Dm", "Bb", "Dm", "A", "Dm", "Bb", "C", "A", "Dm", "C", "Bb", "A", "Dm", "Bb", "A", "Dm"]
    CH = dict(CHORDS, Bb=[58, 62, 65], A=[57, 61, 64])
    RT = dict(ROOT, Dm=38, Bb=34, A=33, C=36)
    length = len(prog) * bar
    n = int((length + 4) * SR)
    pd, bl, bs, ck, ld = (np.zeros(n) for _ in range(5))
    for b, c in enumerate(prog):
        t0 = b * bar
        put(pd, pad(CH[c] + [CH[c][0] + 12], bar * 1.05, lp=1100), t0)
        put(bs, bass(RT[c], bar * 0.95, "sine"), t0, 0.9)
        for q in range(8):
            put(ck, woodblock(91 if q % 2 == 0 else 86) * (0.35 if q % 2 == 0 else 0.2), t0 + q * beat / 2)
        tones = CH[c] + [CH[c][0] + 12]
        for q in range(4):
            put(bl, bell(tones[(q * 3 + b) % 4] + 24, 1.5, 2.0), t0 + q * beat, 0.3)
    motif = [(4, 0, 2, 74), (4, 2, 1, 77), (4, 3, 1, 76), (5, 0, 4, 74), (6, 0, 2, 72), (6, 2, 2, 76), (7, 0, 4, 73),
             (8, 0, 2, 77), (8, 2, 1, 81), (8, 3, 1, 79), (9, 0, 4, 76), (10, 0, 2, 74), (10, 2, 2, 77), (11, 0, 4, 76),
             (12, 0, 2, 74), (12, 2, 1, 72), (12, 3, 1, 70), (13, 0, 4, 69), (14, 0, 4, 73)]
    for bb, bt, bl_, m in motif:
        put(ld, epiano(m, bl_ * beat * 0.95), bb * bar + bt * beat)
    mix = mix_stereo([(pd, 0, 0.35), (bl, -0.35, 0.25), (bs, 0, 0.4), (ck, 0.3, 0.3), (ld, 0.15, 0.4)])
    save_loop("MusicLabyrinth", with_hall(mix, 0.8), length)


def dimension_theme():
    """Another Dimension: floating, luminous. E major, 70 bpm. Shimmering bells over warm pads."""
    bpm = 70
    beat = 60 / bpm
    bar = 4 * beat
    prog = ["E", "Cs", "A", "B", "E", "Gs", "A", "B"]
    CH = {"E": [52, 56, 59, 63], "Cs": [49, 52, 56, 59], "A": [45, 49, 52, 56], "B": [47, 51, 54, 57], "Gs": [44, 47, 51, 54]}
    length = len(prog) * bar
    n = int((length + 4) * SR)
    pd, ar, bs, ld = (np.zeros(n) for _ in range(4))
    for b, c in enumerate(prog):
        t0 = b * bar
        put(pd, pad([m + 12 for m in CH[c]], bar * 1.08, lp=1400), t0)
        put(bs, bass(CH[c][0] - 12, bar * 0.95, "sine"), t0, 0.8)
        tones = [m + 24 for m in CH[c]] + [CH[c][0] + 36]
        for q in range(16):
            put(ar, bell(tones[[0, 1, 2, 3, 4, 3, 2, 1][q % 8]], 1.2, 1.2), t0 + q * beat / 4, 0.18 if q % 4 else 0.28)
    melody = [(0, 0, 3, 80), (0, 3, 1, 78), (1, 0, 4, 76), (2, 0, 2, 73), (2, 2, 2, 76), (3, 0, 4, 75),
              (4, 0, 3, 83), (4, 3, 1, 80), (5, 0, 4, 80), (6, 0, 2, 81), (6, 2, 2, 78), (7, 0, 4, 78)]
    for bb, bt, bl_, m in melody:
        put(ld, synth_lead(m, bl_ * beat * 0.95, bright=1800, vib=0.01), bb * bar + bt * beat)
    mix = mix_stereo([(pd, 0, 0.4), (ar, -0.3, 0.25), (bs, 0, 0.35), (ld, 0.25, 0.22)])
    save_loop("MusicDimension", with_hall(mix, 0.9), length)

if __name__ == "__main__":
    import sys
    only = sys.argv[1:]  # e.g. "sfx" or "music" to make just one half
    print("audio ->", os.path.normpath(OUT))
    if not only or "sfx" in only:
        for name, fn in SFX.items():
            save(name, fn())
        fanfare([(0, 0.5, 72), (0.5, 0.5, 76), (1, 0.5, 79), (1.5, 2.5, 84)], 0.18, "ChapterStart")
        fanfare([(0, 0.5, 72), (0.5, 0.5, 76), (1, 0.5, 79), (1.5, 1, 84), (2.5, 0.5, 81), (3, 0.5, 84), (3.5, 3, 88)], 0.18, "ChapterComplete")
        fanfare([(0, 0.25, 79), (0.25, 0.25, 84), (0.5, 0.25, 88), (0.75, 2, 91)], 0.16, "HatCraft", wet=0.6)
        death_sting()
    if not only or "music" in only:
        title_theme()
        cranberry_theme()
        chinatown_theme()
        pirates_theme()
        crisis_theme()
        jungle_theme()
        labyrinth_theme()
        dimension_theme()
