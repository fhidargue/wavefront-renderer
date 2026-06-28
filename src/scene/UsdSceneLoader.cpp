#include <scene/UsdSceneLoader.h>
#include <geometry/Mesh.h>
#include <shading/Material.h>
#include <math/Vec3.h>

#include <iostream>
#include <unordered_map>

#include <pxr/usd/usd/stage.h>
#include <pxr/usd/usd/primRange.h>
#include <pxr/usd/usdGeom/mesh.h>
#include <pxr/usd/usdGeom/xformCache.h>
#include <pxr/usd/usdGeom/mesh.h>
#include <pxr/usd/usdShade/material.h>
#include <pxr/usd/usdShade/materialBindingAPI.h>
#include <pxr/usd/usdShade/shader.h>
#include <pxr/usd/usd/timeCode.h>
#include <pxr/base/gf/vec3f.h>
#include <pxr/base/gf/vec4d.h>
#include <pxr/base/gf/matrix4d.h>
#include <pxr/base/vt/array.h>

using std::cerr;
using std::cout;
using std::endl;
using std::string;
using std::vector;

PXR_NAMESPACE_USING_DIRECTIVE

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

Scene UsdSceneLoader::load(const string& usdFilePath)
{
    UsdStageRefPtr stage = UsdStage::Open(usdFilePath);

    if (!stage)
    {
        cerr << "ERROR: Could not open: " << usdFilePath << endl;
        return Scene();
    }

    Scene scene;
    std::unordered_map<string, int> materialIndexMap;

    // Extract materials
    for (const UsdPrim& prim : stage->Traverse())
    {

        if (!prim.IsA<UsdShadeMaterial>())
            continue;

        UsdShadeMaterial material(prim);
        string materialPath = prim.GetPath().GetString();

        Color diffuse(0.8f, 0.8f, 0.8f);
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
                    isEmissive = (emission[0] + emission[1] + emission[2]) > 0.0f;
            }
        }

        // Set UUID from prim path before adding to scene
        Material mat =
            isEmissive ? Material::makeEmissive(diffuse, 15.0f) : Material::makeDiffuse(diffuse);

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

        if (boundMaterial)
        {
            string matPath = boundMaterial.GetPath().GetString();

            auto iterator = materialIndexMap.find(matPath);

            if (iterator != materialIndexMap.end())
                mesh.materialID = iterator->second;
        }

        totalTriangles += mesh.triangleCount();
        scene.addMesh(mesh);
    }

    cout << "Scene loaded from USD: " << usdFilePath << endl;

    return scene;
};