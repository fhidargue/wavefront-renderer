from pxr import Usd, UsdShade, Sdf, Gf

from constants import (
    CORNELL_BOX_ASSET,
    CORNELL_WALL_COLOR,
    CORNELL_LIGHT_EMISSIVE_STRENGTH,
)


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
        shader_prim = stage.GetPrimAtPath(
            f"{prim_path}/Materials/{wall_material_name}/Shader"
        )
        if shader_prim.IsValid():
            UsdShade.Shader(shader_prim).CreateInput(
                "diffuseColor", Sdf.ValueTypeNames.Color3f
            ).Set(Gf.Vec3f(*CORNELL_WALL_COLOR))

    light_shader_prim = stage.GetPrimAtPath(
        f"{prim_path}/Materials/LightMaterial/Shader"
    )

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

    return prim
