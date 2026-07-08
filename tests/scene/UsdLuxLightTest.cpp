#ifdef ENABLE_USD

#include <gtest/gtest.h>
#include <scene/UsdSceneLoader.h>
#include <core/Scene.h>
#include <fstream>
#include <string>

#include <OpenImageIO/imageio.h>

using std::string;

static string writeTempUsda(const string& filename, const string& content)
{
    string path = "/tmp/" + filename;
    std::ofstream file(path);
    file << content;
    file.close();

    return path;
}

static string writeSolidColorImage(const string& filename, int width, int height, float r, float g,
                                   float b)
{
    string path = "/tmp/" + filename;

    auto out = OIIO::ImageOutput::create(path);
    OIIO::ImageSpec spec(width, height, 3, OIIO::TypeDesc::FLOAT);
    out->open(path, spec);

    std::vector<float> pixels(static_cast<size_t>(width) * height * 3);
    for (size_t i = 0; i < pixels.size(); i += 3)
    {
        pixels[i + 0] = r;
        pixels[i + 1] = g;
        pixels[i + 2] = b;
    }

    out->write_image(OIIO::TypeDesc::FLOAT, pixels.data());
    out->close();

    return path;
}

TEST(UsdLuxLightTest, RectLightBecomesEmissiveMesh)
{
    string content = R"(#usda 1.0
(
    defaultPrim = "Root"
    upAxis = "Y"
)

def Xform "Root"
{
    def RectLight "CeilingLight"
    {
        float inputs:width = 1.0
        float inputs:height = 1.0
        float inputs:intensity = 500.0
        color3f inputs:color = (1.0, 1.0, 1.0)
        double3 xformOp:translate = (0, 2, 0)
        uniform token[] xformOpOrder = ["xformOp:translate"]
    }
}
)";
    string path = writeTempUsda("rectLightTest.usda", content);
    Scene scene = UsdSceneLoader::load(path);

    ASSERT_EQ(scene.meshes.size(), 1u);

    int materialID = scene.meshes[0].materialID;
    EXPECT_EQ(scene.materials[materialID].type, MaterialType::Emissive);
}

TEST(UsdLuxLightTest, RectLightIntensityIsCarriedToMaterial)
{
    string content = R"(#usda 1.0
(
    defaultPrim = "Root"
    upAxis = "Y"
)

def Xform "Root"
{
    def RectLight "CeilingLight"
    {
        float inputs:width = 1.0
        float inputs:height = 1.0
        float inputs:intensity = 750.0
        color3f inputs:color = (1.0, 1.0, 1.0)
    }
}
)";
    string path = writeTempUsda("rectLightIntensityTest.usda", content);
    Scene scene = UsdSceneLoader::load(path);

    bool foundEmissiveWithIntensity = false;

    for (const auto& material : scene.materials)
    {
        if (material.type == MaterialType::Emissive && material.emitted().x > 0.0f)
        {
            foundEmissiveWithIntensity = true;
            break;
        }
    }

    EXPECT_TRUE(foundEmissiveWithIntensity);
}

TEST(UsdLuxLightTest, DiskLightBecomesEmissiveMesh)
{
    string content = R"(#usda 1.0
(
    defaultPrim = "Root"
    upAxis = "Y"
)

def Xform "Root"
{
    def DiskLight "PanelLight"
    {
        float inputs:radius = 0.5
        float inputs:intensity = 300.0
        color3f inputs:color = (1.0, 1.0, 1.0)
    }
}
)";
    string path = writeTempUsda("diskLightTest.usda", content);
    Scene scene = UsdSceneLoader::load(path);

    ASSERT_EQ(scene.meshes.size(), 1u);

    int materialID = scene.meshes[0].materialID;
    EXPECT_EQ(scene.materials[materialID].type, MaterialType::Emissive);
    EXPECT_GT(scene.meshes[0].triangleCount(), 0u);
}

TEST(UsdLuxLightTest, SphereLightBecomesEmissiveMesh)
{
    string content = R"(#usda 1.0
(
    defaultPrim = "Root"
    upAxis = "Y"
)

def Xform "Root"
{
    def SphereLight "BulbLight"
    {
        float inputs:radius = 0.3
        float inputs:intensity = 200.0
        color3f inputs:color = (1.0, 1.0, 1.0)
    }
}
)";
    string path = writeTempUsda("sphereLightTest.usda", content);
    Scene scene = UsdSceneLoader::load(path);

    ASSERT_EQ(scene.meshes.size(), 1u);

    int materialID = scene.meshes[0].materialID;
    EXPECT_EQ(scene.materials[materialID].type, MaterialType::Emissive);
}

TEST(UsdLuxLightTest, CylinderLightBecomesEmissiveMesh)
{
    string content = R"(#usda 1.0
(
    defaultPrim = "Root"
    upAxis = "Y"
)

def Xform "Root"
{
    def CylinderLight "TubeLight"
    {
        float inputs:radius = 0.05
        float inputs:length = 1.0
        float inputs:intensity = 150.0
        color3f inputs:color = (1.0, 1.0, 1.0)
    }
}
)";
    string path = writeTempUsda("cylinderLightTest.usda", content);
    Scene scene = UsdSceneLoader::load(path);

    ASSERT_EQ(scene.meshes.size(), 1u);

    int materialID = scene.meshes[0].materialID;
    EXPECT_EQ(scene.materials[materialID].type, MaterialType::Emissive);
}

TEST(UsdLuxLightTest, DistantLightSetsSceneDirectionalLight)
{
    string content = R"(#usda 1.0
(
    defaultPrim = "Root"
    upAxis = "Y"
)

def Xform "Root"
{
    def DistantLight "SunLight"
    {
        float inputs:intensity = 2.0
        color3f inputs:color = (1.0, 0.95, 0.9)
    }
}
)";
    string path = writeTempUsda("distantLightTest.usda", content);
    Scene scene = UsdSceneLoader::load(path);

    EXPECT_GT(scene.directionalLight.intensity, 0.0f);
    EXPECT_NEAR(scene.directionalLight.direction.length(), 1.0f, 0.001f);
}

TEST(UsdLuxLightTest, SceneWithoutDistantLightHasZeroIntensity)
{
    string content = R"(#usda 1.0
(
    defaultPrim = "Root"
    upAxis = "Y"
)

def Xform "Root"
{
    def Mesh "Floor"
    {
        point3f[] points = [(-1, 0, -1), (1, 0, -1), (1, 0, 1), (-1, 0, 1)]
        int[] faceVertexCounts = [4]
        int[] faceVertexIndices = [0, 1, 2, 3]
    }
}
)";
    string path = writeTempUsda("noDistantLightTest.usda", content);
    Scene scene = UsdSceneLoader::load(path);

    EXPECT_FLOAT_EQ(scene.directionalLight.intensity, 0.0f);
}

TEST(UsdLuxLightTest, SceneWithoutDomeLightHasNoEnvironmentMap)
{
    string content = R"(#usda 1.0
(
    defaultPrim = "Root"
    upAxis = "Y"
)

def Xform "Root"
{
    def Mesh "Floor"
    {
        point3f[] points = [(-1, 0, -1), (1, 0, -1), (1, 0, 1), (-1, 0, 1)]
        int[] faceVertexCounts = [4]
        int[] faceVertexIndices = [0, 1, 2, 3]
    }
}
)";
    string path = writeTempUsda("noDomeLightTest.usda", content);
    Scene scene = UsdSceneLoader::load(path);

    EXPECT_FALSE(scene.environmentMap.isLoaded());
}

// Dome Light tests

TEST(UsdLuxLightTest, DomeLightLoadsEnvironmentMap)
{
    string texturePath = writeSolidColorImage("domeLightTexture.exr", 8, 4, 0.6f, 0.3f, 0.1f);

    string content = R"(#usda 1.0
(
    defaultPrim = "Root"
    upAxis = "Y"
)

def Xform "Root"
{
    def DomeLight "SkyLight"
    {
        asset inputs:texture:file = @)" +
                     texturePath + R"(@
        float inputs:intensity = 1.5
        color3f inputs:color = (1.0, 1.0, 1.0)
    }
}
)";
    string path = writeTempUsda("domeLightTest.usda", content);
    Scene scene = UsdSceneLoader::load(path);

    ASSERT_TRUE(scene.environmentMap.isLoaded());

    Color sampled = scene.environmentMap.sample(Vec3(0.0f, 1.0f, 0.0f));

    EXPECT_NEAR(sampled.x, 0.9f, 0.05f);
    EXPECT_NEAR(sampled.y, 0.45f, 0.05f);
    EXPECT_NEAR(sampled.z, 0.15f, 0.05f);
}

TEST(UsdLuxLightTest, DomeLightWithoutTextureDoesNotLoadEnvironmentMap)
{
    string content = R"(#usda 1.0
(
    defaultPrim = "Root"
    upAxis = "Y"
)

def Xform "Root"
{
    def DomeLight "SkyLight"
    {
        float inputs:intensity = 1.0
        color3f inputs:color = (1.0, 1.0, 1.0)
    }
}
)";
    string path = writeTempUsda("domeLightNoTextureTest.usda", content);
    Scene scene = UsdSceneLoader::load(path);

    EXPECT_FALSE(scene.environmentMap.isLoaded());
}

#endif