#!/usr/bin/env python3
import argparse
from pathlib import Path

from pxr import Usd, UsdGeom, Sdf, Gf

DEFAULT_TEXEL_WORLD_SIZE = 1.5
MIN_BBOX_EXTENT = 1e-6


def compute_face_normal(face_points: list) -> Gf.Vec3d:
    """
    Computes an unnormalized face normal via Newell's method.

    Args:
        face_points: List of Gf.Vec3d (or similar) points forming the face's vertex loop.
    """
    normal = Gf.Vec3d(0.0, 0.0, 0.0)
    count = len(face_points)

    for i in range(count):
        current = face_points[i]
        nxt = face_points[(i + 1) % count]
        normal[0] += (current[1] - nxt[1]) * (current[2] + nxt[2])
        normal[1] += (current[2] - nxt[2]) * (current[0] + nxt[0])
        normal[2] += (current[0] - nxt[0]) * (current[1] + nxt[1])

    return normal


def dominant_axis(normal: Gf.Vec3d) -> int:
    """
    Determines which axis (0=X, 1=Y, 2=Z) the given normal points along most strongly.

    Args:
        normal: The face normal vector to evaluate.
    """
    abs_normal = [abs(normal[0]), abs(normal[1]), abs(normal[2])]

    return abs_normal.index(max(abs_normal))


def normalize_to_unit_range(value: float, axis_min: float, axis_max: float) -> float:
    """
    Remaps a value into the [0, 1] range based on the given axis bounds.

    Args:
        value: The coordinate value to normalize.
        axis_min: The minimum bound of the axis range.
        axis_max: The maximum bound of the axis range.
    """
    extent = max(axis_max - axis_min, MIN_BBOX_EXTENT)

    return (value - axis_min) / extent


def compute_box_projected_uvs(
    points, face_vertex_counts, face_vertex_indices, texel_world_size: float
) -> list:
    """
    Generates face-varying box-projected UVs for a mesh by projecting each face onto its dominant bounding-box axis plane.

    Args:
        points: Sequence of mesh vertex positions.
        face_vertex_counts: Sequence giving the number of vertices per face.
        face_vertex_indices: Flat sequence of vertex indices per face, matching face_vertex_counts.
        texel_world_size: World-space size of one texture repeat, used to scale UV tiling per axis.
    """
    bbox_min = Gf.Vec3d(*[min(p[axis] for p in points) for axis in range(3)])
    bbox_max = Gf.Vec3d(*[max(p[axis] for p in points) for axis in range(3)])
    bbox_extent = bbox_max - bbox_min

    # One tile count per axis
    tile_counts = [
        max(bbox_extent[axis], MIN_BBOX_EXTENT) / texel_world_size for axis in range(3)
    ]

    face_varying_uvs = []
    cursor = 0

    for count in face_vertex_counts:
        face_indices = face_vertex_indices[cursor : cursor + count]
        face_points = [points[i] for i in face_indices]

        normal = compute_face_normal(face_points)
        axis = dominant_axis(normal)
        u_axis, v_axis = [a for a in (0, 1, 2) if a != axis]

        for point in face_points:
            u = normalize_to_unit_range(
                point[u_axis], bbox_min[u_axis], bbox_max[u_axis]
            )
            v = normalize_to_unit_range(
                point[v_axis], bbox_min[v_axis], bbox_max[v_axis]
            )
            face_varying_uvs.append(
                Gf.Vec2f(u * tile_counts[u_axis], v * tile_counts[v_axis])
            )

        cursor += count

    return face_varying_uvs


def find_mesh_prims(stage: Usd.Stage) -> list:
    """
    Collects every UsdGeomMesh prim present in the given stage.

    Args:
        stage: The USD stage to traverse.
    """
    return [prim for prim in stage.Traverse() if prim.IsA(UsdGeom.Mesh)]


def mesh_has_uvs(prim: Usd.Prim) -> bool:
    """
    Checks whether the given prim already has a defined, non-empty "st" UV primvar.

    Args:
        prim: The USD prim to check for existing UVs.
    """
    primvar = UsdGeom.PrimvarsAPI(prim).GetPrimvar("st")

    return primvar.IsDefined() and primvar.HasValue()


def generate_uvs(target_path: Path, texel_world_size: float, force: bool):
    """
    Opens the target USD asset, computes and writes box-projected UVs onto its meshes, and saves the file in place.

    Args:
        target_path: Filesystem path to the .usda asset to modify.
        texel_world_size: World-space size of one texture repeat used for UV tiling.
        force: If True, overwrite UVs on meshes that already have them.
    """
    stage = Usd.Stage.Open(str(target_path))

    if not stage:
        raise RuntimeError(f"Could not open {target_path}")

    mesh_prims = find_mesh_prims(stage)

    if not mesh_prims:
        raise RuntimeError(f"No UsdGeomMesh prims found in {target_path}")

    processed = 0
    skipped = 0

    for mesh_prim in mesh_prims:
        already_has_uvs = mesh_has_uvs(mesh_prim)

        if already_has_uvs and not force:
            skipped += 1
            continue

        mesh = UsdGeom.Mesh(mesh_prim)
        points = mesh.GetPointsAttr().Get()
        face_vertex_counts = mesh.GetFaceVertexCountsAttr().Get()
        face_vertex_indices = mesh.GetFaceVertexIndicesAttr().Get()

        if not points or not face_vertex_counts or not face_vertex_indices:
            skipped += 1
            continue

        uvs = compute_box_projected_uvs(
            points, face_vertex_counts, face_vertex_indices, texel_world_size
        )
        assert len(uvs) == len(face_vertex_indices), (
            "UV count must match faceVertexIndices count"
        )

        st_primvar = UsdGeom.PrimvarsAPI(mesh_prim).CreatePrimvar(
            "st", Sdf.ValueTypeNames.TexCoord2fArray, UsdGeom.Tokens.faceVarying
        )
        st_primvar.Set(uvs)

        print(f"Generated UVs for {mesh_prim.GetPath()}")
        processed += 1

    stage.GetRootLayer().Save()
    print(
        f"\nOverwrote {target_path}. {processed} mesh(es) processed, {skipped} skipped"
    )

    if processed == 0:
        print("WARNING: no meshes were processed")


def main():
    parser = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter
    )
    parser.add_argument(
        "target", type=Path, help="Path to the .usda asset to modify IN PLACE"
    )
    parser.add_argument(
        "--texel-size",
        type=float,
        default=DEFAULT_TEXEL_WORLD_SIZE,
        help=f"World-space size of one texture repeat (default: {DEFAULT_TEXEL_WORLD_SIZE})",
    )
    parser.add_argument(
        "--force",
        action="store_true",
        help="Overwrite UVs on meshes that already have them",
    )
    args = parser.parse_args()

    target_path = args.target.resolve()

    print(f"Target: {target_path}")
    generate_uvs(target_path, args.texel_size, args.force)


if __name__ == "__main__":
    main()
