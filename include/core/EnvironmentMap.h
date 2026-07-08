#pragma once

#include <string>
#include <vector>
#include <math/Vec3.h>
#include <shading/Material.h>

class EnvironmentMap
{
  public:
    bool load(const std::string& filePath);
    bool isLoaded() const
    {
        return loaded;
    };
    Color sample(const Vec3& direction) const;

    float intensityScale = 1.0f;
    Color tint = Color(1.0f, 1.0f, 1.0f);

  private:
    std::vector<float> pixels;
    int width = 0;
    int height = 0;
    bool loaded = false;
};