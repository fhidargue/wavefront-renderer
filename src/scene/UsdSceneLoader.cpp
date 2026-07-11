#include <scene/UsdSceneLoader.h>
#include <geometry/Mesh.h>
#include <shading/Material.h>
#include <math/Vec3.h>
#include <shading/Texture.h>

#include <iostream>
#include <unordered_map>
#include <cstdio>
#include <cmath>

#include <pxr/usd/usd/stage.h>
#include <pxr/usd/usd/primRange.h>
#include <pxr/usd/usdGeom/mesh.h>
#include <pxr/usd/usdGeom/xformCache.h>
#include <pxr/usd/usdGeom/mesh.h>
#include <pxr/usd/usdGeom/sphere.h>
#include <pxr/usd/usdGeom/cube.h>
#include <pxr/usd/usdGeom/sphere.h>
#include <pxr/usd/usdGeom/cylinder.h>
#include <pxr/usd/usdGeom/metrics.h>
#include <pxr/usd/usdGeom/primvarsAPI.h>
#include <pxr/usd/usdShade/material.h>
#include <pxr/usd/usdShade/materialBindingAPI.h>
#include <pxr/usd/usdShade/shader.h>
#include <pxr/usd/usdLux/domeLight.h>
#include <pxr/usd/usdLux/rectLight.h>
#include <pxr/usd/usdLux/diskLight.h>
#include <pxr/usd/usdLux/sphereLight.h>
#include <pxr/usd/usdLux/cylinderLight.h>
#include <pxr/usd/usdLux/distantLight.h>
#include <pxr/usd/usd/timeCode.h>
#include <pxr/base/gf/vec3f.h>
#include <pxr/base/gf/vec4d.h>
#include <pxr/base/gf/matrix4d.h>
#include <pxr/base/vt/array.h>
#include <pxr/base/gf/rotation.h>

using std::cerr;
using std::cout;
using std::endl;
using std::string;
using std::to_string;
using std::vector;

PXR_NAMESPACE_USING_DIRECTIVE

static string colorToHex(const Color& color)
{
    char buffer[8];
    std::snprintf(buffer, sizeof(buffer), "%02X%02X%02X", static_cast<int>(color.x * 255),
                  static_cast<int>(color.y * 255), static_cast<int>(color.z * 255));

    return string(buffer);
}

static void triangulateFace(const VtIntArray& indices, int startIndex, int vertexCount,
                            vector<int>& outIndices)
{
    for (int i = 1; i < vertexCount - 1; ++i)
    {
        outIndices.push_back(indices[startIndex]);
        outIndices.push_back(indices[startIndex + i]);
        outIndices.push_back(indices[startIndex + i + 1]);
    }
}

static void triangulateFaceUVs(const VtVec2fArray& faceUVs, int startIndex, int vertexCount,
                               vector<float>& outU, vector<float>& outV)
{
    for (int i = 1; i < vertexCount - 1; ++i)
    {
        outU.push_back(faceUVs[startIndex][0]);
        outV.push_back(faceUVs[startIndex][1]);
        outU.push_back(faceUVs[startIndex + i][0]);
        outV.push_back(faceUVs[startIndex + i][1]);
        outU.push_back(faceUVs[startIndex + i + 1][0]);
        outV.push_back(faceUVs[startIndex + i + 1][1]);
    }
}

static void toVtArrays(const vector<GfVec3f>& points, const vector<int>& faceCounts,
                       const vector<int>& faceIndices, VtVec3fArray& outPoints,
                       VtIntArray& outFaceCounts, VtIntArray& outFaceIndices)
{
    outPoints = VtVec3fArray(points.begin(), points.end());
    outFaceCounts = VtIntArray(faceCounts.begin(), faceCounts.end());
    outFaceIndices = VtIntArray(faceIndices.begin(), faceIndices.end());
}

static void appendRing(vector<GfVec3f>& points, double radius, float z, int segments)
{
    for (int i = 0; i < segments; ++i)
    {
        float angle = static_cast<float>(i) / segments * 2.0f * static_cast<float>(M_PI);
        points.push_back(GfVec3f(static_cast<float>(radius) * std::cos(angle),
                                 static_cast<float>(radius) * std::sin(angle), z));
    }
}

static void tessellateCube(double size, VtVec3fArray& outPoints, VtIntArray& outFaceCounts,
                           VtIntArray& outFaceIndices)
{
    float halfSize = static_cast<float>(size) * 0.5f;

    vector<GfVec3f> points = {
        GfVec3f(-halfSize, -halfSize, -halfSize), GfVec3f(halfSize, -halfSize, -halfSize),
        GfVec3f(halfSize, halfSize, -halfSize),   GfVec3f(-halfSize, halfSize, -halfSize),
        GfVec3f(-halfSize, -halfSize, halfSize),  GfVec3f(halfSize, -halfSize, halfSize),
        GfVec3f(halfSize, halfSize, halfSize),    GfVec3f(-halfSize, halfSize, halfSize),
    };

    // Six quad faces
    vector<int> faceIndices = {
        0, 1, 2, 3, // -Z
        4, 7, 6, 5, // +Z
        0, 4, 5, 1, // -Y
        2, 6, 7, 3, // +Y
        0, 3, 7, 4, // -X
        1, 5, 6, 2, // +X
    };
    vector<int> faceCounts(6, 4);

    toVtArrays(points, faceCounts, faceIndices, outPoints, outFaceCounts, outFaceIndices);
}

static void tessellateSphere(double radius, int segmentsU, int segmentsV, VtVec3fArray& outPoints,
                             VtIntArray& outFaceCounts, VtIntArray& outFaceIndices)
{
    vector<GfVec3f> points;
    vector<int> faceCounts;
    vector<int> faceIndices;

    // Rings of vertices from pole to pole, UV sphere
    for (int v = 0; v <= segmentsV; ++v)
    {
        float theta = static_cast<float>(v) / segmentsV * static_cast<float>(M_PI);
        float y = static_cast<float>(radius) * std::cos(theta);
        float ringRadius = radius * std::sin(theta);

        appendRing(points, ringRadius, y, segmentsU);
    }

    auto vertexIndex = [segmentsU](int v, int u) { return v * segmentsU + (u % segmentsU); };

    for (int v = 0; v < segmentsV; ++v)
    {
        for (int u = 0; u < segmentsU; ++u)
        {
            faceIndices.insert(faceIndices.end(),
                               {vertexIndex(v, u), vertexIndex(v, u + 1), vertexIndex(v + 1, u + 1),
                                vertexIndex(v + 1, u)});
            faceCounts.push_back(4);
        }
    }

    toVtArrays(points, faceCounts, faceIndices, outPoints, outFaceCounts, outFaceIndices);
}

static void tessellateCylinder(double radius, double height, int segments, VtVec3fArray& outPoints,
                               VtIntArray& outFaceCounts, VtIntArray& outFaceIndices)
{
    vector<GfVec3f> points;
    vector<int> faceCounts;
    vector<int> faceIndices;

    float halfHeight = static_cast<float>(height) * 0.5f;

    appendRing(points, radius, -halfHeight, segments);
    appendRing(points, radius, halfHeight, segments);

    int bottomCenterIndex = static_cast<int>(points.size());
    points.push_back(GfVec3f(0.0f, 0.0f, -halfHeight));
    int topCenterIndex = static_cast<int>(points.size());
    points.push_back(GfVec3f(0.0f, 0.0f, halfHeight));

    for (int i = 0; i < segments; ++i)
    {
        int next = (i + 1) % segments;

        // Side quad
        faceIndices.insert(faceIndices.end(), {i, next, segments + next, segments + i});
        faceCounts.push_back(4);

        // Bottom cap triangle
        faceIndices.insert(faceIndices.end(), {bottomCenterIndex, next, i});
        faceCounts.push_back(3);

        // Top cap triangle
        faceIndices.insert(faceIndices.end(), {topCenterIndex, segments + i, segments + next});
        faceCounts.push_back(3);
    }

    toVtArrays(points, faceCounts, faceIndices, outPoints, outFaceCounts, outFaceIndices);
}

static void buildDiskLightQuad(double radius, int segments, VtVec3fArray& outPoints,
                               VtIntArray& outFaceCounts, VtIntArray& outFaceIndices)
{
    vector<GfVec3f> points;
    vector<int> faceCounts;
    vector<int> faceIndices;

    // Disk lies in local XY plane to match UsdLux convention
    appendRing(points, radius, 0.0f, segments);
    int centerIndex = static_cast<int>(points.size());
    points.push_back(GfVec3f(0.0f, 0.0f, 0.0f));

    for (int i = 0; i < segments; ++i)
    {
        int next = (i + 1) % segments;
        faceIndices.insert(faceIndices.end(), {centerIndex, i, next});
        faceCounts.push_back(3);
    }

    toVtArrays(points, faceCounts, faceIndices, outPoints, outFaceCounts, outFaceIndices);
}

static void buildRectLightQuad(double width, double height, VtVec3fArray& outPoints,
                               VtIntArray& outFaceCounts, VtIntArray& outFaceIndices)
{
    float halfWidth = static_cast<float>(width) * 0.5f;
    float halfHeight = static_cast<float>(height) * 0.5f;

    vector<GfVec3f> points = {
        GfVec3f(-halfWidth, -halfHeight, 0.0f),
        GfVec3f(halfWidth, -halfHeight, 0.0f),
        GfVec3f(halfWidth, halfHeight, 0.0f),
        GfVec3f(-halfWidth, halfHeight, 0.0f),
    };
    vector<int> faceIndices = {0, 1, 2, 3};
    vector<int> faceCounts = {4};

    toVtArrays(points, faceCounts, faceIndices, outPoints, outFaceCounts, outFaceIndices);
}

static Color readDisplayColor(const UsdPrim& prim)
{
    // Fallback implementation for scenes without UsdPreviewSurface
    const Color defaultColor = Color(0.8f, 0.8f, 0.8f);
    UsdGeomPrimvar displayColorPrimvar =
        UsdGeomPrimvarsAPI(prim).GetPrimvar(TfToken("displayColor"));

    if (!displayColorPrimvar.IsDefined())
        return defaultColor;

    VtValue value;
    displayColorPrimvar.Get(&value, UsdTimeCode::Default());

    if (value.IsEmpty())
        return defaultColor;

    if (value.IsHolding<VtVec3fArray>())
    {
        VtVec3fArray colors = value.UncheckedGet<VtVec3fArray>();

        if (!colors.empty())
            return Color(colors[0][0], colors[0][1], colors[0][2]);
    }

    if (value.IsHolding<GfVec3f>())
    {
        GfVec3f color = value.UncheckedGet<GfVec3f>();
        return Color(color[0], color[1], color[2]);
    }

    return defaultColor;
}

static void printSceneSummary(const string& usdFilePath, const Scene& scene, int totalTriangles,
                              bool isZUp)
{
    int diffuseCount = 0;
    int metalCount = 0;
    int emissiveCount = 0;
    int spotLightCount = 0;
    int glassCount = 0;

    for (const auto& material : scene.materials)
    {
        switch (material.type)
        {
        case MaterialType::Diffuse:
            diffuseCount++;
            break;
        case MaterialType::Metal:
            metalCount++;
            break;
        case MaterialType::Emissive:
            emissiveCount++;
            break;
        case MaterialType::SpotLight:
            spotLightCount++;
            break;
        case MaterialType::Glass:
            glassCount++;
            break;
        }
    }

    cout << "\n========================================" << endl;
    cout << "  Scene Summary" << endl;
    cout << "========================================" << endl;
    cout << "  File       : " << usdFilePath << endl;
    cout << "  upAxis     : " << (isZUp ? "Z (corrected to Y)" : "Y") << endl;
    cout << "  Materials  : " << scene.materials.size() << " (" << diffuseCount << " diffuse, "
         << metalCount << " metal, " << emissiveCount << " emissive, " << spotLightCount
         << " spotlight, " << glassCount << " glass)" << endl;
    cout << "  Textures   : " << scene.textures.size() << endl;
    cout << "  Meshes     : " << scene.meshes.size() << endl;
    cout << "  Triangles  : " << totalTriangles << endl;
    cout << "========================================\n" << endl;
}

Scene UsdSceneLoader::load(const string& usdFilePath)
{
    UsdStageRefPtr stage = UsdStage::Open(usdFilePath, UsdStage::LoadAll);

    if (!stage)
    {
        cerr << "ERROR: Could not open: " << usdFilePath << endl;
        return Scene();
    }

    Scene scene;
    std::unordered_map<string, int> materialIndexMap;

    // Detect upAxis from the USD scene
    TfToken upAxis = UsdGeomGetStageUpAxis(stage);
    bool isZUp = (upAxis == UsdGeomTokens->z);

    GfMatrix4d upAxisCorrection(1.0);
    if (isZUp)
    {
        upAxisCorrection = GfMatrix4d().SetRotate(GfRotation(GfVec3d(1, 0, 0), -90.0));

        cout << "Applied Z up axis correction" << endl;
    }

    // Extract global lights
    UsdGeomXformCache xformCache;

    for (const UsdPrim& prim : stage->Traverse())
    {
        if (prim.IsA<UsdLuxDistantLight>() && scene.directionalLight.intensity == 0.0f)
        {
            UsdLuxDistantLight distantLight(prim);
            float intensity = 1.0f;
            GfVec3f color(1.0f, 1.0f, 1.0f);

            distantLight.GetIntensityAttr().Get(&intensity, UsdTimeCode::Default());
            distantLight.GetColorAttr().Get(&color, UsdTimeCode::Default());

            GfMatrix4d worldTransform = xformCache.GetLocalToWorldTransform(prim);

            if (isZUp)
                worldTransform = worldTransform * upAxisCorrection;

            GfVec4d localDir(0.0, 0.0, -1.0, 0.0);
            GfVec4d worldDir = localDir * worldTransform;

            scene.directionalLight.direction =
                Vec3(static_cast<float>(worldDir[0]), static_cast<float>(worldDir[1]),
                     static_cast<float>(worldDir[2]))
                    .normalized();
            scene.directionalLight.color = Color(color[0], color[1], color[2]);
            scene.directionalLight.intensity = intensity;
        }

        if (prim.IsA<UsdLuxDomeLight>() && !scene.environmentMap.isLoaded())
        {
            UsdLuxDomeLight domeLight(prim);
            SdfAssetPath textureAsset;
            float intensity = 1.0f;
            GfVec3f color(1.0f, 1.0f, 1.0f);

            domeLight.GetTextureFileAttr().Get(&textureAsset, UsdTimeCode::Default());
            domeLight.GetIntensityAttr().Get(&intensity, UsdTimeCode::Default());
            domeLight.GetColorAttr().Get(&color, UsdTimeCode::Default());

            string resolvedPath = textureAsset.GetResolvedPath();

            if (resolvedPath.empty())
                resolvedPath = textureAsset.GetAssetPath();

            if (!resolvedPath.empty() && scene.environmentMap.load(resolvedPath))
            {
                scene.environmentMap.intensityScale = intensity;
                scene.environmentMap.tint = Color(color[0], color[1], color[2]);
            }
        }
    }

    // Extract materials
    for (const UsdPrim& prim : stage->Traverse())
    {

        if (!prim.IsA<UsdShadeMaterial>())
            continue;

        UsdShadeMaterial material(prim);
        string materialPath = prim.GetPath().GetString();

        Color diffuse(0.8f, 0.8f, 0.8f);
        Color emissiveColor(0.0f, 0.0f, 0.0f);
        float roughness = 1.0f;

        // Light source variables
        bool isEmissive = false;
        bool isSpotLight = false;
        float spotOuterAngle = 180.0f;
        float spotFalloffAngle = 180.0f;

        // Glass source variables
        bool isGlass = false;
        float indexOfRefraction = 1.5f;

        // Metal source variables
        bool isMetal = false;
        float metallic = 0.0f;

        // Plastic
        bool isPlastic = false;

        // Checker texture
        bool wantsSpatialChecker = false;
        float spatialCheckerCellSize = 1.0f;
        float spatialCheckerReduceContrast = 0.25f;

        for (const UsdShadeOutput& output : material.GetOutputs())
        {
            UsdShadeConnectableAPI source;
            TfToken sourceName;
            UsdShadeAttributeType sourceType;

            if (!UsdShadeConnectableAPI::GetConnectedSource(output, &source, &sourceName,
                                                            &sourceType))
                continue;

            UsdShadeShader shader(source.GetPrim());
            TfToken shaderId;
            shader.GetIdAttr().Get(&shaderId, UsdTimeCode::Default());

            if (shaderId != TfToken("UsdPreviewSurface"))
                continue;

            UsdShadeInput diffuseInput = shader.GetInput(TfToken("diffuseColor"));
            if (diffuseInput)
            {
                GfVec3f color;
                if (diffuseInput.Get(&color, UsdTimeCode::Default()))
                    diffuse = Color(color[0], color[1], color[2]);
            }

            UsdShadeInput roughnessInput = shader.GetInput(TfToken("roughness"));

            if (roughnessInput)
                roughnessInput.Get(&roughness, UsdTimeCode::Default());

            UsdShadeInput emissiveInput = shader.GetInput(TfToken("emissiveColor"));

            if (emissiveInput)
            {
                GfVec3f emission;
                if (emissiveInput.Get(&emission, UsdTimeCode::Default()))
                {
                    emissiveColor = Color(emission[0], emission[1], emission[2]);
                    isEmissive = (emission[0] + emission[1] + emission[2]) > 0.0f;
                }
            }

            // Spotlight
            UsdShadeInput spotOuterInput = shader.GetInput(TfToken("spotOuterAngle"));

            if (spotOuterInput)
                isSpotLight = spotOuterInput.Get(&spotOuterAngle, UsdTimeCode::Default());

            UsdShadeInput spotFalloffInput = shader.GetInput(TfToken("spotFalloffAngle"));

            if (spotFalloffInput)
                spotFalloffInput.Get(&spotFalloffAngle, UsdTimeCode::Default());

            // Glass
            UsdShadeInput iorInput = shader.GetInput(TfToken("ior"));

            if (iorInput)
                isGlass = iorInput.Get(&indexOfRefraction, UsdTimeCode::Default());

            // Metal
            UsdShadeInput metallicInput = shader.GetInput(TfToken("metallic"));

            if (metallicInput)
            {
                metallicInput.Get(&metallic, UsdTimeCode::Default());
                isMetal = metallic > 0.5f;
            }

            // Plastic
            UsdShadeInput plasticInput = shader.GetInput(TfToken("plastic"));

            if (plasticInput)
                plasticInput.Get(&isPlastic, UsdTimeCode::Default());

            // Checker texture
            UsdShadeInput spatialCheckerInput = shader.GetInput(TfToken("spatialChecker"));

            if (spatialCheckerInput)
                spatialCheckerInput.Get(&wantsSpatialChecker, UsdTimeCode::Default());

            UsdShadeInput cellSizeInput = shader.GetInput(TfToken("spatialCheckerCellSize"));

            if (cellSizeInput)
                cellSizeInput.Get(&spatialCheckerCellSize, UsdTimeCode::Default());
        }

        // Set UUID from prim path before adding to scene
        Material mat;

        if (isEmissive)
        {
            if (isSpotLight)
            {
                mat =
                    Material::makeSpotLight(emissiveColor, 1.0f, spotOuterAngle, spotFalloffAngle);
            }
            else
            {
                mat = Material::makeEmissive(emissiveColor, 1.0f);
            }
        }
        else if (isGlass)
        {
            mat = Material::makeGlass(indexOfRefraction);
        }
        else if (isMetal)
        {
            mat = Material::makeMetal(diffuse, roughness);
        }
        else if (isPlastic)
        {
            mat = Material::makePlastic(diffuse, roughness);
        }
        else
        {
            mat = Material::makeDiffuse(diffuse);
        }

        if (wantsSpatialChecker)
        {
            mat.useSpatialChecker = true;
            mat.spatialCheckerCellSize = spatialCheckerCellSize;

            Texture checkerPalette =
                TextureGenerator::generateSpatialPalette(512, spatialCheckerReduceContrast);
            checkerPalette.name = materialPath + " (Checker Palette)";

            mat.textureID = scene.addTexture(checkerPalette);
        }

        mat.uuid = materialPath;

        int id = scene.addMaterial(mat);
        materialIndexMap[materialPath] = id;
    }

    if (scene.materials.empty())
    {
        Material defaultMat = Material::makeDiffuse(Color(0.8f, 0.8f, 0.8f));
        defaultMat.uuid = "__default__";
        materialIndexMap["__default__"] = scene.addMaterial(defaultMat);
    }

    // Extract meshes
    int totalTriangles = 0;

    for (const UsdPrim& prim : stage->Traverse())
    {
        bool isMesh = prim.IsA<UsdGeomMesh>();
        bool isCube = prim.IsA<UsdGeomCube>();
        bool isSphere = prim.IsA<UsdGeomSphere>();
        bool isCylinder = prim.IsA<UsdGeomCylinder>();
        bool isRectLight = prim.IsA<UsdLuxRectLight>();
        bool isDiskLight = prim.IsA<UsdLuxDiskLight>();
        bool isSphereLight = prim.IsA<UsdLuxSphereLight>();
        bool isCylinderLight = prim.IsA<UsdLuxCylinderLight>();

        if (!isMesh && !isCube && !isSphere && !isCylinder && !isRectLight && !isDiskLight &&
            !isSphereLight && !isCylinderLight)
            continue;

        VtVec3fArray points;
        VtIntArray faceVertexIndices;
        VtIntArray faceVertexCounts;
        VtVec2fArray faceVaryingUVs;
        bool hasUVs = false;

        if (isMesh)
        {
            UsdGeomMesh meshPrim(prim);

            // Use VtValue to read point3f[] from the USD
            VtValue pointsVal;
            meshPrim.GetPointsAttr().Get(&pointsVal, UsdTimeCode::Default());

            if (pointsVal.IsEmpty())
            {
                cerr << "WARNING: " << prim.GetPath() << " has no points, skipping" << endl;
                continue;
            }

            points = pointsVal.Get<VtVec3fArray>();

            VtValue faceIndicesVal;
            VtValue faceCountsVal;

            meshPrim.GetFaceVertexIndicesAttr().Get(&faceIndicesVal, UsdTimeCode::Default());
            meshPrim.GetFaceVertexCountsAttr().Get(&faceCountsVal, UsdTimeCode::Default());

            if (faceIndicesVal.IsEmpty() || faceCountsVal.IsEmpty())
            {
                cerr << "WARNING: " << prim.GetPath() << "has no face data, skipping mesh" << endl;
                continue;
            }

            faceVertexIndices = faceIndicesVal.Get<VtIntArray>();
            faceVertexCounts = faceCountsVal.Get<VtIntArray>();

            // Logic to read UV faces
            UsdGeomPrimvar stPrimvar = UsdGeomPrimvarsAPI(prim).GetPrimvar(TfToken("st"));

            if (stPrimvar.IsDefined())
            {
                VtValue stValue;
                stPrimvar.Get(&stValue, UsdTimeCode::Default());

                if (stValue.IsHolding<VtVec2fArray>())
                {
                    faceVaryingUVs = stValue.UncheckedGet<VtVec2fArray>();
                    hasUVs = (faceVaryingUVs.size() == faceVertexIndices.size());
                }
            }
        }
        else if (isCube)
        {
            UsdGeomCube cubePrim(prim);
            double size = 2.0;

            cubePrim.GetSizeAttr().Get(&size, UsdTimeCode::Default());
            tessellateCube(size, points, faceVertexCounts, faceVertexIndices);
        }
        else if (isSphere)
        {
            UsdGeomSphere spherePrim(prim);
            double radius = 1.0;

            spherePrim.GetRadiusAttr().Get(&radius, UsdTimeCode::Default());
            tessellateSphere(radius, 16, 12, points, faceVertexCounts, faceVertexIndices);
        }
        else if (isCylinder)
        {
            UsdGeomCylinder cylinderPrim(prim);
            double radius = 1.0;
            double height = 2.0;

            cylinderPrim.GetRadiusAttr().Get(&radius, UsdTimeCode::Default());
            cylinderPrim.GetHeightAttr().Get(&height, UsdTimeCode::Default());
            tessellateCylinder(radius, height, 16, points, faceVertexCounts, faceVertexIndices);
        }
        else if (isRectLight)
        {
            UsdLuxRectLight rectLight(prim);
            double width = 1.0;
            double height = 1.0;

            rectLight.GetWidthAttr().Get(&width, UsdTimeCode::Default());
            rectLight.GetHeightAttr().Get(&height, UsdTimeCode::Default());
            buildRectLightQuad(width, height, points, faceVertexCounts, faceVertexIndices);
        }
        else if (isDiskLight)
        {
            UsdLuxDiskLight diskLight(prim);
            double radius = 0.5;

            diskLight.GetRadiusAttr().Get(&radius, UsdTimeCode::Default());
            buildDiskLightQuad(radius, 24, points, faceVertexCounts, faceVertexIndices);
        }
        else if (isSphereLight)
        {
            UsdLuxSphereLight sphereLight(prim);
            double radius = 0.5;

            sphereLight.GetRadiusAttr().Get(&radius, UsdTimeCode::Default());
            tessellateSphere(radius, 16, 12, points, faceVertexCounts, faceVertexIndices);
        }
        else if (isCylinderLight)
        {
            UsdLuxCylinderLight cylinderLight(prim);
            double radius = 0.5;
            double length = 1.0;

            cylinderLight.GetRadiusAttr().Get(&radius, UsdTimeCode::Default());
            cylinderLight.GetLengthAttr().Get(&length, UsdTimeCode::Default());
            tessellateCylinder(radius, length, 16, points, faceVertexCounts, faceVertexIndices);
        }

        // Apply world transform to every vertex
        GfMatrix4d worldTransform = xformCache.GetLocalToWorldTransform(prim);

        if (isZUp)
            worldTransform = worldTransform * upAxisCorrection;

        Mesh mesh;
        mesh.uuid = prim.GetPath().GetString();

        for (const GfVec3d& point : points)
        {
            GfVec4d worldPoint = GfVec4d(point[0], point[1], point[2], 1.0) * worldTransform;

            mesh.vertexPositions.push_back(Point3(static_cast<float>(worldPoint[0]),
                                                  static_cast<float>(worldPoint[1]),
                                                  static_cast<float>(worldPoint[2])));
        }

        // Triangulate faces
        int indexPos = 0;

        for (int count : faceVertexCounts)
        {
            triangulateFace(faceVertexIndices, indexPos, count, mesh.triangleIndices);

            if (hasUVs)
                triangulateFaceUVs(faceVaryingUVs, indexPos, count, mesh.triangleUVsU,
                                   mesh.triangleUVsV);

            indexPos += count;
        }

        // Resolve material
        UsdShadeMaterialBindingAPI bindingAPI(prim);
        UsdShadeMaterial boundMaterial = bindingAPI.ComputeBoundMaterial();

        // Determine if the Mesh is a light source
        bool isAreaLight = isRectLight || isDiskLight || isSphereLight || isCylinderLight;

        mesh.materialID = 0;

        if (isAreaLight)
        {
            UsdLuxLightAPI lightAPI(prim);
            float intensity = 1.0f;
            GfVec3f color(1.0f, 1.0f, 1.0f);

            lightAPI.GetIntensityAttr().Get(&intensity, UsdTimeCode::Default());
            lightAPI.GetColorAttr().Get(&color, UsdTimeCode::Default());

            Color emissiveColor(color[0], color[1], color[2]);
            Material material = Material::makeEmissive(emissiveColor, intensity);
            material.uuid = prim.GetPath().GetString();
            mesh.materialID = scene.addMaterial(material);
        }
        else if (boundMaterial)
        {
            string matPath = boundMaterial.GetPath().GetString();

            auto iterator = materialIndexMap.find(matPath);

            if (iterator != materialIndexMap.end())
                mesh.materialID = iterator->second;
        }
        else
        {
            Color displayColor = readDisplayColor(prim);
            string colorKey = "displayColor_" + to_string(int(displayColor.x * 255)) + "_" +
                              to_string(int(displayColor.y * 255)) + "_" +
                              to_string(int(displayColor.z * 255));

            auto iterator = materialIndexMap.find(colorKey);

            if (iterator != materialIndexMap.end())
            {
                mesh.materialID = iterator->second;
            }
            else
            {
                Material material = Material::makeDiffuse(displayColor);
                material.uuid = "displayColor #" + colorToHex(displayColor);
                int id = scene.addMaterial(material);
                materialIndexMap[colorKey] = id;
                mesh.materialID = id;
            }
        }

        totalTriangles += mesh.triangleCount();
        scene.addMesh(mesh);
    }

    printSceneSummary(usdFilePath, scene, totalTriangles, isZUp);

    return scene;
};