#!/usr/bin/env python3
import random
from pathlib import Path

from pxr import Usd, UsdGeom

from constants import SCENES_DIR, SCENE_RANDOM_SEED, OBJECT_COUNT
from utils.textures import generate_texture_pool
from utils.materials import build_material_pool, MaterialRecipe
from utils.usd_materials import add_materials_scope
from utils.geometry import populate_teapot_grid, populate_mixed_grid
from utils.cornell_reference import reference_empty_cornell_box

ROOT_PRIM_NAME = "StressTest"
CORNELL_BOX_PRIM_NAME = "CornellBox"
MATERIALS_SCOPE_NAME = "GridMaterials"
GEOMETRY_SCOPE_NAME = "Grid"


def create_stage(output_path: Path, doc: str) -> Usd.Stage:
    if output_path.exists():
        output_path.unlink()

    stage = Usd.Stage.CreateNew(str(output_path))
    UsdGeom.SetStageUpAxis(stage, UsdGeom.Tokens.y)

    root = stage.DefinePrim(f"/{ROOT_PRIM_NAME}", "Xform")
    stage.SetDefaultPrim(root)
    stage.SetMetadata("documentation", doc)

    return stage


def build_teapot_scene(
    materials: list[MaterialRecipe], material_names_by_cell: list[str]
) -> Path:
    output_path = SCENES_DIR / "stressTestTeapots.usda"
    stage = create_stage(
        output_path,
        "Stress-test scene: a grid of teapots inside the existing Cornell "
        "box, each teapot with a unique material. Isolates shading/material " \
        "incoherence with no geometry variance.",
    )
    root_path = f"/{ROOT_PRIM_NAME}"

    reference_empty_cornell_box(stage, f"{root_path}/{CORNELL_BOX_PRIM_NAME}")
    material_paths = add_materials_scope(
        stage, f"{root_path}/{MATERIALS_SCOPE_NAME}", materials
    )
    populate_teapot_grid(
        stage, f"{root_path}/{GEOMETRY_SCOPE_NAME}", material_paths, material_names_by_cell
    )

    stage.GetRootLayer().Save()
    return output_path


def build_mixed_scene(
    materials: list[MaterialRecipe],
    material_names_by_cell: list[str],
    rng: random.Random,
) -> Path:
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
    rng = random.Random(SCENE_RANDOM_SEED)

    texture_pool = generate_texture_pool(rng)
    print(f"Generated {len(texture_pool)} textures")

    materials = build_material_pool(OBJECT_COUNT, texture_pool, rng)
    material_names_by_cell = [m.name for m in materials]
    print(f"Generated {len(materials)} materials")

    teapot_path = build_teapot_scene(materials, material_names_by_cell)
    print(f"Wrote {teapot_path}")

    mixed_path = build_mixed_scene(materials, material_names_by_cell, rng)
    print(f"Wrote {mixed_path}")


if __name__ == "__main__":
    main()