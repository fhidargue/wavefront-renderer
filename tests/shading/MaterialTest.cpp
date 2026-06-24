#include <gtest/gtest.h>
#include <shading/Material.h>

TEST(MaterialTest, MakeDiffuseSetsType)
{
    Material material = Material::makeDiffuse(Color(1.0f, 0.0f, 0.0f));

    EXPECT_EQ(material.type, MaterialType::Diffuse);
}

TEST(MaterialTest, MakeDiffuseStoresAlbedo)
{
    Material material = Material::makeDiffuse(Color(0.8f, 0.2f, 0.3f));

    EXPECT_NEAR(material.albedo.x, 0.8f, 0.0001f);
    EXPECT_NEAR(material.albedo.y, 0.2f, 0.0001f);
    EXPECT_NEAR(material.albedo.z, 0.3f, 0.0001f);
}

TEST(MaterialTest, MakeDiffuseDefaultTextureIDIsMinusOne)
{
    Material material = Material::makeDiffuse(Color(1.0f, 1.0f, 1.0f));

    EXPECT_EQ(material.textureID, -1);
}

TEST(MaterialTest, MakeMetalSetsType)
{
    Material material = Material::makeMetal(Color(0.8f, 0.8f, 0.8f), 0.1f);

    EXPECT_EQ(material.type, MaterialType::Metal);
}

TEST(MaterialTest, MakeMetalStoresRoughness)
{
    Material material = Material::makeMetal(Color(0.8f, 0.8f, 0.8f), 0.3f);

    EXPECT_NEAR(material.roughness, 0.3f, 0.0001f);
}

TEST(MaterialTest, MakeEmissiveSetsType)
{
    Material material = Material::makeEmissive(Color(1.0f, 1.0f, 1.0f), 5.0f);

    EXPECT_EQ(material.type, MaterialType::Emissive);
}

TEST(MaterialTest, MakeEmissiveStoresEmissionStrength)
{
    Material material = Material::makeEmissive(Color(1.0f, 1.0f, 1.0f), 5.0f);

    EXPECT_NEAR(material.emission, 5.0f, 0.0001f);
}

TEST(MaterialTest, EmittedReturnsBlackForDiffuse)
{
    Material material = Material::makeDiffuse(Color(1.0f, 0.0f, 0.0f));
    Color emitted = material.emitted();

    EXPECT_NEAR(emitted.x, 0.0f, 0.0001f);
    EXPECT_NEAR(emitted.y, 0.0f, 0.0001f);
    EXPECT_NEAR(emitted.z, 0.0f, 0.0001f);
}

TEST(MaterialTest, EmittedReturnsNonBlackForEmissive)
{
    Material material = Material::makeEmissive(Color(1.0f, 1.0f, 1.0f), 5.0f);
    Color emitted = material.emitted();

    EXPECT_GT(emitted.x, 0.0f);
    EXPECT_GT(emitted.y, 0.0f);
    EXPECT_GT(emitted.z, 0.0f);
}