#include <gtest/gtest.h>
#include <accelerator/Embree.h>
#include <core/Scene.h>
#include <geometry/Mesh.h>
#include <shading/Material.h>

// Build a minimal scene
static Scene buildSingleQuadScene()
{
    Scene scene;
    int materialID = scene.addMaterial(Material::makeDiffuse(Color(0.8f, 0.8f, 0.8f)));

    Mesh quad;
    quad.materialID = materialID;

    quad.vertexPositions = {Point3(-1.0f, -1.0f, -1.0f), Point3(1.0f, -1.0f, -1.0f),
                            Point3(1.0f, 1.0f, -1.0f), Point3(-1.0f, 1.0f, -1.0f)};

    // Two triangles forming a quad
    quad.triangleIndices = {0, 1, 2, 0, 2, 3};

    scene.addMesh(quad);
    scene.buildAccelerator();

    return scene;
}

TEST(EmbreeTest, AcceleratorBuildsWithoutCrash)
{
    Scene scene = buildSingleQuadScene();

    EXPECT_TRUE(scene.acceleratorBuilt);
}

TEST(EmbreeTest, RayHitsQuadReturnsTrue)
{
    Scene scene = buildSingleQuadScene();
    Ray ray(Point3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 0.0f, -1.0f));
    HitRecord record;

    bool hit = scene.accelerator.intersect(ray, 0.001f, 1e9f, record);

    EXPECT_TRUE(hit);
}

TEST(EmbreeTest, RayHitDistanceIsCorrect)
{
    Scene scene = buildSingleQuadScene();
    Ray ray(Point3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 0.0f, -1.0f));
    HitRecord record;

    scene.accelerator.intersect(ray, 0.001f, 1e9f, record);

    EXPECT_NEAR(record.distance, 1.0f, 0.001f);
}

TEST(EmbreeTest, RayMissReturnsNoHit)
{
    Scene scene = buildSingleQuadScene();
    Ray ray(Point3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 0.0f, 1.0f));
    HitRecord record;

    bool hit = scene.accelerator.intersect(ray, 0.001f, 1e9f, record);

    EXPECT_FALSE(hit);
}

TEST(EmbreeTest, RayMissesOutsideQuadBoundary)
{
    Scene scene = buildSingleQuadScene();
    Ray ray(Point3(5.0f, 5.0f, 0.0f), Vec3(0.0f, 0.0f, -1.0f));
    HitRecord record;

    bool hit = scene.accelerator.intersect(ray, 0.001f, 1e9f, record);

    EXPECT_FALSE(hit);
}

TEST(EmbreeTest, HitRecordMaterialIDMatchesMesh)
{
    Scene scene = buildSingleQuadScene();
    Ray ray(Point3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 0.0f, -1.0f));
    HitRecord record;

    scene.accelerator.intersect(ray, 0.001f, 1e9f, record);

    EXPECT_EQ(record.materialID, 0);
}

TEST(EmbreeTest, HitRecordNormalPointsTowardRay)
{
    Scene scene = buildSingleQuadScene();
    Ray ray(Point3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 0.0f, -1.0f));
    HitRecord record;

    scene.accelerator.intersect(ray, 0.001f, 1e9f, record);

    EXPECT_GT(record.normal.z, 0.0f);
    EXPECT_TRUE(record.frontFace);
}

TEST(EmbreeTest, HitBeyondMaxDistanceReturnsFalse)
{
    Scene scene = buildSingleQuadScene();
    Ray ray(Point3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 0.0f, -1.0f));
    HitRecord record;

    bool hit = scene.accelerator.intersect(ray, 0.001f, 0.5f, record);

    EXPECT_FALSE(hit);
}

TEST(EmbreeTest, EmptySceneReturnsNoHit)
{
    Scene scene;
    scene.buildAccelerator();

    Ray ray(Point3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 0.0f, -1.0f));
    HitRecord record;

    bool hit = scene.accelerator.intersect(ray, 0.001f, 1e9f, record);

    EXPECT_FALSE(hit);
}

TEST(EmbreeTest, MultiMeshSceneHitsCorrectMaterial)
{
    Scene scene;

    int materialA = scene.addMaterial(Material::makeDiffuse(Color(1.0f, 0.0f, 0.0f)));
    int materialB = scene.addMaterial(Material::makeDiffuse(Color(0.0f, 1.0f, 0.0f)));

    Mesh meshA;
    meshA.materialID = materialA;
    meshA.vertexPositions = {Point3(-1.0f, -1.0f, -2.0f), Point3(1.0f, -1.0f, -2.0f),
                             Point3(0.0f, 1.0f, -2.0f)};
    meshA.triangleIndices = {0, 1, 2};
    scene.addMesh(meshA);

    Mesh meshB;
    meshB.materialID = materialB;
    meshB.vertexPositions = {Point3(-1.0f, -1.0f, -1.0f), Point3(1.0f, -1.0f, -1.0f),
                             Point3(0.0f, 1.0f, -1.0f)};
    meshB.triangleIndices = {0, 1, 2};
    scene.addMesh(meshB);

    scene.buildAccelerator();

    Ray ray(Point3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 0.0f, -1.0f));
    HitRecord record;

    scene.accelerator.intersect(ray, 0.001f, 1e9f, record);

    EXPECT_EQ(record.materialID, materialB);
    EXPECT_NEAR(record.distance, 1.0f, 0.01f);
}