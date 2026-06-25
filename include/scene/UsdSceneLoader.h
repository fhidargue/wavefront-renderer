#pragma once

#include <string>
#include <core/Scene.h>

struct UsdSceneLoader
{
    static Scene load(const std::string& usdFilePath);
};