#include <gtest/gtest.h>
#include <shading/Material.h>
#include <shading/Texture.h>
#include <math/SpatialHash.h>
#include <geometry/Mesh.h>
#include <core/HitRecord.h>
#include <scheduling/ShadingQueue.h>
#include <scheduling/RayQueue.h>

static std::vector<Material> makeTestMaterials()
{
    return {
        Material::makeDiffuse(Color(1, 1, 1)),
        Material::makeMetal(Color(1, 1, 1), 0.5f),
        Material::makeEmissive(Color(1, 1, 1), 1.0f),
        Material::makeSpotLight(Color(1, 1, 1), 1.0f, 45.0f, 30.0f),
        Material::makeGlass(1.5f),
        Material::makePlastic(Color(1, 1, 1), 0.5f),
    };
}

static std::vector<Texture> makeTestTextures()
{
    return {Texture(1, 1), Texture(2, 2), Texture(4, 4)};
}

TEST(SpatialHashTest, SameCellProducesSameHash)
{
    unsigned int hashA = hashGridCell(3, -2, 7);
    unsigned int hashB = hashGridCell(3, -2, 7);
    EXPECT_EQ(hashA, hashB);
}

TEST(SpatialHashTest, DifferentCellsProduceDifferentHashes)
{
    unsigned int hashA = hashGridCell(0, 0, 0);
    unsigned int hashB = hashGridCell(1, 0, 0);
    EXPECT_NE(hashA, hashB);
}

TEST(SpatialHashTest, ColorFromHashStaysInRange)
{
    Color color = colorFromHash(hashGridCell(5, 5, 5), 0.25f);
    EXPECT_GE(color.x, 0.0f);
    EXPECT_LE(color.x, 1.0f);
    EXPECT_GE(color.y, 0.0f);
    EXPECT_LE(color.y, 1.0f);
    EXPECT_GE(color.z, 0.0f);
    EXPECT_LE(color.z, 1.0f);
}

TEST(SpatialHashTest, PastelAmountPushesColorTowardWhite)
{
    unsigned int hash = hashGridCell(9, 1, 4);
    Color sharp = colorFromHash(hash, 0.0f);
    Color pastel = colorFromHash(hash, 0.9f);

    float sharpDistance = (1.0f - sharp.x) + (1.0f - sharp.y) + (1.0f - sharp.z);
    float pastelDistance = (1.0f - pastel.x) + (1.0f - pastel.y) + (1.0f - pastel.z);
    EXPECT_LT(pastelDistance, sharpDistance);
}

TEST(MeshUVTest, FallsBackToBarycentricWhenNoUVsAuthored)
{
    Mesh mesh;
    mesh.vertexPositions = {Point3(0, 0, 0), Point3(1, 0, 0), Point3(0, 1, 0)};
    mesh.triangleIndices = {0, 1, 2};

    float outU, outV;
    mesh.interpolateUV(0, 0.3f, 0.4f, outU, outV);

    EXPECT_FLOAT_EQ(outU, 0.3f);
    EXPECT_FLOAT_EQ(outV, 0.4f);
}

TEST(MeshUVTest, InterpolatesAuthoredUVsCorrectly)
{
    Mesh mesh;
    mesh.vertexPositions = {Point3(0, 0, 0), Point3(1, 0, 0), Point3(0, 1, 0)};
    mesh.triangleIndices = {0, 1, 2};

    mesh.triangleUVsU = {0.0f, 1.0f, 0.0f};
    mesh.triangleUVsV = {0.0f, 0.0f, 1.0f};

    float outU, outV;

    mesh.interpolateUV(0, 0.0f, 0.0f, outU, outV);
    EXPECT_FLOAT_EQ(outU, 0.0f);
    EXPECT_FLOAT_EQ(outV, 0.0f);

    mesh.interpolateUV(0, 1.0f, 0.0f, outU, outV);
    EXPECT_FLOAT_EQ(outU, 1.0f);
    EXPECT_FLOAT_EQ(outV, 0.0f);

    mesh.interpolateUV(0, 0.0f, 1.0f, outU, outV);
    EXPECT_FLOAT_EQ(outU, 0.0f);
    EXPECT_FLOAT_EQ(outV, 1.0f);
}

TEST(HitRecordTest, TriangleIndexDefaultsToMinusOne)
{
    HitRecord record;
    EXPECT_EQ(record.triangleIndex, -1);
}

static HitRecord makeHit(float u, float v, int triangleIndex, int materialID)
{
    HitRecord hit;
    hit.point = Point3(0, 0, 0);
    hit.normal = Vec3(0, 1, 0);
    hit.distance = 1.0f;
    hit.u = u;
    hit.v = v;
    hit.triangleIndex = triangleIndex;
    hit.materialID = materialID;
    hit.textureID = -1;
    return hit;
}

TEST(ShadingQueueTest, PreservesUVAndTriangleIndexThroughAddAndGet)
{
    RayQueue rays;
    rays.addPrimary(Ray(Point3(0, 0, 0), Vec3(0, 0, -1)), 0);

    ShadingQueue queue(SchedulingPolicy::None);
    queue.add(rays, 0, makeHit(0.25f, 0.75f, 42, 0));

    std::vector<Material> materials = makeTestMaterials();
    std::vector<Texture> textures = makeTestTextures();
    queue.schedule(materials, textures);

    HitRecord retrieved = queue.getHitRecord(0);
    EXPECT_FLOAT_EQ(retrieved.u, 0.25f);
    EXPECT_FLOAT_EQ(retrieved.v, 0.75f);
    EXPECT_EQ(retrieved.triangleIndex, 42);
}

TEST(ShadingQueueTest, MaterializeKeepsUVInSyncAfterSort)
{
    RayQueue rays;
    rays.addPrimary(Ray(Point3(0, 0, 0), Vec3(0, 0, -1)), 0);
    rays.addPrimary(Ray(Point3(0, 0, 0), Vec3(0, 0, -1)), 1);
    rays.addPrimary(Ray(Point3(0, 0, 0), Vec3(0, 0, -1)), 2);

    ShadingQueue queue(SchedulingPolicy::MaterialAware);

    queue.add(rays, 0, makeHit(0.1f, 0.1f, 100, 2));
    queue.add(rays, 1, makeHit(0.2f, 0.2f, 200, 0));
    queue.add(rays, 2, makeHit(0.3f, 0.3f, 300, 1));

    std::vector<Material> materials = makeTestMaterials();
    std::vector<Texture> textures = makeTestTextures();
    queue.schedule(materials, textures);
    queue.materialize();

    for (int i = 0; i < queue.size(); ++i)
    {
        HitRecord record = queue.getHitRecord(i);

        if (record.materialID == 2)
        {
            EXPECT_FLOAT_EQ(record.u, 0.1f);
            EXPECT_EQ(record.triangleIndex, 100);
        }
        else if (record.materialID == 0)
        {
            EXPECT_FLOAT_EQ(record.u, 0.2f);
            EXPECT_EQ(record.triangleIndex, 200);
        }
        else if (record.materialID == 1)
        {
            EXPECT_FLOAT_EQ(record.u, 0.3f);
            EXPECT_EQ(record.triangleIndex, 300);
        }
        else
        {
            FAIL() << "Unexpected materialID in queue after materialize";
        }
    }
}

TEST(MaterialGlassTest, ScatterAlwaysContinuesAndHasNoCleanPdf)
{
    Material glass = Material::makeGlass(1.5f);

    Ray incoming(Point3(0, 1, 0), Vec3(0, -1, 0));
    HitRecord record;
    record.point = Point3(0, 0, 0);
    record.setFaceNormal(incoming, Vec3(0, 1, 0));

    Color attenuation;
    Ray scattered;
    float outPdf;
    std::vector<Texture> noTextures;

    bool didScatter = glass.scatter(incoming, record, noTextures, attenuation, scattered, outPdf);

    EXPECT_TRUE(didScatter);
    EXPECT_FLOAT_EQ(outPdf, -1.0f);
}

TEST(MaterialGlassTest, GrazingAngleTriggersTotalInternalReflection)
{
    Material glass = Material::makeGlass(1.5f);

    HitRecord record;
    record.point = Point3(0, 0, 0);
    record.normal = Vec3(0, 1, 0);
    record.frontFace = false; // ray is exiting the glass

    // Direction nearly parallel to the surface
    Ray incoming(Point3(0, 0.01f, 0), Vec3(0.999f, -0.05f, 0).normalized());

    Color attenuation;
    Ray scattered;
    float outPdf;
    std::vector<Texture> noTextures;

    bool didScatter = glass.scatter(incoming, record, noTextures, attenuation, scattered, outPdf);

    EXPECT_TRUE(didScatter);

    // Reflected ray should stay on the same side of the surface as the normal
    EXPECT_GT(scattered.direction.dot(record.normal), -0.01f);
}

TEST(MaterialPlasticTest, ScatterAlwaysContinuesAndHasNoCleanPdfOnSpecularBranch)
{
    Material plastic = Material::makePlastic(Color(0.9f, 0.75f, 0.15f), 0.1f);

    Ray incoming(Point3(0, 1, 0), Vec3(0, -1, 0));
    HitRecord record;
    record.point = Point3(0, 0, 0);
    record.setFaceNormal(incoming, Vec3(0, 1, 0));

    std::vector<Texture> noTextures;

    for (int i = 0; i < 50; ++i)
    {
        Color attenuation;
        Ray scattered;
        float outPdf;

        bool didScatter =
            plastic.scatter(incoming, record, noTextures, attenuation, scattered, outPdf);

        EXPECT_TRUE(didScatter);
        EXPECT_TRUE(outPdf > 0.0f || outPdf == -1.0f);
    }
}

TEST(MaterialPlasticTest, DiffuseBranchTintsWithAlbedoSpecularBranchStaysWhite)
{
    Color albedo(0.9f, 0.2f, 0.2f);
    Material plastic = Material::makePlastic(albedo, 0.1f);

    Ray incoming(Point3(0, 1, 0), Vec3(0, -1, 0));
    HitRecord record;
    record.point = Point3(0, 0, 0);
    record.setFaceNormal(incoming, Vec3(0, 1, 0));

    std::vector<Texture> noTextures;
    bool sawDiffuseBranch = false;
    bool sawSpecularBranch = false;

    for (int i = 0; i < 200 && !(sawDiffuseBranch && sawSpecularBranch); ++i)
    {
        Color attenuation;
        Ray scattered;
        float outPdf;
        plastic.scatter(incoming, record, noTextures, attenuation, scattered, outPdf);

        if (outPdf > 0.0f)
        {
            sawDiffuseBranch = true;
            EXPECT_NEAR(attenuation.x, albedo.x, 1e-5f);
            EXPECT_NEAR(attenuation.y, albedo.y, 1e-5f);
            EXPECT_NEAR(attenuation.z, albedo.z, 1e-5f);
        }
        else
        {
            sawSpecularBranch = true;
            EXPECT_NEAR(attenuation.x, 1.0f, 1e-5f);
            EXPECT_NEAR(attenuation.y, 1.0f, 1e-5f);
            EXPECT_NEAR(attenuation.z, 1.0f, 1e-5f);
        }
    }

    EXPECT_TRUE(sawDiffuseBranch) << "Never observed the diffuse branch in 200 samples";
    EXPECT_TRUE(sawSpecularBranch) << "Never observed the specular branch in 200 samples";
}