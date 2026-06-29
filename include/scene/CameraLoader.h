#pragma once

#include <string>
#include <core/Camera.h>

struct CameraLoader
{
    static Camera load(const std::string& cameraFilePath, int imageWidth, int imageHeight);
    static Camera defaultCornellBox(int imageWidth, int imageHeight);
    static Camera defaultKitchenSet(int imageWidth, int imageHeight);
};