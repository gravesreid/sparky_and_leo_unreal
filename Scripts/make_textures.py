"""Paint Sparky and Leo's textures as small PNGs (no source images, all maths).

    python3 Scripts/make_textures.py         # writes TextureSource/*.png
    ./build_content.sh                       # imports them into Content/Textures

128x128 "Expansion Pak" textures with gently posterized colour, bilinear-filtered in game,
like an N64 cartridge's. Everything is seeded, so re-running gives identical files.
"""
import os

import numpy as np
from PIL import Image

OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "TextureSource")
N = 128
yy, xx = np.mgrid[0:N, 0:N]


def rng(seed):
    return np.random.default_rng(seed)


def value_noise(size, cells, seed):
    """Smooth tileable noise in 0..1."""
    r = rng(seed)
    g = r.random((cells, cells))
    x = np.arange(size) * cells / size
    i0 = np.floor(x).astype(int)
    f = x - i0
    f = f * f * (3 - 2 * f)
    i1 = (i0 + 1) % cells
    a = g[np.ix_(i0, i0)] * (1 - f)[None, :] + g[np.ix_(i0, i1)] * f[None, :]
    b = g[np.ix_(i1, i0)] * (1 - f)[None, :] + g[np.ix_(i1, i1)] * f[None, :]
    return a * (1 - f)[:, None] + b * f[:, None]


def fbm(seed, octaves=(4, 8, 16), weights=(0.5, 0.3, 0.2)):
    return sum(w * value_noise(N, c, seed + k) for k, (c, w) in enumerate(zip(octaves, weights)))


def grain(seed, amount):
    return (rng(seed).random((N, N)) - 0.5) * amount


def ramp(t, colors):
    """Map 0..1 to a colour ramp (list of RGB 0..255)."""
    c = np.array(colors, dtype=float)
    t = np.clip(t, 0, 1) * (len(c) - 1)
    i = np.minimum(t.astype(int), len(c) - 2)
    f = (t - i)[..., None]
    return c[i] * (1 - f) + c[i + 1] * f


def save(name, img, levels=24):
    os.makedirs(OUT, exist_ok=True)
    step = 256 / levels
    img = np.clip(np.round(np.asarray(img, dtype=float) / step) * step, 0, 255)
    Image.fromarray(img.astype(np.uint8), "RGB").save(os.path.join(OUT, name + ".png"))
    print(f"  {name}.png")


def circle(cx, cy, r):
    """Tileable disc mask."""
    dx = np.minimum(np.abs(xx - cx), N - np.abs(xx - cx))
    dy = np.minimum(np.abs(yy - cy), N - np.abs(yy - cy))
    return dx * dx + dy * dy < r * r


# ---------------------------------------------------------------- Cranberry Plasma

def grass():
    """Alien turf: magenta-violet tufts."""
    t = fbm(10) * 0.8 + grain(11, 0.15)
    img = ramp(t, [(95, 45, 85), (125, 60, 105), (150, 80, 125), (170, 105, 140)])
    blades = (rng(12).random((N, N)) > 0.975)
    img[blades] = (185, 130, 160)
    return img


def tile():
    """Plaza flagstones: pale pink stone, irregular joints."""
    t = 0.55 + (fbm(20) - 0.5) * 0.4 + grain(21, 0.1)
    img = ramp(t, [(170, 140, 150), (205, 180, 185), (230, 210, 210)])
    joint = ((yy % 32) < 2) | (((xx + (yy // 32) * 16) % 32) < 2)
    img[joint] = (120, 90, 100)
    return img


def stucco():
    """Cranberry adobe walls."""
    t = 0.5 + (fbm(30, (4, 8, 32), (0.4, 0.3, 0.3)) - 0.5) * 0.6 + grain(31, 0.12)
    return ramp(t, [(150, 50, 70), (190, 75, 90), (215, 105, 115)])


def styro():
    """Styrofoam: white, made of little fused beads."""
    img = np.full((N, N, 3), 238.0)
    r = rng(40)
    for _ in range(260):
        cx, cy, rad = r.integers(0, N), r.integers(0, N), r.integers(3, 7)
        d = np.hypot(np.minimum(np.abs(xx - cx), N - np.abs(xx - cx)), np.minimum(np.abs(yy - cy), N - np.abs(yy - cy)))
        rim = (d > rad - 1.2) & (d < rad)
        img[rim] -= 22
    img += grain(41, 10)[..., None]
    img[..., 2] -= 6
    return np.clip(img, 0, 255)


def velcro():
    """Velcro: fuzzy hot-pink loops."""
    t = 0.5 + grain(50, 0.7) * 0.8 + (value_noise(N, 32, 51) - 0.5) * 0.4
    img = ramp(t, [(170, 40, 110), (230, 80, 160), (255, 150, 210)])
    stripe = (yy % 16) < 2
    img[stripe] *= 0.75
    return img


def planks():
    t = 0.5 + (value_noise(N, 4, 60)[:, :1] - 0.5) * 0.3 + grain(61, 0.1)
    grainy = np.sin(yy * 0.9 + value_noise(N, 8, 62) * 12) * 0.08
    img = ramp(t + grainy, [(110, 70, 50), (150, 100, 70), (180, 130, 90)])
    img[(xx % 32) < 2] = (70, 45, 35)
    return img


def rock():
    t = fbm(70, (4, 8, 16, 32), (0.35, 0.3, 0.2, 0.15))
    img = ramp(t, [(110, 70, 90), (160, 110, 125), (200, 155, 160)])
    return img + grain(71, 14)[..., None]


def bog():
    """The cranberry bog: deep red jelly with floating berries catching the light."""
    t = 0.4 + (fbm(80) - 0.5) * 0.6
    img = ramp(t, [(80, 5, 25), (130, 15, 40), (170, 30, 55)])
    r = rng(81)
    for _ in range(14):
        cx, cy, rad = r.integers(0, N), r.integers(0, N), r.integers(4, 9)
        img[circle(cx, cy, rad)] = (200, 30, 60)
        img[circle(cx - rad // 3, cy - rad // 3, max(1, rad // 3))] = (255, 140, 160)
    return img


def lily():
    """A lily pad: green, with a notch and veins."""
    t = 0.5 + (fbm(90) - 0.5) * 0.4
    img = ramp(t, [(40, 110, 50), (70, 150, 70), (110, 190, 90)])
    ang = np.arctan2(yy - 64, xx - 64)
    veins = np.abs(np.sin(ang * 6)) < 0.08
    img[veins] = (130, 210, 110)
    return img


def pad():
    """The landing pad: concrete with hazard ring and an H."""
    t = 0.55 + (fbm(100) - 0.5) * 0.25 + grain(101, 0.08)
    img = ramp(t, [(120, 120, 125), (160, 160, 165)])
    r = np.hypot(xx - 64, yy - 64)
    ring = (r > 52) & (r < 60)
    img[ring & (((xx + yy) // 8) % 2 == 0)] = (240, 190, 40)
    img[ring & (((xx + yy) // 8) % 2 == 1)] = (30, 30, 35)
    h = ((np.abs(xx - 48) < 5) | (np.abs(xx - 80) < 5)) & (np.abs(yy - 64) < 24)
    h |= (np.abs(yy - 64) < 4) & (np.abs(xx - 64) < 18)
    img[h] = (235, 235, 235)
    return img


def jelly():
    """Jelly hill: wobbly translucent-looking cranberry gel with bubbles."""
    t = 0.5 + (fbm(110, (2, 4, 8), (0.5, 0.3, 0.2)) - 0.5) * 0.7
    img = ramp(t, [(170, 20, 60), (215, 50, 90), (245, 100, 130)])
    r = rng(111)
    for _ in range(20):
        cx, cy, rad = r.integers(0, N), r.integers(0, N), r.integers(2, 6)
        img[circle(cx, cy, rad)] = (250, 150, 180)
    return img


def plasma():
    """Plasma platform tops: glowing violet with energy lines."""
    t = 0.5 + np.sin(xx * 0.2 + value_noise(N, 4, 120) * 8) * 0.2
    img = ramp(t, [(80, 30, 130), (150, 60, 200), (220, 140, 255)])
    img[((xx % 32) < 2) | ((yy % 32) < 2)] = (255, 200, 255)
    return img


def awning():
    """Market awning: candy stripes."""
    img = np.where(((xx // 16) % 2 == 0)[..., None], np.array([240, 220, 230.0]), np.array([210, 60, 120.0]))
    return img * (0.9 + grain(130, 0.1)[..., None])


# ---------------------------------------------------------------- Chinatown

def asphalt():
    """Wet night asphalt: dark, with puddles reflecting neon."""
    t = 0.35 + (fbm(200, (8, 16, 32), (0.4, 0.3, 0.3)) - 0.5) * 0.3 + grain(201, 0.15)
    img = ramp(t, [(25, 25, 32), (45, 45, 55), (65, 62, 72)])
    puddle = value_noise(N, 4, 202) > 0.7
    img[puddle] = img[puddle] * 0.6 + np.array([60, 30, 50]) * 0.4
    return img


def brick():
    t = 0.5 + grain(210, 0.25) + (fbm(211) - 0.5) * 0.3
    img = ramp(t, [(110, 45, 35), (150, 65, 45), (175, 90, 65)])
    row = yy // 16
    mortar = ((yy % 16) < 2) | (((xx + (row % 2) * 16) % 32) < 2)
    img[mortar] = (70, 65, 62)
    return img


def mat():
    """The fight mat: blue canvas with a red ring line."""
    t = 0.5 + grain(220, 0.15) + (fbm(221) - 0.5) * 0.2
    img = ramp(t, [(30, 50, 110), (45, 70, 140)])
    img[(yy % 64) < 3] = (200, 40, 40)
    return img


def cage():
    """Chain-link fence."""
    img = np.full((N, N, 3), 40.0)
    link = (np.abs(((xx + yy) % 16) - 8) < 1.2) | (np.abs(((xx - yy) % 16) - 8) < 1.2)
    img[link] = (170, 175, 185)
    return img


def crate():
    t = 0.5 + (value_noise(N, 4, 230)[:, :1] - 0.5) * 0.25 + grain(231, 0.1)
    img = ramp(t, [(130, 90, 50), (170, 120, 70), (200, 150, 90)])
    img[(yy % 32) < 2] = (90, 60, 35)
    frame = (xx < 8) | (xx > N - 9) | (yy < 8) | (yy > N - 9)
    img[frame] = (100, 70, 40)
    diag = (np.abs(xx - yy) < 5) & ~frame
    img[diag] = (110, 78, 45)
    return img


def dumpster():
    t = 0.5 + grain(240, 0.15) + (fbm(241) - 0.5) * 0.3
    img = ramp(t, [(30, 70, 45), (45, 95, 60)])
    img[(xx % 32) < 3] *= 0.7
    rust = value_noise(N, 8, 242) > 0.72
    img[rust] = (110, 70, 40)
    return img


# ---------------------------------------------------------------- the ship

def hull():
    """Sparky's hull plating: white panels, rivets, a few scuffs."""
    t = 0.65 + (fbm(300) - 0.5) * 0.15 + grain(301, 0.05)
    img = ramp(t, [(170, 175, 185), (210, 215, 222), (235, 238, 242)])
    seam = ((xx % 64) < 2) | ((yy % 32) < 2)
    img[seam] = (120, 125, 135)
    rivet = ((xx % 64) - 6) ** 2 + ((yy % 32) - 6) ** 2 < 3
    img[rivet] = (140, 145, 155)
    return img


def glass():
    t = 0.5 + (yy / N) * 0.4 + grain(310, 0.05)
    img = ramp(t, [(120, 230, 255), (50, 140, 200), (20, 70, 120)])
    glint = (np.abs(xx - yy - 20) < 6) | (np.abs(xx - yy + 30) < 3)
    img[glint] = (220, 250, 255)
    return img


def grate():
    img = np.full((N, N, 3), 30.0)
    bars = ((xx % 16) < 10) & ((yy % 16) < 12)
    img[bars] = ramp(0.5 + grain(320, 0.2), [(80, 85, 95), (120, 125, 135)])[bars]
    img[(yy % 64) < 3] = (60, 62, 70)
    return img


def hazard():
    on = ((xx + yy) // 16) % 2 == 0
    return np.where(on[..., None], np.array([240, 190, 30.0]), np.array([30, 30, 34.0])) * (0.9 + grain(330, 0.1)[..., None])


def panel():
    """Interior wall panels: grey-blue with a light strip."""
    t = 0.55 + (fbm(340) - 0.5) * 0.15 + grain(341, 0.05)
    img = ramp(t, [(80, 88, 105), (110, 118, 135)])
    img[(xx % 64) < 2] = (50, 55, 65)
    img[(yy % 128) < 2] = (50, 55, 65)
    img[(np.abs((yy % 128) - 20) < 2)] = (160, 200, 230)
    return img


def carpet():
    t = 0.5 + grain(350, 0.35) + (fbm(351) - 0.5) * 0.2
    img = ramp(t, [(70, 40, 90), (100, 60, 120)])
    paws = np.zeros((N, N), bool)
    for cx, cy in [(32, 32), (96, 96)]:
        paws |= circle(cx, cy + 4, 7)
        for dx, dy in [(-8, -8), (-3, -11), (3, -11), (8, -8)]:
            paws |= circle(cx + dx, cy + dy, 3)
    img[paws] = (140, 100, 160)
    return img


def blanket():
    """Sparky's blanket: checked, with a fish."""
    on = ((xx // 16) + (yy // 16)) % 2 == 0
    img = np.where(on[..., None], np.array([200, 80, 70.0]), np.array([230, 200, 150.0]))
    return img * (0.92 + grain(360, 0.08)[..., None])


def computer():
    """Console panels: dark, with screens and buttons."""
    img = np.full((N, N, 3), 45.0) + grain(370, 8)[..., None]
    scr = (xx > 10) & (xx < 70) & (yy > 10) & (yy < 60)
    img[scr] = (20, 70, 50)
    img[scr & ((yy % 6) < 2)] = (60, 200, 120)
    r = rng(371)
    for _ in range(18):
        cx, cy = r.integers(80, 120), r.integers(10, 118)
        img[circle(cx, cy, 3)] = [(220, 60, 50), (240, 200, 50), (60, 180, 240)][r.integers(0, 3)]
    img[(yy > 70) & (yy < 118) & (xx > 10) & (xx < 70) & ((xx % 10) < 7) & ((yy % 10) < 7)] = (120, 125, 135)
    return img


TEXTURES = {
    "T_Grass": grass, "T_Tile": tile, "T_Stucco": stucco, "T_Styro": styro, "T_Velcro": velcro,
    "T_Planks": planks, "T_Rock": rock, "T_Bog": bog, "T_Lily": lily, "T_Pad": pad, "T_Jelly": jelly,
    "T_Plasma": plasma, "T_Awning": awning,
    "T_Asphalt": asphalt, "T_Brick": brick, "T_Mat": mat, "T_Cage": cage, "T_Crate": crate, "T_Dumpster": dumpster,
    "T_Hull": hull, "T_Glass": glass, "T_Grate": grate, "T_Hazard": hazard, "T_Panel": panel, "T_Carpet": carpet,
    "T_Blanket": blanket, "T_Computer": computer,
}


# ---------------------------------------------------------------- the green planet (chapter 5)

def jungle_grass():
    t = fbm(400) * 0.8 + grain(401, 0.15)
    img = ramp(t, [(45, 95, 40), (70, 130, 50), (100, 160, 65), (130, 185, 80)])
    img[rng(402).random((N, N)) > 0.975] = (170, 210, 110)
    return img


def bark():
    t = 0.5 + (value_noise(N, 8, 410)[:1, :] - 0.5) * 0.5 + grain(411, 0.1)
    ridges = np.sin(xx * 0.5 + value_noise(N, 8, 412) * 9) * 0.12
    return ramp(t + ridges, [(60, 40, 28), (95, 65, 42), (125, 90, 60)])


def leaves():
    img = ramp(fbm(420, (4, 8, 16), (0.3, 0.4, 0.3)) + grain(421, 0.2), [(30, 85, 35), (55, 125, 45), (90, 165, 60)])
    r = rng(422)
    for _ in range(40):
        cx, cy = r.integers(0, N), r.integers(0, N)
        img[circle(cx, cy, r.integers(3, 7))] *= 1.2
    return np.clip(img, 0, 255)


def thatch():
    t = 0.5 + grain(430, 0.4) * 0.6 + np.sin(xx * 1.3 + value_noise(N, 16, 431) * 6) * 0.15
    img = ramp(t, [(140, 110, 55), (185, 150, 80), (215, 185, 110)])
    img[(yy % 24) < 3] *= 0.7
    return img


def water():
    t = 0.4 + (fbm(440, (4, 8, 16), (0.4, 0.35, 0.25)) - 0.5) * 0.5
    img = ramp(t, [(25, 90, 120), (45, 130, 160), (90, 180, 200)])
    glint = (np.sin(xx * 0.6 + value_noise(N, 8, 441) * 10) > 0.94)
    img[glint] = (190, 230, 240)
    return img


def fungus():
    t = 0.5 + grain(450, 0.2) + (fbm(451) - 0.5) * 0.3
    img = ramp(t, [(170, 120, 70), (215, 165, 100), (240, 210, 150)])
    ang = np.arctan2(yy - 64, xx - 64)
    img[np.abs(np.sin(ang * 14)) < 0.12] *= 0.8
    return img


# ---------------------------------------------------------------- the Labyrinth (chapters 7-8)

def rune():
    """Time-worn stone flags with glowing glyphs."""
    t = 0.5 + (fbm(460) - 0.5) * 0.4 + grain(461, 0.1)
    img = ramp(t, [(55, 50, 75), (80, 75, 105), (105, 100, 130)])
    img[((xx % 64) < 2) | ((yy % 64) < 2)] = (35, 30, 50)
    r = rng(462)
    for _ in range(10):
        cx, cy = r.integers(8, N - 8), r.integers(8, N - 8)
        if r.random() < 0.5:
            img[cy - 6:cy + 6, cx - 1:cx + 1] = (130, 110, 230)
            img[cy - 1:cy + 1, cx - 5:cx + 5] = (130, 110, 230)
        else:
            img[circle(cx, cy, 5) & ~circle(cx, cy, 3)] = (130, 110, 230)
    return img


def marble():
    t = 0.75 + (fbm(470, (4, 8, 16), (0.4, 0.35, 0.25)) - 0.5) * 0.25
    veins = np.abs(np.sin((xx + yy) * 0.06 + value_noise(N, 8, 471) * 8)) < 0.05
    img = ramp(t, [(105, 110, 130), (140, 145, 165), (165, 170, 190)])
    img[veins] = (80, 95, 135)
    img[((xx % 64) < 1) | ((yy % 64) < 1)] = (85, 90, 110)
    return img


# ---------------------------------------------------------------- another dimension (chapter 9)

def island():
    t = fbm(480) * 0.8 + grain(481, 0.15)
    img = ramp(t, [(120, 80, 150), (165, 110, 185), (200, 150, 215)])
    img[rng(482).random((N, N)) > 0.97] = (255, 220, 255)
    return img


def crystal():
    t = 0.5 + (xx / N) * 0.3 + grain(490, 0.08)
    img = ramp(t, [(110, 80, 220), (160, 130, 255), (220, 200, 255)])
    img[np.abs(xx - yy) < 3] = (240, 235, 255)
    img[np.abs(xx + yy - 140) < 2] = (240, 235, 255)
    return img


def energy():
    t = 0.5 + np.sin(xx * 0.1 + value_noise(N, 4, 500) * 10) * 0.25 + (fbm(501) - 0.5) * 0.3
    return ramp(t, [(40, 120, 160), (120, 220, 230), (230, 160, 240)])


TEXTURES.update({
    "T_JungleGrass": jungle_grass, "T_Bark": bark, "T_Leaves": leaves, "T_Thatch": thatch, "T_Water": water,
    "T_Fungus": fungus, "T_Rune": rune, "T_Marble": marble, "T_Island": island, "T_Crystal": crystal, "T_Energy": energy,
})

if __name__ == "__main__":
    print("textures ->", os.path.normpath(OUT))
    for name, fn in TEXTURES.items():
        save(name, fn())
