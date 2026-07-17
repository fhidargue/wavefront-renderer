#!/usr/bin/env python3
import random
from pathlib import Path

from pxr import Usd, UsdGeom

from constants import SCENES_DIR, OBJECT_COUNT
from utils.materials import build_material_pool, MaterialRecipe
from utils.textures import estimate_resident_texture_bytes
from utils.usd_materials import add_materials_scope
from utils.geometry import populate_dragon_grid, populate_mixed_grid
from utils.cornell_reference import reference_empty_cornell_box

ROOT_PRIM_NAME = "StressTest"
CORNELL_BOX_PRIM_NAME = "CornellBox"
MATERIALS_SCOPE_NAME = "GridMaterials"
GEOMETRY_SCOPE_NAME = "Grid"


def create_stage(output_path: Path, doc: str) -> Usd.Stage:
    """
    Creates a fresh USD stage at the given path with a documented root Xform prim set as the
    default prim.

    Args:
        output_path: Filesystem path where the new .usda stage should be written, replacing any existing file.
        doc: Documentation string to store as stage metadata describing the scene's purpose.
    """
    if output_path.exists():
        output_path.unlink()

    stage = Usd.Stage.CreateNew(str(output_path))
    UsdGeom.SetStageUpAxis(stage, UsdGeom.Tokens.y)

    root = stage.DefinePrim(f"/{ROOT_PRIM_NAME}", "Xform")
    stage.SetDefaultPrim(root)
    stage.SetMetadata("documentation", doc)

    return stage


def build_dragon_scene(
    materials: list[MaterialRecipe], material_names_by_cell: list[str]
) -> Path:
    """
    Builds and saves a stress-test scene of a dragon grid with per-cell unique materials inside a
    reference Cornell box.

    Args:
        materials: Pool of material recipes to bind into the scene's materials scope.
        material_names_by_cell: Material name assigned to each grid cell, in cell order.
    """
    output_path = SCENES_DIR / "stressTestDragons.usda"
    stage = create_stage(
        output_path,
        "Stress-test scene: a grid of teapots inside the existing Cornell "
        "box, each teapot with a unique material. Isolates shading/material "
        "incoherence with no geometry variance.",
    )
    root_path = f"/{ROOT_PRIM_NAME}"

    reference_empty_cornell_box(stage, f"{root_path}/{CORNELL_BOX_PRIM_NAME}")
    material_paths = add_materials_scope(
        stage, f"{root_path}/{MATERIALS_SCOPE_NAME}", materials
    )
    populate_dragon_grid(
        stage,
        f"{root_path}/{GEOMETRY_SCOPE_NAME}",
        material_paths,
        material_names_by_cell,
    )

    stage.GetRootLayer().Save()

    return output_path


def build_mixed_scene(
    materials: list[MaterialRecipe],
    material_names_by_cell: list[str],
    rng: random.Random,
) -> Path:
    """
    Builds and saves a stress-test scene mixing teapots and dragons inside a reference Cornell box to add
    geometric complexity variance on top of material incoherence.

    Args:
        materials: Pool of material recipes to bind into the scene's materials scope.
        material_names_by_cell: Material name assigned to each grid cell, in cell order.
        rng: Seeded random number generator used to decide geometry placement/mix within the grid.
    """
    output_path = SCENES_DIR / "stressTestMixed.usda"
    stage = create_stage(
        output_path,
        "Stress-test scene: grid of teapots and dragons inside the "
        "existing Cornell box (same material assignment as "
        "stressTestTeapots.usda), adding geometric complexity variance "
        "on top of material/texture incoherence.",
    )
    root_path = f"/{ROOT_PRIM_NAME}"

    reference_empty_cornell_box(stage, f"{root_path}/{CORNELL_BOX_PRIM_NAME}")
    material_paths = add_materials_scope(
        stage, f"{root_path}/{MATERIALS_SCOPE_NAME}", materials
    )
    populate_mixed_grid(
        stage,
        f"{root_path}/{GEOMETRY_SCOPE_NAME}",
        material_paths,
        material_names_by_cell,
        rng,
    )

    stage.GetRootLayer().Save()

    return output_path


def main():
    """
    Generates the shared material pool, then builds and writes both the dragon-only and mixed stress-test scenes,
    reporting stats along the way.

    Args:
        None (parameters are drawn internally from constants and a seeded RNG).
    """
    seed = random.SystemRandom().randrange(2**32)
    rng = random.Random(seed)

    materials = build_material_pool(OBJECT_COUNT, rng)
    material_names_by_cell = [m.name for m in materials]
    resident_bytes = estimate_resident_texture_bytes(materials)

    print(
        f"Generated {len(materials)} materials ({sum(1 for m in materials if m.texture_path)} textured)"
    )
    print(f"Estimated resident texture memory: {resident_bytes / (1024**3):.2f} GB")

    dragon_path = build_dragon_scene(materials, material_names_by_cell)
    print(f"Wrote {dragon_path}")

    mixed_path = build_mixed_scene(materials, material_names_by_cell, rng)
    print(f"Wrote {mixed_path}")


if __name__ == "__main__":
    main()
