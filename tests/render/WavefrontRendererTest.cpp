#include <gtest/gtest.h>
#include <render/WavefrontRenderer.h>
#include <core/Scene.h>
#include <core/Camera.h>
#include <core/Image.h>
#include <shading/Material.h>
#include <geometry/Mesh.h>

// Builds a test scene
static Scene buildTestScene() {
    Scene scene;

    int whiteMaterial = scene.addMaterial(
        Material::makeDiffuse(Color(0.8f, 0.8f, 0.8f)));
    int lightMaterial = scene.addMaterial(
        Material::makeEmissive(Color(1.0f, 1.0f, 1.0f), 10.0f));

    // Floor quad
    Mesh floor;
    floor.materialID = whiteMaterial;
    floor.vertexPositions = {
        Point3(-2.0f, -1.0f, -3.0f),
        Point3( 2.0f, -1.0f, -3.0f),
        Point3( 2.0f, -1.0f,  1.0f),
        Point3(-2.0f, -1.0f,  1.0f)
    };
    floor.triangleIndices = { 0, 1, 2,  0, 2, 3 };
    scene.addMesh(floor);

    // Light quad
    Mesh light;
    light.materialID = lightMaterial;
    light.vertexPositions = {
        Point3(-0.5f, 2.0f, -2.0f),
        Point3( 0.5f, 2.0f, -2.0f),
        Point3( 0.5f, 2.0f, -1.0f),
        Point3(-0.5f, 2.0f, -1.0f)
    };
    light.triangleIndices = { 0, 1, 2,  0, 2, 3 };
    scene.addMesh(light);

    scene.buildAccelerator();

    return scene;
}

static Camera buildTestCamera(int width, int height) {
    return Camera(
        Point3(0.0f, 0.5f, 2.0f),
        Point3(0.0f, 0.0f, -1.0f),
        Vec3(0.0f, 1.0f, 0.0f),
        45.0f,
        width, height
    );
}

// Constructors

TEST(WavefrontRendererTest, ConstructorStoresSamples) {
    WavefrontRenderer renderer(16, 4, SchedulingPolicy::None);

    EXPECT_EQ(renderer.samplesPerPixel, 16);
}

TEST(WavefrontRendererTest, ConstructorStoresDepth) {
    WavefrontRenderer renderer(16, 4, SchedulingPolicy::None);

    EXPECT_EQ(renderer.maxDepth, 4);
}

TEST(WavefrontRendererTest, ConstructorStoresPolicy) {
    WavefrontRenderer renderer(16, 4, SchedulingPolicy::MaterialAware);

    EXPECT_EQ(renderer.policy, SchedulingPolicy::MaterialAware);
}

// Render outputs
TEST(WavefrontRendererTest, RenderSceneWritesAllPixels) {
    const int width  = 16;
    const int height = 16;

    Scene scene = buildTestScene();
    Camera camera = buildTestCamera(width, height);
    Image image(width, height);

    std::srand(42);
    WavefrontRenderer renderer(2, 2, SchedulingPolicy::None);
    renderer.renderScene(scene, camera, image);

    int blackPixelCount = 0;
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            Color pixel = image.pixels[y * width + x];

            if (pixel.x == 0.0f && pixel.y == 0.0f && pixel.z == 0.0f)
                ++blackPixelCount;
        }
    }

    // Most pixels should not be  black
    EXPECT_LT(blackPixelCount, (width * height) / 2);
}

TEST(WavefrontRendererTest, RenderReturnsNonNegativeShadingTime) {
    Scene scene = buildTestScene();
    Camera camera = buildTestCamera(8, 8);
    Image image(8, 8);

    std::srand(42);
    WavefrontRenderer renderer(1, 2, SchedulingPolicy::None);
    double shadingMs = renderer.renderScene(scene, camera, image);

    EXPECT_GE(shadingMs, 0.0);
}

TEST(WavefrontRendererTest, PixelValuesAreInValidRange) {
    const int width = 16;
    const int height = 16;

    Scene scene = buildTestScene();
    Camera camera = buildTestCamera(width, height);
    Image image(width, height);

    std::srand(42);
    WavefrontRenderer renderer(4, 4, SchedulingPolicy::None);
    renderer.renderScene(scene, camera, image);

    // After gamma correction all values should be in [0, 1]
    for (int i = 0; i < width * height; ++i) {
        EXPECT_GE(image.pixels[i].x, 0.0f);
        EXPECT_GE(image.pixels[i].y, 0.0f);
        EXPECT_GE(image.pixels[i].z, 0.0f);
        EXPECT_TRUE(std::isfinite(image.pixels[i].x));
        EXPECT_TRUE(std::isfinite(image.pixels[i].y));
        EXPECT_TRUE(std::isfinite(image.pixels[i].z));
    }
}

// Scheduling policies

TEST(WavefrontRendererTest, MaterialAwarePolicyProducesSameValueRange) {
    const int width = 16;
    const int height = 16;

    Scene scene = buildTestScene();
    Camera camera = buildTestCamera(width, height);

    Image imageBaseline(width, height);
    Image imageMaterialAware(width, height);

    std::srand(42);
    WavefrontRenderer baselineRenderer(4, 4, SchedulingPolicy::None);
    baselineRenderer.renderScene(scene, camera, imageBaseline);

    std::srand(42);
    WavefrontRenderer materialRenderer(4, 4, SchedulingPolicy::MaterialAware);
    materialRenderer.renderScene(scene, camera, imageMaterialAware);

    // Compute average brightness of both images
    float baselineBrightness = 0.0f;
    float materialAwareBrightness = 0.0f;

    for (int i = 0; i < width * height; ++i) {
        baselineBrightness +=
            (imageBaseline.pixels[i].x +
             imageBaseline.pixels[i].y +
             imageBaseline.pixels[i].z) / 3.0f;

        materialAwareBrightness +=
            (imageMaterialAware.pixels[i].x +
             imageMaterialAware.pixels[i].y +
             imageMaterialAware.pixels[i].z) / 3.0f;
    }

    baselineBrightness /= static_cast<float>(width * height);
    materialAwareBrightness /= static_cast<float>(width * height);

    EXPECT_NEAR(baselineBrightness, materialAwareBrightness, baselineBrightness * 0.10f);
}

// Scene hit routing

TEST(SceneHitTest, WithoutAcceleratorUsesBruteForceFallback) {
    Scene scene;
    scene.addMaterial(Material::makeDiffuse(Color(0.8f, 0.8f, 0.8f)));

    Mesh quad;
    quad.materialID = 0;
    quad.vertexPositions = {
        Point3(-1.0f, -1.0f, -1.0f),
        Point3( 1.0f, -1.0f, -1.0f),
        Point3( 1.0f,  1.0f, -1.0f),
        Point3(-1.0f,  1.0f, -1.0f)
    };
    quad.triangleIndices = { 0, 1, 2,  0, 2, 3 };
    scene.addMesh(quad);

    // Don't call buildAccelerator()
    EXPECT_FALSE(scene.acceleratorBuilt);

    Ray ray(Point3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 0.0f, -1.0f));
    HitRecord record;

    bool hit = scene.hit(ray, 0.001f, 1e9f, record);
    EXPECT_TRUE(hit);
}

TEST(SceneHitTest, WithAcceleratorHitsSameMeshAsWithout) {
    auto buildScene = [](bool withAccelerator) {
        Scene scene;
        scene.addMaterial(Material::makeDiffuse(Color(0.8f, 0.8f, 0.8f)));

        Mesh quad;
        quad.materialID = 0;
        quad.vertexPositions = {
            Point3(-1.0f, -1.0f, -1.0f),
            Point3( 1.0f, -1.0f, -1.0f),
            Point3( 1.0f,  1.0f, -1.0f),
            Point3(-1.0f,  1.0f, -1.0f)
        };
        quad.triangleIndices = { 0, 1, 2,  0, 2, 3 };
        scene.addMesh(quad);

        if (withAccelerator)
            scene.buildAccelerator();

        return scene;
    };

    Scene sceneWithout = buildScene(false);
    Scene sceneWith = buildScene(true);

    Ray ray(Point3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 0.0f, -1.0f));
    HitRecord recordWithout, recordWith;

    bool hitWithout = sceneWithout.hit(ray, 0.001f, 1e9f, recordWithout);
    bool hitWith = sceneWith.hit(ray, 0.001f, 1e9f, recordWith);

    EXPECT_EQ(hitWithout, hitWith);
    EXPECT_NEAR(recordWithout.distance, recordWith.distance, 0.001f);
    EXPECT_EQ(recordWithout.materialID, recordWith.materialID);
}