from constants import (
    CORNELL_BOX_ASSET,
    CORNELL_LIGHT_EMISSIVE_STRENGTH,
    CORNELL_WALL_COLOR,
)
from pxr import Gf, Sdf, Usd, UsdGeom, UsdShade

TALL_BOX_PARTS = [
    "TallBox_Bottom",
    "TallBox_Top",
    "TallBox_Front",
    "TallBox_Back",
    "TallBox_Left",
    "TallBox_Right",
]
SHORT_BOX_PARTS = [
    "ShortBox_Bottom",
    "ShortBox_Top",
    "ShortBox_Front",
    "ShortBox_Back",
    "ShortBox_Left",
    "ShortBox_Right",
]


def reference_empty_cornell_box(stage: Usd.Stage, prim_path: str):
    """
    References the shared cornellBox.usda asset into the stage, deactivates
    its tall/short box parts, and overrides its wall and light material colors.

    Args:
        stage: The USD stage to reference the Cornell box asset into.
        prim_path: Path at which to define the referencing Xform prim.
    """
    prim = stage.DefinePrim(prim_path, "Xform")
    prim.GetReferences().AddReference(str(CORNELL_BOX_ASSET))

    for part_name in TALL_BOX_PARTS + SHORT_BOX_PARTS:
        part_prim = stage.GetPrimAtPath(f"{prim_path}/{part_name}")
        if part_prim.IsValid():
            part_prim.SetActive(False)

    for wall_material_name in ("RedMaterial", "GreenMaterial"):
        shader_prim = stage.GetPrimAtPath(f"{prim_path}/Materials/{wall_material_name}/Shader")
        if shader_prim.IsValid():
            UsdShade.Shader(shader_prim).CreateInput(
                "diffuseColor", Sdf.ValueTypeNames.Color3f
            ).Set(Gf.Vec3f(*CORNELL_WALL_COLOR))

    light_shader_prim = stage.GetPrimAtPath(f"{prim_path}/Materials/LightMaterial/Shader")

    if light_shader_prim.IsValid():
        UsdShade.Shader(light_shader_prim).CreateInput(
            "emissiveColor", Sdf.ValueTypeNames.Color3f
        ).Set(
            Gf.Vec3f(
                CORNELL_LIGHT_EMISSIVE_STRENGTH,
                CORNELL_LIGHT_EMISSIVE_STRENGTH,
                CORNELL_LIGHT_EMISSIVE_STRENGTH,
            )
        )

    _add_mirror_panel(stage, prim_path)

    return prim


def _add_mirror_panel(stage: Usd.Stage, prim_path: str):
    """
    Adds a mirror panel on the right wall of the Cornell box.

    Args:
        stage: The USD stage to define the mirror on.
        prim_path: Path of the Cornell box prim to nest the mirror under.
    """
    mirror_material_path = f"{prim_path}/Materials/MirrorMaterial"
    material = UsdShade.Material.Define(stage, mirror_material_path)
    shader = UsdShade.Shader.Define(stage, f"{mirror_material_path}/Shader")
    shader.CreateIdAttr("UsdPreviewSurface")
    shader.CreateInput("diffuseColor", Sdf.ValueTypeNames.Color3f).Set(Gf.Vec3f(0.95, 0.95, 0.95))
    shader.CreateInput("metallic", Sdf.ValueTypeNames.Float).Set(1.0)
    shader.CreateInput("roughness", Sdf.ValueTypeNames.Float).Set(0.0)
    material.CreateSurfaceOutput().ConnectToSource(shader.ConnectableAPI(), "surface")

    mirror_mesh = UsdGeom.Mesh.Define(stage, f"{prim_path}/MirrorPanel")
    mirror_mesh.CreatePointsAttr(
        [
            Gf.Vec3f(4.98, 1.0, -4.5),
            Gf.Vec3f(4.98, 1.0, 4.5),
            Gf.Vec3f(4.98, 8.5, 4.5),
            Gf.Vec3f(4.98, 8.5, -4.5),
        ]
    )
    mirror_mesh.CreateFaceVertexCountsAttr([4])
    mirror_mesh.CreateFaceVertexIndicesAttr([0, 1, 2, 3])
    mirror_mesh.CreateOrientationAttr(UsdGeom.Tokens.rightHanded)
    UsdShade.MaterialBindingAPI.Apply(mirror_mesh.GetPrim()).Bind(material)
