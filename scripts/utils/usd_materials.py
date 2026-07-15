from pxr import Usd, UsdShade, UsdGeom, Sdf, Gf

from .materials import MaterialRecipe

UV_PRIMVAR_NAME = "st"


def add_materials_scope(
    stage: Usd.Stage, scope_path: str, materials: list[MaterialRecipe]
) -> dict:
    """
    Defines a Scope of UsdPreviewSurface materials from the given recipes, wiring up textures
    and kind-specific inputs (metallic, roughness, ior, checker), and returns a mapping from material name to its prim path.

    Args:
        stage: The USD stage to define the materials scope and materials on.
        scope_path: Prim path at which to define the Scope containing the materials.
        materials: Material recipes describing each material's kind, color, and other kind-specific properties to build.
    """
    UsdGeom.Scope.Define(stage, scope_path)
    material_paths = {}

    for recipe in materials:
        material_path = f"{scope_path}/{recipe.name}"
        material = UsdShade.Material.Define(stage, material_path)

        shader = UsdShade.Shader.Define(stage, f"{material_path}/PreviewSurface")
        shader.CreateIdAttr("UsdPreviewSurface")
        material.CreateSurfaceOutput().ConnectToSource(
            shader.ConnectableAPI(), "surface"
        )

        diffuse_input = shader.CreateInput("diffuseColor", Sdf.ValueTypeNames.Color3f)

        if recipe.texture_path:
            # Standard UsdUVTexture
            st_reader = UsdShade.Shader.Define(stage, f"{material_path}/stReader")
            st_reader.CreateIdAttr("UsdPrimvarReader_float2")
            st_reader.CreateInput("varname", Sdf.ValueTypeNames.Token).Set(
                UV_PRIMVAR_NAME
            )
            st_output = st_reader.CreateOutput("result", Sdf.ValueTypeNames.Float2)

            texture = UsdShade.Shader.Define(stage, f"{material_path}/DiffuseTexture")
            texture.CreateIdAttr("UsdUVTexture")
            texture.CreateInput("file", Sdf.ValueTypeNames.Asset).Set(
                Sdf.AssetPath(recipe.texture_path)
            )
            texture.CreateInput("st", Sdf.ValueTypeNames.Float2).ConnectToSource(
                st_output
            )
            texture_rgb = texture.CreateOutput("rgb", Sdf.ValueTypeNames.Float3)

            diffuse_input.ConnectToSource(texture_rgb)
        else:
            diffuse_input.Set(Gf.Vec3f(*recipe.color))

        if recipe.kind in ("metal", "metal_tex"):
            shader.CreateInput("metallic", Sdf.ValueTypeNames.Float).Set(1.0)
            shader.CreateInput("roughness", Sdf.ValueTypeNames.Float).Set(
                recipe.roughness
            )
        elif recipe.kind in ("plastic", "plastic_tex"):
            shader.CreateInput("plastic", Sdf.ValueTypeNames.Bool).Set(True)
            shader.CreateInput("roughness", Sdf.ValueTypeNames.Float).Set(
                recipe.roughness
            )
        elif recipe.kind == "glass":
            shader.CreateInput("ior", Sdf.ValueTypeNames.Float).Set(recipe.ior)
        elif recipe.kind == "checker":
            shader.CreateInput("spatialChecker", Sdf.ValueTypeNames.Bool).Set(True)
            shader.CreateInput("spatialCheckerCellSize", Sdf.ValueTypeNames.Float).Set(
                recipe.checker_cell_size
            )
        # "diffuse"/"diffuse_tex" need nothing beyond diffuseColor

        material_paths[recipe.name] = Sdf.Path(material_path)

    return material_paths
