#include <scene/CornellBox.h>
#include <geometry/Mesh.h>
#include <shading/Material.h>

using std::cos;
using std::sin;

const float PI = 3.14159265f;

static Point3 rotateY(const Point3& point, const Point3& pivot, float angleDegrees) {
    float angleRadians = angleDegrees * PI / 180.0f;
    float cosAngle = cos(angleRadians);
    float sinAngle = sin(angleRadians);

    // Translate to pivot origin
    float localX = point.x - pivot.x;
    float localZ = point.z - pivot.z;

    // Rotate in the XZ plane
    float rotatedX = cosAngle * localX - sinAngle * localZ;
    float rotatedZ = sinAngle * localX + cosAngle * localZ;

    // Translate back
    return Point3(rotatedX + pivot.x, point.y, rotatedZ + pivot.z);
}

void CornellBox::addQuad(Mesh& mesh, const Point3& bottomLeft, const Point3& bottomRight, 
    const Point3& topRight, const Point3& topLeft) {
    int firstVertexIndex = static_cast<int>(mesh.vertexPositions.size());

    mesh.vertexPositions.push_back(bottomLeft);
    mesh.vertexPositions.push_back(bottomRight);
    mesh.vertexPositions.push_back(topRight);
    mesh.vertexPositions.push_back(topLeft);

    mesh.triangleIndices.push_back(firstVertexIndex + 0);
    mesh.triangleIndices.push_back(firstVertexIndex + 1);
    mesh.triangleIndices.push_back(firstVertexIndex + 2);

    mesh.triangleIndices.push_back(firstVertexIndex + 0);
    mesh.triangleIndices.push_back(firstVertexIndex + 2);
    mesh.triangleIndices.push_back(firstVertexIndex + 3);
}

void CornellBox::addRotatedBox(Scene& scene, int materialID, const Point3& minCorner, const Point3& maxCorner,
    float rotationDegrees) {
    float x0 = minCorner.x, y0 = minCorner.y, z0 = minCorner.z;
    float x1 = maxCorner.x, y1 = maxCorner.y, z1 = maxCorner.z;

    // Pivot is the centre of the box base on the floor
    Point3 pivot((x0 + x1) * 0.5f, y0, (z0 + z1) * 0.5f);

    // The 8 corners of the box
    Point3 corners[8] = {
        Point3(x0, y0, z0), Point3(x1, y0, z0),
        Point3(x1, y0, z1), Point3(x0, y0, z1),
        Point3(x0, y1, z0), Point3(x1, y1, z0),
        Point3(x1, y1, z1), Point3(x0, y1, z1)
    };

    // Rotate every corner
    for (int i = 0; i < 8; ++i)
        corners[i] = rotateY(corners[i], pivot, rotationDegrees);

    Mesh bottom; 
    bottom.materialID = materialID;
    addQuad(bottom, corners[0], corners[1], corners[2], corners[3]);
    scene.addMesh(bottom);

    Mesh top; 
    top.materialID = materialID;
    addQuad(top, corners[7], corners[6], corners[5], corners[4]);
    scene.addMesh(top);

    Mesh front; 
    front.materialID = materialID;
    addQuad(front, corners[3], corners[2], corners[6], corners[7]);
    scene.addMesh(front);

    Mesh back; 
    back.materialID = materialID;
    addQuad(back, corners[1], corners[0], corners[4], corners[5]);
    scene.addMesh(back);

    Mesh left; 
    left.materialID = materialID;
    addQuad(left, corners[0], corners[3], corners[7], corners[4]);
    scene.addMesh(left);

    Mesh right; 
    right.materialID = materialID;
    addQuad(right, corners[2], corners[1], corners[5], corners[6]);
    scene.addMesh(right);
}

Scene CornellBox::build() {
    Scene scene;

    int whiteMaterial = scene.addMaterial(Material::makeDiffuse(Color(0.73f, 0.73f, 0.73f)));
    int redMaterial = scene.addMaterial(Material::makeDiffuse(Color(0.65f, 0.05f, 0.05f)));
    int greenMaterial = scene.addMaterial(Material::makeDiffuse(Color(0.12f, 0.45f, 0.15f)));
    int lightMaterial = scene.addMaterial(Material::makeEmissive(Color(1.0f, 1.0f, 1.0f), 15.0f));

    float roomHalfWidth = 5.0f;
    float roomHeight = 10.0f;
    float roomDepth = 10.0f;

    Mesh floorMesh;
    floorMesh.materialID = whiteMaterial;
    addQuad(floorMesh,
        Point3(-roomHalfWidth, 0.0f, -roomDepth),
        Point3(roomHalfWidth, 0.0f, -roomDepth),
        Point3(roomHalfWidth, 0.0f, roomHalfWidth),
        Point3(-roomHalfWidth, 0.0f, roomHalfWidth));
    scene.addMesh(floorMesh);

    Mesh ceilingMesh;
    ceilingMesh.materialID = whiteMaterial;
    addQuad(ceilingMesh,
        Point3(-roomHalfWidth, roomHeight, roomHalfWidth),
        Point3(roomHalfWidth, roomHeight, roomHalfWidth),
        Point3(roomHalfWidth, roomHeight, -roomDepth),
        Point3(-roomHalfWidth, roomHeight, -roomDepth));
    scene.addMesh(ceilingMesh);

    Mesh backWallMesh;
    backWallMesh.materialID = whiteMaterial;
    addQuad(backWallMesh,
        Point3(-roomHalfWidth, 0.0f, -roomDepth),
        Point3(-roomHalfWidth, roomHeight, -roomDepth),
        Point3(roomHalfWidth, roomHeight, -roomDepth),
        Point3(roomHalfWidth, 0.0f, -roomDepth));
    scene.addMesh(backWallMesh);

    Mesh leftWallMesh;
    leftWallMesh.materialID = redMaterial;
    addQuad(leftWallMesh,
        Point3(-roomHalfWidth, 0.0f, roomHalfWidth),
        Point3(-roomHalfWidth, roomHeight, roomHalfWidth),
        Point3(-roomHalfWidth, roomHeight, -roomDepth),
        Point3(-roomHalfWidth, 0.0f, -roomDepth));
    scene.addMesh(leftWallMesh);

    Mesh rightWallMesh;
    rightWallMesh.materialID = greenMaterial;
    addQuad(rightWallMesh,
        Point3(roomHalfWidth, 0.0f, -roomDepth),
        Point3(roomHalfWidth, roomHeight, -roomDepth),
        Point3(roomHalfWidth, roomHeight, roomHalfWidth),
        Point3(roomHalfWidth, 0.0f, roomHalfWidth));
    scene.addMesh(rightWallMesh);

    Mesh lightMesh;
    lightMesh.materialID = lightMaterial;
    float lightHalfWidth = roomHalfWidth * 0.3f;
    float lightHeight = roomHeight - 0.01f;
    addQuad(lightMesh,
        Point3(-lightHalfWidth, lightHeight, lightHalfWidth),
        Point3(lightHalfWidth, lightHeight, lightHalfWidth),
        Point3(lightHalfWidth, lightHeight, -lightHalfWidth),
        Point3(-lightHalfWidth, lightHeight, -lightHalfWidth));
    scene.addMesh(lightMesh);

    int tallerBoxMaterial = scene.addMaterial(Material::makeDiffuse(Color(0.73f, 0.73f, 0.73f)));
    addRotatedBox(scene, tallerBoxMaterial, Point3(-3.5f, 0.0f, -5.5f), 
        Point3(-0.5f, 6.0f, -2.5f), -15.0f);

    int shorterBoxMaterial = scene.addMaterial(Material::makeDiffuse(Color(0.73f, 0.73f, 0.73f)));
    addRotatedBox(scene, shorterBoxMaterial, Point3(0.1f, 0.0f, -2.0f),
        Point3(3.1f, 3.0f, 1.0f), 18.0f);

    return scene;
}