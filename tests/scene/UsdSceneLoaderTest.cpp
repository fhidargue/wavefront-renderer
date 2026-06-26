#include <gtest/gtest.h>
#include <scene/UsdSceneLoader.h>
#include <core/Scene.h>
#include <fstream>
#include <string>

using std::string;

static string writeTempUsda(const string& filename, const string& content)
{
    string path = "/tmp/" + filename;
    std::ofstream file(path);
    file << content;
    file.close();

    return path;
}

// Minimal Cornell box USD scene
static string minimalScene()
{
    return R"(#usda 1.0
(
    defaultPrim = "Root"
    upAxis = "Y"
)

def Xform "Root"
{
    def Scope "Materials"
    {
        def Material "DiffuseMat"
        {
            token outputs:surface.connect = </Root/Materials/DiffuseMat/Shader.outputs:surface>
            def Shader "Shader"
            {
                uniform token info:id = "UsdPreviewSurface"
                color3f inputs:diffuseColor = (0.8, 0.2, 0.2)
                float inputs:roughness = 1.0
                token outputs:surface
            }
        }

        def Material "EmissiveMat"
        {
            token outputs:surface.connect = </Root/Materials/EmissiveMat/Shader.outputs:surface>
            def Shader "Shader"
            {
                uniform token info:id = "UsdPreviewSurface"
                color3f inputs:diffuseColor = (1.0, 1.0, 1.0)
                color3f inputs:emissiveColor = (1.0, 1.0, 1.0)
                float inputs:roughness = 1.0
                token outputs:surface
            }
        }
    }

    def Mesh "Floor" (apiSchemas = ["MaterialBindingAPI"])
    {
        point3f[] points = [(-1, 0, -1), (1, 0, -1), (1, 0, 1), (-1, 0, 1)]
        int[] faceVertexCounts = [4]
        int[] faceVertexIndices = [0, 1, 2, 3]
        uniform token orientation = "rightHanded"
        rel material:binding = </Root/Materials/DiffuseMat>
    }

    def Mesh "Light" (apiSchemas = ["MaterialBindingAPI"])
    {
        point3f[] points = [(-0.5, 2, -0.5), (0.5, 2, -0.5), (0.5, 2, 0.5), (-0.5, 2, 0.5)]
        int[] faceVertexCounts = [4]
        int[] faceVertexIndices = [0, 1, 2, 3]
        uniform token orientation = "rightHanded"
        rel material:binding = </Root/Materials/EmissiveMat>
    }
}
)";
}

TEST(UsdSceneLoaderTest, InvalidFileReturnsEmptyScene)
{
    Scene scene = UsdSceneLoader::load("/nonexistent/path/scene.usda");

    EXPECT_EQ(scene.meshes.size(), 0u);
    EXPECT_EQ(scene.materials.size(), 0u);
}

TEST(UsdSceneLoaderTest, LoadsCorrectNumberOfMaterials)
{
    string path = writeTempUsda("materialsTest.usda", minimalScene());
    Scene scene = UsdSceneLoader::load(path);

    EXPECT_EQ(scene.materials.size(), 2u);
}

TEST(UsdSceneLoaderTest, DetectsEmissiveMaterial)
{
    string path = writeTempUsda("emissiveTest.usda", minimalScene());
    Scene scene = UsdSceneLoader::load(path);

    bool foundEmissive = false;

    for (const auto& material : scene.materials)
    {
        if (material.type == MaterialType::Emissive)
            foundEmissive = true;
    }

    EXPECT_TRUE(foundEmissive);
}

TEST(UsdSceneLoaderTest, DetectsDiffuseMaterial)
{
    string path = writeTempUsda("diffuseTest.usda", minimalScene());
    Scene scene = UsdSceneLoader::load(path);

    bool foundDiffuse = false;

    for (const auto& mat : scene.materials)
    {
        if (mat.type == MaterialType::Diffuse)
            foundDiffuse = true;
    }

    EXPECT_TRUE(foundDiffuse);
}

TEST(UsdSceneLoaderTest, LoadsCorrectNumberOfMeshes)
{
    string path = writeTempUsda("meshesTest.usda", minimalScene());
    Scene scene = UsdSceneLoader::load(path);

    EXPECT_EQ(scene.meshes.size(), 2u);
}

TEST(UsdSceneLoaderTest, QuadTriangulatesIntoTwoTriangles)
{
    string path = writeTempUsda("triangulationTest.usda", minimalScene());
    Scene scene = UsdSceneLoader::load(path);

    // Each quad mesh should have 2 triangles and 6 indices
    for (const auto& mesh : scene.meshes)
    {
        EXPECT_EQ(mesh.triangleIndices.size(), 6u);
        EXPECT_EQ(mesh.triangleCount(), 2u);
    }
}

TEST(UsdSceneLoaderTest, VertexCountMatchesUsdPoints)
{
    string path = writeTempUsda("verticesTest.usda", minimalScene());
    Scene scene = UsdSceneLoader::load(path);

    // Each mesh has 4 points in the USD
    for (const auto& mesh : scene.meshes)
        EXPECT_EQ(mesh.vertexPositions.size(), 4u);
}

TEST(UsdSceneLoaderTest, MeshMaterialBindingResolved)
{
    string path = writeTempUsda("bindingTest.usda", minimalScene());
    Scene scene = UsdSceneLoader::load(path);

    ASSERT_EQ(scene.meshes.size(), 2u);

    // Both meshes should have valid material IDs
    for (const auto& mesh : scene.meshes)
    {
        EXPECT_GE(mesh.materialID, 0);
        EXPECT_LT(mesh.materialID, static_cast<int>(scene.materials.size()));
    }
}

TEST(UsdSceneLoaderTest, FloorMeshHasDiffuseMaterial)
{
    string path = writeTempUsda("floorMaterialTest.usda", minimalScene());
    Scene scene = UsdSceneLoader::load(path);

    ASSERT_EQ(scene.meshes.size(), 2u);

    int floorMatID = scene.meshes[0].materialID;

    EXPECT_EQ(scene.materials[floorMatID].type, MaterialType::Diffuse);
}

TEST(UsdSceneLoaderTest, LightMeshHasEmissiveMaterial)
{
    string path = writeTempUsda("lightMaterialTest.usda", minimalScene());
    Scene scene = UsdSceneLoader::load(path);

    ASSERT_EQ(scene.meshes.size(), 2u);

    int lightMatID = scene.meshes[1].materialID;

    EXPECT_EQ(scene.materials[lightMatID].type, MaterialType::Emissive);
}