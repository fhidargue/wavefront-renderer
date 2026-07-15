from pxr import Usd, UsdGeom, UsdShade, Gf, Sdf
import itertools

from constants import (
    TEAPOT_ASSET,
    DRAGON_ASSET,
    GRID_ROWS,
    GRID_COLS,
    GRID_LAYERS,
    DRAGON_FRACTION_IN_MIXED_SCENE,
    ROOM_MIN_X,
    ROOM_MAX_X,
    ROOM_MIN_Z,
    ROOM_MAX_Z,
    ROOM_MIN_Y,
    ROOM_MAX_Y,
    GRID_WALL_MARGIN,
    OBJECT_FILL_FACTOR,
    DRAGON_OVERLAP_FACTOR,
    DRAGON_PACKING_MARGIN,
    DRAGON_Y_DROP_MULTIPLIER,
)


def compute_asset_footprint(asset_path) -> Gf.Vec3d:
    """
    Computes the world-space bounding-box size (width, height, depth) of the given
    asset by referencing it into a scratch in-memory stage.

    Args:
        asset_path: Path to the USD asset to measure.
    """
    scratch_stage = Usd.Stage.CreateInMemory()
    prim = scratch_stage.DefinePrim("/Probe")
    prim.GetReferences().AddReference(str(asset_path))
    cache = UsdGeom.BBoxCache(
        Usd.TimeCode.Default(), [UsdGeom.Tokens.default_], useExtentsHint=False
    )
    bound_range = cache.ComputeWorldBound(prim).ComputeAlignedRange()

    return bound_range.GetMax() - bound_range.GetMin()


def compute_asset_min_y(asset_path) -> float:
    """
    Computes the minimum Y (world-space) of the given asset's bounding box by
    referencing it into a scratch in-memory stage.

    Args:
        asset_path: Path to the USD asset to measure.
    """
    scratch_stage = Usd.Stage.CreateInMemory()
    prim = scratch_stage.DefinePrim("/Probe")
    prim.GetReferences().AddReference(str(asset_path))
    cache = UsdGeom.BBoxCache(
        Usd.TimeCode.Default(), [UsdGeom.Tokens.default_], useExtentsHint=False
    )

    return cache.ComputeWorldBound(prim).ComputeAlignedRange().GetMin()[1]


def compute_grid_cell_size() -> Gf.Vec3d:
    """
    Computes the (width, height, depth) size of a single grid cell by dividing the room's
    usable interior space by the grid dimensions.

    Args:
        None (dimensions are drawn from module-level room and grid constants).
    """
    available_width = (ROOM_MAX_X - ROOM_MIN_X) - 2 * GRID_WALL_MARGIN
    available_depth = (ROOM_MAX_Z - ROOM_MIN_Z) - 2 * GRID_WALL_MARGIN
    available_height = (ROOM_MAX_Y - ROOM_MIN_Y) - 2 * GRID_WALL_MARGIN

    return Gf.Vec3d(
        available_width / GRID_COLS,
        available_height / GRID_LAYERS,
        available_depth / GRID_ROWS,
    )


def compute_packed_scale_and_spacing(asset_path, cols: int, layers: int, rows: int):
    """
    Computes a uniform scale and per-axis cell spacing that tightly packs the given grid
    dimensions of the asset into the room, allowing for slight overlap.

    Args:
        asset_path: Path to the USD asset being packed into the grid.
        cols: Number of columns in the grid.
        layers: Number of vertical layers in the grid.
        rows: Number of rows in the grid.
    """
    footprint = compute_asset_footprint(asset_path)
    available_width = (ROOM_MAX_X - ROOM_MIN_X) - 2 * GRID_WALL_MARGIN
    available_depth = (ROOM_MAX_Z - ROOM_MIN_Z) - 2 * GRID_WALL_MARGIN
    available_height = (ROOM_MAX_Y - ROOM_MIN_Y) - 2 * GRID_WALL_MARGIN

    scale = (
        min(
            available_width / (cols * footprint[0] * DRAGON_PACKING_MARGIN),
            available_height / (layers * footprint[1] * DRAGON_PACKING_MARGIN),
            available_depth / (rows * footprint[2] * DRAGON_PACKING_MARGIN),
        )
        * DRAGON_OVERLAP_FACTOR
    )

    spacing = Gf.Vec3d(
        footprint[0] * scale * DRAGON_PACKING_MARGIN,
        footprint[1] * scale * DRAGON_PACKING_MARGIN,
        footprint[2] * scale * DRAGON_PACKING_MARGIN,
    )

    return scale, spacing


def compute_uniform_scale_to_fit(footprint: Gf.Vec3d, cell_size: Gf.Vec3d) -> float:
    """
    Computes the largest uniform scale factor that fits the given footprint within a cell,
    educed by a fill-factor margin.

    Args:
        footprint: The (width, height, depth) bounding-box size of the unscaled asset.
        cell_size: The (width, height, depth) size of the grid cell to fit the asset into.
    """
    limiting_ratio = min(
        cell_size[0] / footprint[0],
        cell_size[1] / footprint[1],
        cell_size[2] / footprint[2],
    )

    return limiting_ratio * OBJECT_FILL_FACTOR


def build_grid_cell_positions(rows: int, cols: int, layers: int, cell_size: Gf.Vec3d):
    """
    Yields the (row, col, layer, x, y, z) world-space center position of every cell in
    a rows x cols x layers grid centered in the room.

    Args:
        rows: Number of rows in the grid (Z axis).
        cols: Number of columns in the grid (X axis).
        layers: Number of vertical layers in the grid (Y axis).
        cell_size: The (width, height, depth) size of a single grid cell.
    """
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


def add_grid_instance(
    stage: Usd.Stage,
    path: str,
    asset_path,
    x: float,
    y: float,
    z: float,
    scale: float,
    material_path: Sdf.Path,
):
    """
    Defines a referenced, positioned, uniformly scaled prim at the given path and binds
    it to the specified material.

    Args:
        stage: The USD stage to define the instance prim on.
        path: Prim path at which to create the new instance.
        asset_path: Path to the USD asset to reference for this instance.
        x: World-space X position of the instance.
        y: World-space Y position of the instance.
        z: World-space Z position of the instance.
        scale: Uniform scale factor to apply to the instance.
        material_path: Path to the material prim to bind to the instance.
    """
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


def populate_dragon_grid(
    stage: Usd.Stage, root_path: str, material_paths: dict, material_names_by_cell: list
):
    """
    Populates a packed 3D grid of dragon instances under root_path, each dropped to rest on its
    cell floor and bound to a cycling sequence of materials.

    Args:
        stage: The USD stage to populate with dragon instances.
        root_path: Prim path under which to create the dragon instance prims.
        material_paths: Mapping from material name to its prim path.
        material_names_by_cell: Material names to cycle through and assign across grid cells.
    """
    scale, spacing = compute_packed_scale_and_spacing(
        DRAGON_ASSET, GRID_COLS, GRID_LAYERS, GRID_ROWS
    )
    dragon_min_y = compute_asset_min_y(DRAGON_ASSET)
    dragon_footprint = compute_asset_footprint(DRAGON_ASSET)
    dragon_y_drop = dragon_footprint[1] * scale * DRAGON_Y_DROP_MULTIPLIER

    for (row, col, layer, x, y, z), name in zip(
        build_grid_cell_positions(GRID_ROWS, GRID_COLS, GRID_LAYERS, spacing),
        itertools.cycle(material_names_by_cell),
    ):
        path = f"{root_path}/Dragon_{layer:02d}_{row:02d}_{col:02d}"
        placement_y = y - dragon_min_y * scale - dragon_y_drop

        add_grid_instance(
            stage,
            path,
            DRAGON_ASSET,
            x,
            placement_y,
            z,
            scale=scale,
            material_path=material_paths[name],
        )


def populate_mixed_grid(
    stage: Usd.Stage,
    root_path: str,
    material_paths: dict,
    material_names_by_cell: list,
    rng,
):
    """
    Populates a packed 3D grid under root_path with a random per-cell mix of dragon and teapot instances,
    each correctly scaled, dropped to its cell floor, and bound to a cycling sequence of materials.

    Args:
        stage: The USD stage to populate with instances.
        root_path: Prim path under which to create the instance prims.
        material_paths: Mapping from material name to its prim path.
        material_names_by_cell: Material names to cycle through and assign across grid cells.
        rng: Seeded random number generator used to decide dragon vs. teapot per cell.
    """
    dragon_scale, spacing = compute_packed_scale_and_spacing(
        DRAGON_ASSET, GRID_COLS, GRID_LAYERS, GRID_ROWS
    )
    dragon_min_y = compute_asset_min_y(DRAGON_ASSET)
    dragon_footprint = compute_asset_footprint(DRAGON_ASSET)
    dragon_y_drop = dragon_footprint[1] * dragon_scale * DRAGON_Y_DROP_MULTIPLIER

    teapot_footprint = compute_asset_footprint(TEAPOT_ASSET)
    teapot_min_y = compute_asset_min_y(TEAPOT_ASSET)
    teapot_scale = compute_uniform_scale_to_fit(teapot_footprint, spacing)

    for (row, col, layer, x, y, z), name in zip(
        build_grid_cell_positions(GRID_ROWS, GRID_COLS, GRID_LAYERS, spacing),
        itertools.cycle(material_names_by_cell),
    ):
        is_dragon = rng.random() < DRAGON_FRACTION_IN_MIXED_SCENE
        asset_path = DRAGON_ASSET if is_dragon else TEAPOT_ASSET
        scale = dragon_scale if is_dragon else teapot_scale
        min_y = dragon_min_y if is_dragon else teapot_min_y
        y_drop = dragon_y_drop if is_dragon else 0.0
        prefix = "Dragon" if is_dragon else "Teapot"

        path = f"{root_path}/{prefix}_{layer:02d}_{row:02d}_{col:02d}"
        placement_y = y - min_y * scale - y_drop

        add_grid_instance(
            stage,
            path,
            asset_path,
            x,
            placement_y,
            z,
            scale=scale,
            material_path=material_paths[name],
        )
