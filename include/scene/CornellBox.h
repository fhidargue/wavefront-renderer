#pragma once

#include <core/Scene.h>

struct CornellBox
{
    static Scene build();

  private:
    static void addQuad(Mesh& mesh, const Point3& bottomLeft, const Point3& bottomRight,
                        const Point3& topRight, const Point3& topLeft);

    static void addBox(Scene& scene, int materialID, const Point3& minCorner,
                       const Point3& maxCorner);

    static void addRotatedBox(Scene& scene, int materialID, const Point3& minCorner,
                              const Point3& maxCorner, float rotationDegrees);
};