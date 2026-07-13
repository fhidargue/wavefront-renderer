#include <gtest/gtest.h>
#include <core/Scene.h>
#include <shading/Material.h>
#include <geometry/Mesh.h>
#include <scheduling/RayQueue.h>

static Scene buildSceneWithKnownExtents()
{
    Scene scene;
    int material = scene.addMaterial(Material::makeDiffuse(Color(0.8f, 0.8f, 0.8f)));

    Mesh quad;
    quad.materialID = material;
    quad.vertexPositions = {Point3(-4.0f, -1.0f, -2.0f), Point3(3.0f, -1.0f, -2.0f),
                            Point3(3.0f, 5.0f, 6.0f), Point3(-4.0f, 5.0f, 6.0f)};
    quad.triangleIndices = {0, 1, 2, 0, 2, 3};
    scene.addMesh(quad);
    scene.buildAccelerator();

    return scene;
}

static Scene buildFlatScene()
{
    Scene scene;
    int material = scene.addMaterial(Material::makeDiffuse(Color(0.8f, 0.8f, 0.8f)));

    Mesh quad;
    quad.materialID = material;
    quad.vertexPositions = {Point3(-1.0f, -1.0f, 0.0f), Point3(1.0f, -1.0f, 0.0f),
                            Point3(1.0f, 1.0f, 0.0f), Point3(-1.0f, 1.0f, 0.0f)};
    quad.triangleIndices = {0, 1, 2, 0, 2, 3};
    scene.addMesh(quad);
    scene.buildAccelerator();

    return scene;
}

TEST(SceneTest, ComputeBoundsMatchesKnownVertexExtents)
{
    Scene scene = buildSceneWithKnownExtents();

    EXPECT_FLOAT_EQ(scene.boundsMin.x, -4.0f);
    EXPECT_FLOAT_EQ(scene.boundsMin.y, -1.0f);
    EXPECT_FLOAT_EQ(scene.boundsMin.z, -2.0f);
    EXPECT_FLOAT_EQ(scene.boundsMax.x, 3.0f);
    EXPECT_FLOAT_EQ(scene.boundsMax.y, 5.0f);
    EXPECT_FLOAT_EQ(scene.boundsMax.z, 6.0f);
}

TEST(SceneTest, BuildAcceleratorSetsBoundsBeforeReturning)
{
    Scene scene = buildSceneWithKnownExtents();

    EXPECT_TRUE(scene.acceleratorBuilt);
    EXPECT_LT(scene.boundsMin.x, scene.boundsMax.x);
}

TEST(SceneTest, FlatSceneProducesZeroExtentOnOneAxis)
{
    Scene scene = buildFlatScene();

    EXPECT_FLOAT_EQ(scene.boundsMin.z, 0.0f);
    EXPECT_FLOAT_EQ(scene.boundsMax.z, 0.0f);
}

TEST(SceneTest, RayQueueScheduleDoesNotDivideByZeroOnFlatScene)
{
    Scene scene = buildFlatScene();

    RayQueue queue;
    queue.addPrimary(Ray(Point3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 0.0f, -1.0f)), 0);
    queue.addPrimary(Ray(Point3(0.5f, 0.5f, 0.0f), Vec3(0.0f, 0.0f, -1.0f)), 1);

    queue.schedule(scene.boundsMin, scene.boundsMax);
    queue.materialize();

    EXPECT_EQ(queue.size(), 2);
    EXPECT_TRUE(std::isfinite(queue.originsX[0]));
    EXPECT_TRUE(std::isfinite(queue.originsZ[0]));
}