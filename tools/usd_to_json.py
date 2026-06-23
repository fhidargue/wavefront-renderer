import json
import sys

from pxr import Usd, UsdGeom, UsdShade


# Material extraction
def find_shader(stage, material):
    """
    Find the connections to get the UsdPreviewSurface shader
    """
    for output in material.GetOutputs():
        for source_list in output.GetConnectedSources():
            for connection in source_list:
                shader_prim = stage.GetPrimAtPath(connection.source.GetPath())

                if not shader_prim:
                    continue
                
                shader = UsdShade.Shader(shader_prim)

                if shader.GetIdAttr().Get() == "UsdPreviewSurface":
                    return shader
    return None

def extract_materials(stage):
    """
    Reads all materials from the stage
    """
    materials = []
    material_path_to_index = {}

    for prim in stage.Traverse():
        if not prim.IsA(UsdShade.Material):
            continue

        material = UsdShade.Material(prim)
        material_path = str(prim.GetPath())

        # Default values if we cannot find a shader
        diffuse_color = [0.8, 0.8, 0.8]
        roughness = 1.0
        emission = [0.0, 0.0, 0.0]
        is_emissive = False

        shader = find_shader(stage, material)

        if shader:
            # Read diffuse color
            diffuse_input = shader.GetInput("diffuseColor")
            if diffuse_input and diffuse_input.Get():
                color = diffuse_input.Get()
                diffuse_color = [float(color[0]), float(color[1]), float(color[2])]
            
            # Read roughness
            roughness_input = shader.GetInput("roughness")
            if roughness_input and roughness_input.Get() is not None:
                roughness = float(roughness_input.Get())

            # Read emissive
            emissive_input = shader.GetInput("emissiveColor")
            if emissive_input and emissive_input.Get():
                emission_color = emissive_input.Get()
                emission = [float(emission_color[0]), float(emission_color[1]), 
                            float(emission_color[2])]
                is_emissive = sum(emission) > 0.0
    

        # Record this material into the JSON
        index = len(materials)
        material_path_to_index[material_path] = index

        materials.append({
            "path": material_path,
            "type": "emissive" if is_emissive else "diffuse",
            "albedo": diffuse_color,
            "roughness": roughness,
            "emission": emission,
            "emissionStrength": 15.0 if is_emissive else 0.0
        })

    if not materials:
        material_path_to_index["__default__"] = 0
        materials.append({
            "path": "__default__",
            "type": "diffuse",
            "albedo": [0.8, 0.8, 0.8],
            "roughness": 1.0,
            "emission": [0.0, 0.0, 0.0],
            "emissionStrength": 0.0
        })

    return materials, material_path_to_index

# Split each USD polygon into triangles using fan triangulation
def triangulate(face_vertex_counts, face_vertex_indices):
    """
    Converts a polygon mesh to a triangle mesh
    """
    triangles = []
    index_pos = 0

    for vertex_count in face_vertex_counts:
        face = face_vertex_indices[index_pos : index_pos + vertex_count]

        for i in range(1, vertex_count - 1):
            triangles.extend([face[0], face[i], face[i + 1]])

        index_pos += vertex_count

    return triangles

# Extract all valid USD meshes
def extract_meshes(stage, material_path_to_index):
    """
    Reads all meshes from the stage
    """
    meshes = []
    xform_cache = UsdGeom.XformCache()

    for prim in stage.Traverse():
        if not prim.IsA(UsdGeom.Mesh):
            continue

        mesh_prim = UsdGeom.Mesh(prim)

        # Read vertex positions
        points_attr = mesh_prim.GetPointsAttr()
        if not points_attr or not points_attr.Get():
            print(f"WARNING: {prim.GetPath()} has no points, skipping mesh")
            continue

        # Read face topology
        indices_attr = mesh_prim.GetFaceVertexIndicesAttr()
        counts_attr = mesh_prim.GetFaceVertexCountsAttr()

        if not indices_attr or not counts_attr:
            print(f"WARNING: {prim.GetPath()} has no face data, skipping mesh")
            continue

        raw_points = points_attr.Get()
        raw_indices = list(indices_attr.Get())
        raw_counts = list(counts_attr.Get())

        # Apply world transform to every vertex
        transform = xform_cache.GetLocalToWorldTransform(prim)
        vertices = []

        for point in raw_points:
            # USD uses Vec4 for matrix multiplication
            from pxr import Gf
            world_point = Gf.Vec4d(point[0], point[1], point[2], 1.0) * transform
            vertices.append([
                round(float(world_point[0]), 6),
                round(float(world_point[1]), 6),
                round(float(world_point[2]), 6)
            ])

        # Triangulate
        triangle_indices = triangulate(raw_counts, raw_indices)

        # Find the material bound to this mesh
        binding_api = UsdShade.MaterialBindingAPI(prim)
        bound_material, _ = binding_api.ComputeBoundMaterial()
        material_id = 0  # default to first material if none found

        if bound_material:
            mat_path = str(bound_material.GetPath())
            material_id = material_path_to_index.get(mat_path, 0)

        meshes.append({
            "prim_path": str(prim.GetPath()),
            "material_id": material_id,
            "vertices": vertices,
            "indices": triangle_indices
        })

    return meshes

def main():
    if len(sys.argv) != 3:
        print("Usage: uv run python3 tools/usd_to_json.py <input.usda> <output.json>")
        sys.exit(1)

    usd_path = sys.argv[1]
    json_path = sys.argv[2]

    # Open the USD stage
    print(f"Opening: {usd_path}")
    stage = Usd.Stage.Open(usd_path)

    if not stage:
        print(f"ERROR: Could not open {usd_path}")
        sys.exit(1)

    # Extract materials
    print("Extracting materials...")
    materials, material_path_to_index = extract_materials(stage)
    print(f" Found {len(materials)} materials")

    # Extract meshes
    print("Extracting meshes...")
    meshes = extract_meshes(stage, material_path_to_index)
    print(f" Found {len(meshes)} meshes")

    # Print extraction summary
    total_triangles = sum(len(mesh["indices"]) // 3 for mesh in meshes)
    total_vertices = sum(len(mesh["vertices"]) for mesh in meshes)

    print(f"Total triangles : {total_triangles}")
    print(f"Total vertices  : {total_vertices}")

    # Write JSON
    scene_data = {
        "materials": materials,
        "meshes": meshes
    }

    with open(json_path, "w") as f:
        json.dump(scene_data, f, indent=2)

    print(f"Written to: {json_path}")


if __name__ == "__main__":
    main()