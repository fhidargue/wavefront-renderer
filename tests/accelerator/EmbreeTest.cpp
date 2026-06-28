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

// SIMD intersect4

// ─────────────────────────────────────────
// SIMD intersect4 tests
// ─────────────────────────────────────────

static Scene buildFloorScene()
{
    Scene scene;
    scene.addMaterial(Material::makeDiffuse(Color(0.8f, 0.8f, 0.8f)));

    Mesh floor;
    floor.materialID = 0;
    floor.uuid = "/Test/Floor";
    floor.vertexPositions = {
        Point3(-5, 0, -5),
        Point3( 5, 0, -5),
        Point3( 5, 0,  5),
        Point3(-5, 0,  5)
    };
    floor.triangleIndices = {0, 1, 2, 0, 2, 3};
    scene.addMesh(floor);
    scene.buildAccelerator();

    return scene;
}

static RayQueue makeQueue(const std::vector<Ray>& rays)
{
    RayQueue queue;
    for (int i = 0; i < static_cast<int>(rays.size()); ++i)
        queue.addPrimary(rays[i], i);

    return queue;
}

TEST(EmbreeTest, Intersect4AllFourRaysHit)
{
    Scene scene = buildFloorScene();

    auto queue = makeQueue({
        Ray(Point3(-2, 5, -2), Vec3(0, -1, 0)),
        Ray(Point3( 0, 5,  0), Vec3(0, -1, 0)),
        Ray(Point3( 2, 5,  2), Vec3(0, -1, 0)),
        Ray(Point3(-1, 5,  1), Vec3(0, -1, 0))
    });

    HitRecord records[4];
    int hitMask = scene.accelerator.intersect4(queue, 0, 4, records);

    EXPECT_EQ(hitMask, 15);
}

TEST(EmbreeTest, Intersect4AllFourRaysMiss)
{
    Scene scene = buildFloorScene();

    auto queue = makeQueue({
        Ray(Point3(-2, 5, -2), Vec3(0, 1, 0)),
        Ray(Point3( 0, 5,  0), Vec3(0, 1, 0)),
        Ray(Point3( 2, 5,  2), Vec3(0, 1, 0)),
        Ray(Point3(-1, 5,  1), Vec3(0, 1, 0))
    });

    HitRecord records[4];
    int hitMask = scene.accelerator.intersect4(queue, 0, 4, records);

    EXPECT_EQ(hitMask, 0);
}

TEST(EmbreeTest, Intersect4PartialHitMask)
{
    Scene scene = buildFloorScene();

    // rays 0 and 2 hit, rays 1 and 3 miss
    auto queue = makeQueue({
        Ray(Point3(0, 5, 0), Vec3(0, -1, 0)),
        Ray(Point3(0, 5, 0), Vec3(0,  1, 0)),
        Ray(Point3(2, 5, 2), Vec3(0, -1, 0)),
        Ray(Point3(0, 5, 0), Vec3(0,  1, 0))
    });

    HitRecord records[4];
    int hitMask = scene.accelerator.intersect4(queue, 0, 4, records);

    EXPECT_TRUE(hitMask  & (1 << 0));
    EXPECT_FALSE(hitMask & (1 << 1));
    EXPECT_TRUE(hitMask  & (1 << 2));
    EXPECT_FALSE(hitMask & (1 << 3));
}

TEST(EmbreeTest, Intersect4MaterialIDCorrect)
{
    Scene scene = buildFloorScene();

    auto queue = makeQueue({
        Ray(Point3(0, 5, 0), Vec3(0, -1, 0)),
        Ray(Point3(0, 5, 0), Vec3(0, -1, 0)),
        Ray(Point3(0, 5, 0), Vec3(0, -1, 0)),
        Ray(Point3(0, 5, 0), Vec3(0, -1, 0))
    });

    HitRecord records[4];
    int hitMask = scene.accelerator.intersect4(queue, 0, 4, records);

    ASSERT_EQ(hitMask, 15);

    for (int i = 0; i < 4; ++i)
        EXPECT_EQ(records[i].materialID, 0);
}

TEST(EmbreeTest, Intersect4MatchesScalarIntersect)
{
    Scene scene = buildFloorScene();

    std::vector<Ray> rays = {
        Ray(Point3(-2, 5, -2), Vec3(0, -1, 0)),
        Ray(Point3( 0, 5,  0), Vec3(0, -1, 0)),
        Ray(Point3( 2, 5,  2), Vec3(0, -1, 0)),
        Ray(Point3(-1, 5,  1), Vec3(0, -1, 0))
    };

    auto queue = makeQueue(rays);
    HitRecord simdRecords[4];
    scene.accelerator.intersect4(queue, 0, 4, simdRecords);

    for (int i = 0; i < 4; ++i)
    {
        HitRecord scalar;
        bool hit = scene.accelerator.intersect(rays[i], 0.001f, 1e9f, scalar);

        ASSERT_TRUE(hit);
        EXPECT_NEAR(simdRecords[i].distance, scalar.distance, 0.001f);
        EXPECT_NEAR(simdRecords[i].point.x,  scalar.point.x,  0.001f);
        EXPECT_NEAR(simdRecords[i].point.y,  scalar.point.y,  0.001f);
        EXPECT_NEAR(simdRecords[i].point.z,  scalar.point.z,  0.001f);
        EXPECT_EQ(simdRecords[i].materialID, scalar.materialID);
    }
}

TEST(EmbreeTest, Intersect4StartIndexOffset)
{
    Scene scene = buildFloorScene();

    // First 2 rays miss, last 4 hit
    auto queue = makeQueue({
        Ray(Point3(0, 5, 0), Vec3(0,  1, 0)),
        Ray(Point3(0, 5, 0), Vec3(0,  1, 0)),
        Ray(Point3(0, 5, 0), Vec3(0, -1, 0)),
        Ray(Point3(1, 5, 0), Vec3(0, -1, 0)),
        Ray(Point3(2, 5, 0), Vec3(0, -1, 0)),
        Ray(Point3(3, 5, 0), Vec3(0, -1, 0))
    });

    HitRecord records[4];
    int hitMask = scene.accelerator.intersect4(queue, 2, 4, records);

    EXPECT_EQ(hitMask, 15);
}