from pxr import Usd

from constants import CORNELL_BOX_ASSET

TALL_BOX_PARTS = [
    "TallBox_Bottom", "TallBox_Top", "TallBox_Front",
    "TallBox_Back", "TallBox_Left", "TallBox_Right",
]
SHORT_BOX_PARTS = [
    "ShortBox_Bottom", "ShortBox_Top", "ShortBox_Front",
    "ShortBox_Back", "ShortBox_Left", "ShortBox_Right",
]


def reference_empty_cornell_box(stage: Usd.Stage, prim_path: str):
    """References cornellBox.usda"""
    prim = stage.DefinePrim(prim_path, "Xform")
    prim.GetReferences().AddReference(str(CORNELL_BOX_ASSET))

    for part_name in TALL_BOX_PARTS + SHORT_BOX_PARTS:
        part_prim = stage.GetPrimAtPath(f"{prim_path}/{part_name}")
        if part_prim.IsValid():
            part_prim.SetActive(False)

    return prim