#include <scene/UsdSceneLoader.h>
#include <geometry/Mesh.h>
#include <shading/Material.h>
#include <math/Vec3.h>

#include <iostream>
#include <unordered_map>
#include <cstdio>

#include <pxr/usd/usd/stage.h>
#include <pxr/usd/usd/primRange.h>
#include <pxr/usd/usdGeom/mesh.h>
#include <pxr/usd/usdGeom/xformCache.h>
#include <pxr/usd/usdGeom/mesh.h>
#include <pxr/usd/usdGeom/metrics.h>
#include <pxr/usd/usdGeom/primvarsAPI.h>
#include <pxr/usd/usdShade/material.h>
#include <pxr/usd/usdShade/materialBindingAPI.h>
#include <pxr/usd/usdShade/shader.h>
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

static Color readDisplayColor(const UsdGeomMesh& meshPrim)
{
    // Fallback implementation for scenes without UsdPreviewSurface
    const Color defaultColor = Color(0.8f, 0.8f, 0.8f);
    UsdGeomPrimvar displayColorPrimvar =
        UsdGeomPrimvarsAPI(meshPrim.GetPrim()).GetPrimvar(TfToken("displayColor"));

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
    int emissiveCount = 0;

    for (const auto& mat : scene.materials)
    {
        if (mat.type == MaterialType::Diffuse)
            diffuseCount++;
        if (mat.type == MaterialType::Emissive)
            emissiveCount++;
    }

    cout << "\n========================================" << endl;
    cout << "  Scene Summary" << endl;
    cout << "========================================" << endl;
    cout << "  File       : " << usdFilePath << endl;
    cout << "  upAxis     : " << (isZUp ? "Z (corrected to Y)" : "Y") << endl;
    cout << "  Materials  : " << scene.materials.size() << " (" << diffuseCount << " diffuse, "
         << emissiveCount << " emissive)" << endl;
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
        bool isEmissive = false;

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
        }

        // Set UUID from prim path before adding to scene
        Material mat = isEmissive ? Material::makeEmissive(emissiveColor, 1.0f)
                                  : Material::makeDiffuse(diffuse);

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
    UsdGeomXformCache xformCache;
    int totalTriangles = 0;

    for (const UsdPrim& prim : stage->Traverse())
    {
        if (!prim.IsA<UsdGeomMesh>())
            continue;

        UsdGeomMesh meshPrim(prim);

        // Use VtValue to read point3f[] from the USD
        VtValue pointsVal;
        meshPrim.GetPointsAttr().Get(&pointsVal, UsdTimeCode::Default());

        if (pointsVal.IsEmpty())
        {
            cerr << "WARNING: " << prim.GetPath() << " has no points, skipping" << endl;
            continue;
        }

        VtVec3fArray points = pointsVal.Get<VtVec3fArray>();

        VtValue faceIndicesVal;
        VtValue faceCountsVal;
        meshPrim.GetFaceVertexIndicesAttr().Get(&faceIndicesVal, UsdTimeCode::Default());
        meshPrim.GetFaceVertexCountsAttr().Get(&faceCountsVal, UsdTimeCode::Default());

        if (faceIndicesVal.IsEmpty() || faceCountsVal.IsEmpty())
        {
            cerr << "WARNING: " << prim.GetPath() << "has no face data, skipping mesh" << endl;
            continue;
        }

        VtIntArray faceVertexIndices = faceIndicesVal.Get<VtIntArray>();
        VtIntArray faceVertexCounts = faceCountsVal.Get<VtIntArray>();

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
            indexPos += count;
        }

        // Resolve material
        UsdShadeMaterialBindingAPI bindingAPI(prim);
        UsdShadeMaterial boundMaterial = bindingAPI.ComputeBoundMaterial();

        mesh.materialID = 0;

        if (boundMaterial)
        {
            string matPath = boundMaterial.GetPath().GetString();

            auto iterator = materialIndexMap.find(matPath);

            if (iterator != materialIndexMap.end())
                mesh.materialID = iterator->second;
        }
        else
        {
            Color displayColor = readDisplayColor(meshPrim);
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