#include <scene/CameraLoader.h>
#include <core/PrintUtils.h>
#include <iostream>

#ifdef ENABLE_USD
#include <pxr/usd/usd/stage.h>
#include <pxr/usd/usd/prim.h>
#include <pxr/base/gf/vec3f.h>
#include <pxr/usd/usd/attribute.h>
PXR_NAMESPACE_USING_DIRECTIVE
#endif

using std::cerr;
using std::cout;
using std::endl;
using std::string;
using std::to_string;

Camera CameraLoader::defaultCornellBox(int imageWidth, int imageHeight)
{
    return Camera(Point3(0.0f, 5.0f, 15.0f), Point3(0.0f, 5.0f, -2.0f), Vec3(0.0f, 1.0f, 0.0f),
                  53.0f, imageWidth, imageHeight);
}

Camera CameraLoader::defaultKitchenSet(int imageWidth, int imageHeight)
{
    return Camera(Point3(240.0f, 200.0f, 400.0f), Point3(71.0f, 60.0f, 0.0f),
                  Vec3(0.0f, 1.0f, 0.0f), 50.0f, imageWidth, imageHeight);
}

Camera CameraLoader::load(const string& cameraFilePath, int imageWidth, int imageHeight)
{
#ifdef ENABLE_USD
    UsdStageRefPtr stage = UsdStage::Open(cameraFilePath);

    if (!stage)
    {
        cerr << "WARNING: Could not open camera file: " << cameraFilePath << endl;

        return defaultCornellBox(imageWidth, imageHeight);
    }

    UsdPrim cameraPrim = stage->GetDefaultPrim();

    if (!cameraPrim.IsValid())
    {
        cerr << "WARNING: No default prim in camera file" << endl;

        return defaultCornellBox(imageWidth, imageHeight);
    }

    // Read custom camera attributes
    auto readVec3 = [&](const string& name, GfVec3f fallback) -> GfVec3f
    {
        UsdAttribute attribute = cameraPrim.GetAttribute(TfToken(name));

        if (!attribute.IsValid())
            return fallback;

        GfVec3f value;
        attribute.Get(&value);

        return value;
    };

    auto readFloat = [&](const string& name, float fallback) -> float
    {
        UsdAttribute attribute = cameraPrim.GetAttribute(TfToken(name));

        if (!attribute.IsValid())
            return fallback;

        float value;
        attribute.Get(&value);

        return value;
    };

    GfVec3f eye = readVec3("custom:eye", GfVec3f(0, 5, 15));
    GfVec3f target = readVec3("custom:target", GfVec3f(0, 5, -2));
    GfVec3f up = readVec3("custom:up", GfVec3f(0, 1, 0));
    float fov = readFloat("custom:fov", 53.0f);

    printStatsBlock("Camera Information ",
                    {
                        "Camera : " + cameraFilePath,
                        "Eye    : (" + to_string(eye[0]) + ", " + to_string(eye[1]) + ", " +
                            to_string(eye[2]) + ")",
                        "Target : (" + to_string(target[0]) + ", " + to_string(target[1]) + ", " +
                            to_string(target[2]) + ")",
                        "FOV    : " + to_string(fov),
                    });

    return Camera(Point3(eye[0], eye[1], eye[2]), Point3(target[0], target[1], target[2]),
                  Vec3(up[0], up[1], up[2]), fov, imageWidth, imageHeight);
#else
    return defaultCornellBox(imageWidth, imageHeight);
#endif
}