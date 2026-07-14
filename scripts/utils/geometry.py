from pxr import Usd, UsdGeom, UsdShade, Gf, Sdf

from constants import (
    TEAPOT_ASSET, DRAGON_ASSET, GRID_ROWS, GRID_COLS, GRID_LAYERS,
    DRAGON_FRACTION_IN_MIXED_SCENE, ROOM_MIN_X, ROOM_MAX_X, ROOM_MIN_Z,
    ROOM_MAX_Z, ROOM_MIN_Y, ROOM_MAX_Y, GRID_WALL_MARGIN, OBJECT_FILL_FACTOR,
)


def compute_asset_footprint(asset_path) -> Gf.Vec3d:
    scratch_stage = Usd.Stage.CreateInMemory()
    prim = scratch_stage.DefinePrim("/Probe")
    prim.GetReferences().AddReference(str(asset_path))
    cache = UsdGeom.BBoxCache(Usd.TimeCode.Default(), [UsdGeom.Tokens.default_], useExtentsHint=False)
    bound_range = cache.ComputeWorldBound(prim).ComputeAlignedRange()
    return bound_range.GetMax() - bound_range.GetMin()


def compute_grid_cell_size() -> Gf.Vec3d:
    available_width = (ROOM_MAX_X - ROOM_MIN_X) - 2 * GRID_WALL_MARGIN
    available_depth = (ROOM_MAX_Z - ROOM_MIN_Z) - 2 * GRID_WALL_MARGIN
    available_height = (ROOM_MAX_Y - ROOM_MIN_Y) - 2 * GRID_WALL_MARGIN

    return Gf.Vec3d(
        available_width / GRID_COLS,
        available_height / GRID_LAYERS,
        available_depth / GRID_ROWS,
    )


def compute_uniform_scale_to_fit(footprint: Gf.Vec3d, cell_size: Gf.Vec3d) -> float:
    limiting_ratio = min(
        cell_size[0] / footprint[0],
        cell_size[1] / footprint[1],
        cell_size[2] / footprint[2],
    )
    return limiting_ratio * OBJECT_FILL_FACTOR


def build_grid_cell_positions(rows: int, cols: int, layers: int, cell_size: Gf.Vec3d):
    room_center_x = (ROOM_MIN_X + ROOM_MAX_X) / 2.0
    room_center_z = (ROOM_MIN_Z + ROOM_MAX_Z) / 2.0
    grid_width = cell_size[0] * cols
    grid_depth = cell_size[2] * rows

    origin_x = room_center_x - grid_width / 2.0
    origin_z = room_center_z - grid_depth / 2.0
    origin_y = ROOM_MIN_Y + GRID_WALL_MARGIN

    for layer in range(layers):
        for row in range(rows):
            for col in range(cols):
                x = origin_x + (col + 0.5) * cell_size[0]
                z = origin_z + (row + 0.5) * cell_size[2]
                y = origin_y + layer * cell_size[1]
                yield row, col, layer, x, y, z


def add_grid_instance(stage: Usd.Stage, path: str, asset_path, x: float, y: float, z: float,
                       scale: float, material_path: Sdf.Path):
    prim = stage.DefinePrim(path, "Xform")
    prim.GetReferences().AddReference(str(asset_path))

    xformable = UsdGeom.Xformable(prim)
    xformable.ClearXformOpOrder()
    xformable.AddTranslateOp().Set(Gf.Vec3d(x, y, z))
    xformable.AddScaleOp().Set(Gf.Vec3f(scale, scale, scale))

    # Bind the Material API
    binding_api = UsdShade.MaterialBindingAPI.Apply(prim)
    binding_api.Bind(UsdShade.Material(stage.GetPrimAtPath(material_path)))

    return prim


def populate_teapot_grid(stage: Usd.Stage, root_path: str, material_paths: dict,
                         material_names_by_cell: list):
    cell_size = compute_grid_cell_size()
    teapot_footprint = compute_asset_footprint(TEAPOT_ASSET)
    scale = compute_uniform_scale_to_fit(teapot_footprint, cell_size)

    for (row, col, layer, x, y, z), name in zip(
        build_grid_cell_positions(GRID_ROWS, GRID_COLS, GRID_LAYERS, cell_size),
        material_names_by_cell,
    ):
        path = f"{root_path}/Teapot_{layer:02d}_{row:02d}_{col:02d}"
        add_grid_instance(stage, path, TEAPOT_ASSET, x, y, z, scale=scale,
                          material_path=material_paths[name])


def populate_mixed_grid(stage: Usd.Stage, root_path: str, material_paths: dict,
                        material_names_by_cell: list, rng):
    cell_size = compute_grid_cell_size()
    teapot_footprint = compute_asset_footprint(TEAPOT_ASSET)
    dragon_footprint = compute_asset_footprint(DRAGON_ASSET)
    teapot_scale = compute_uniform_scale_to_fit(teapot_footprint, cell_size)
    dragon_scale = compute_uniform_scale_to_fit(dragon_footprint, cell_size)

    for (row, col, layer, x, y, z), name in zip(
        build_grid_cell_positions(GRID_ROWS, GRID_COLS, GRID_LAYERS, cell_size),
        material_names_by_cell,
    ):
        is_dragon = rng.random() < DRAGON_FRACTION_IN_MIXED_SCENE
        asset_path = DRAGON_ASSET if is_dragon else TEAPOT_ASSET
        scale = dragon_scale if is_dragon else teapot_scale
        prefix = "Dragon" if is_dragon else "Teapot"

        path = f"{root_path}/{prefix}_{layer:02d}_{row:02d}_{col:02d}"
        add_grid_instance(stage, path, asset_path, x, y, z, scale=scale,
                          material_path=material_paths[name])