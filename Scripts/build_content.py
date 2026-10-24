"""Create Sparky and Leo's materials and its (empty) map, and import the textures and audio.

Run headless with ./build_content.sh, or in the editor: Tools > Execute Python Script.
The levels themselves are built at runtime by the C++ (from the ASCII maps in
the C++ (Source/SparkyLeo/Levels.cpp)), so the map only needs a PlayerStart.

Materials and the map are only created when missing, so your edits survive a re-run
(delete an asset to have it rebuilt). Textures and audio are always re-imported from
TextureSource/*.png and AudioSource/*.wav.
"""
import glob
import os

import unreal

HERE = os.path.dirname(os.path.abspath(__file__))
MAT_DIR = "/Game/Materials"
TEX_DIR = "/Game/Textures"
AUDIO_DIR = "/Game/Audio"
MAP = "/Game/Maps/Main"
LOOPING = {"MusicTitle", "MusicCranberry", "MusicChinatown", "MusicPirates", "MusicCrisis", "MusicJungle", "MusicLabyrinth", "MusicDimension"}

eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
P = unreal.MaterialProperty


def import_files(pattern, dest):
    files = sorted(glob.glob(os.path.join(HERE, "..", pattern)))
    tasks = []
    for f in files:
        existing = f"{dest}/{os.path.splitext(os.path.basename(f))[0]}"
        if eal.does_asset_exist(existing):
            eal.load_asset(existing)  # fully loaded, or the re-import can't be saved over it
        t = unreal.AssetImportTask()
        t.set_editor_property("filename", f)
        t.set_editor_property("destination_path", dest)
        t.set_editor_property("automated", True)
        t.set_editor_property("replace_existing", True)
        t.set_editor_property("save", False)
        tasks.append(t)
    tools.import_asset_tasks(tasks)
    return [os.path.splitext(os.path.basename(f))[0] for f in files]


textures = import_files("TextureSource/*.png", TEX_DIR)
for name in textures:
    tex = unreal.load_asset(f"{TEX_DIR}/{name}")
    if tex is None:
        raise RuntimeError("import failed: " + name)
    tex.set_editor_property("filter", unreal.TextureFilter.TF_BILINEAR)  # N64: blurry, never sharp
    tex.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_WORLD)
    tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_EDITOR_ICON)  # keep the palette exact
    tex.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_FROM_TEXTURE_GROUP)
    eal.save_loaded_asset(tex)


def new_material(name):
    """None if it already exists (kept as is)."""
    if eal.does_asset_exist(f"{MAT_DIR}/{name}"):
        unreal.log(f"keeping existing {name}")
        return None
    return tools.create_asset(name, MAT_DIR, unreal.Material, unreal.MaterialFactoryNew())


def node(mat, cls, x, y, **props):
    n = mel.create_material_expression(mat, cls, x, y)
    for k, v in props.items():
        n.set_editor_property(k, v)
    return n


def vector(mat, name, rgb, x, y):
    return node(mat, unreal.MaterialExpressionVectorParameter, x, y, parameter_name=name,
                default_value=unreal.LinearColor(*rgb, 1.0))


def scalar(mat, name, value, x, y):
    return node(mat, unreal.MaterialExpressionScalarParameter, x, y, parameter_name=name, default_value=value)


def binary(mat, cls, a, b, x, y, a_out="", b_out=""):
    m = node(mat, cls, x, y)
    mel.connect_material_expressions(a, a_out, m, "A")
    mel.connect_material_expressions(b, b_out, m, "B")
    return m


def finish(mat):
    mel.recompile_material(mat)
    eal.save_loaded_asset(mat)


# M_Retro: everything in the world. Unlit; the lighting is baked into the vertex colours by
# the C++ (Gouraud, like the N64), with linear distance fog done here.
#   Emissive = lerp(Tex * VertexColor * Tint * 2, FogColor, fog) + FlashColor * Flash
retro = new_material("M_Retro")
if retro:
    retro.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    tex = node(retro, unreal.MaterialExpressionTextureSampleParameter2D, -1400, -200, parameter_name="Tex",
               texture=unreal.load_asset("/Engine/EngineResources/WhiteSquareTexture"))
    vc = node(retro, unreal.MaterialExpressionVertexColor, -1400, 50)
    lit = binary(retro, unreal.MaterialExpressionMultiply, tex, vc, -1100, -100, a_out="RGB")
    tinted = binary(retro, unreal.MaterialExpressionMultiply, lit, vector(retro, "Tint", (1, 1, 1), -1400, 250), -900, -50)
    base = binary(retro, unreal.MaterialExpressionMultiply, tinted, node(retro, unreal.MaterialExpressionConstant, -1100, 350, r=2.0), -700, 0)

    dist = binary(retro, unreal.MaterialExpressionDistance,
                  node(retro, unreal.MaterialExpressionCameraPositionWS, -1400, 500),
                  node(retro, unreal.MaterialExpressionWorldPosition, -1400, 600), -1100, 550)
    start = scalar(retro, "FogStart", 1500.0, -1400, 750)
    end = scalar(retro, "FogEnd", 6000.0, -1400, 850)
    num = binary(retro, unreal.MaterialExpressionSubtract, dist, start, -900, 600)
    den = binary(retro, unreal.MaterialExpressionSubtract, end, start, -900, 750)
    frac = binary(retro, unreal.MaterialExpressionDivide, num, den, -700, 650)
    sat = node(retro, unreal.MaterialExpressionSaturate, -550, 650)
    mel.connect_material_expressions(frac, "", sat, "")
    fog = binary(retro, unreal.MaterialExpressionMultiply, sat, scalar(retro, "FogAmount", 1.0, -700, 850), -400, 650)

    lerp = node(retro, unreal.MaterialExpressionLinearInterpolate, -250, 100)
    mel.connect_material_expressions(base, "", lerp, "A")
    mel.connect_material_expressions(vector(retro, "FogColor", (0.5, 0.4, 0.3), -550, 350), "", lerp, "B")
    mel.connect_material_expressions(fog, "", lerp, "Alpha")
    flash = binary(retro, unreal.MaterialExpressionMultiply, vector(retro, "FlashColor", (1, 0.2, 0.1), -550, 950),
                   scalar(retro, "Flash", 0.0, -550, 1100), -250, 950)
    out = binary(retro, unreal.MaterialExpressionAdd, lerp, flash, -50, 300)
    mel.connect_material_property(out, "", P.MP_EMISSIVE_COLOR)
    finish(retro)

# M_Glow: additive, unlit. Stars, plasma, sparks, lasers, welding. Color x Intensity x VertexColor.
glow = new_material("M_Glow")
if glow:
    glow.set_editor_property("blend_mode", unreal.BlendMode.BLEND_ADDITIVE)
    glow.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    emit = binary(glow, unreal.MaterialExpressionMultiply, vector(glow, "Color", (1, 1, 1), -500, 0),
                  scalar(glow, "Intensity", 1.0, -500, 200), -300, 50)
    emit = binary(glow, unreal.MaterialExpressionMultiply, emit, node(glow, unreal.MaterialExpressionVertexColor, -500, 350),
                  -100, 100)
    mel.connect_material_property(emit, "", P.MP_EMISSIVE_COLOR)
    finish(glow)

# M_Glow may be used on instanced meshes: the material has to say so
# (or the instances fall back to the default material and turn black).
glow = unreal.load_asset(f"{MAT_DIR}/M_Glow")
if not glow.get_editor_property("used_with_instanced_static_meshes"):
    glow.set_editor_property("used_with_instanced_static_meshes", True)
    finish(glow)

# Map: an empty level with a PlayerStart.
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
if eal.does_asset_exist(MAP):
    unreal.log("keeping existing " + MAP)
else:
    if not les.new_level(MAP):
        raise RuntimeError("could not create " + MAP)
    eas.spawn_actor_from_class(unreal.PlayerStart, unreal.Vector(0, 0, 100), unreal.Rotator(0, 0, 0))
    les.save_current_level()

# Audio: import every WAV, replacing what's there.
sounds = import_files("AudioSource/*.wav", AUDIO_DIR)
for name in sounds:
    sound = unreal.load_asset(f"{AUDIO_DIR}/{name}")
    if sound is None:
        raise RuntimeError("import failed: " + name)
    sound.set_editor_property("looping", name in LOOPING)
    eal.save_loaded_asset(sound)
unreal.log(f"SparkyLeo content built ({len(textures)} textures, {len(sounds)} sounds)")
