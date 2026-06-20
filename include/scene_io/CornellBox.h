#pragma once

#include <core/Scene.h>
#include <geometry/Mesh.h>
#include <shading/Material.h>
#include <math/Vec3.h>

struct CornellBox {
    static void addQuad(Mesh& mesh, const Point3& bottomLeft, const Point3& bottomRight, 
        const Point3& topRight, const Point3& topLeft) {
        int firstVertexIndex = static_cast<int>(mesh.vertexPositions.size());

        mesh.vertexPositions.push_back(bottomLeft);
        mesh.vertexPositions.push_back(bottomRight);
        mesh.vertexPositions.push_back(topRight);
        mesh.vertexPositions.push_back(topLeft);

        // Two triangles forming the quad
        mesh.triangleIndices.push_back(firstVertexIndex + 0);
        mesh.triangleIndices.push_back(firstVertexIndex + 1);
        mesh.triangleIndices.push_back(firstVertexIndex + 2);

        mesh.triangleIndices.push_back(firstVertexIndex + 0);
        mesh.triangleIndices.push_back(firstVertexIndex + 2);
        mesh.triangleIndices.push_back(firstVertexIndex + 3);
    }

    static Scene build() {
        Scene scene;

        // Materials
        int whiteMaterial = scene.addMaterial(Material::makeDiffuse(Color(0.73f, 0.73f, 0.73f)));
        int redMaterial = scene.addMaterial(Material::makeDiffuse(Color(0.65f, 0.05f, 0.05f)));
        int greenMaterial = scene.addMaterial(Material::makeDiffuse(Color(0.12f, 0.45f, 0.15f)));
        int lightMaterial = scene.addMaterial(Material::makeEmissive(Color(1.0f, 1.0f, 1.0f), 15.0f));

        float boxHalfWidth = 5.0f;
        float boxHeight = boxHalfWidth * 2.0f;

        // Floor
        Mesh floorMesh;
        floorMesh.materialID = whiteMaterial;
        addQuad(floorMesh,
            Point3(-boxHalfWidth, 0.0f, -boxHalfWidth),
            Point3( boxHalfWidth, 0.0f, -boxHalfWidth),
            Point3( boxHalfWidth, 0.0f, boxHalfWidth),
            Point3(-boxHalfWidth, 0.0f, boxHalfWidth));
        scene.addMesh(floorMesh);

        // Ceiling
        Mesh ceilingMesh;
        ceilingMesh.materialID = whiteMaterial;
        addQuad(ceilingMesh,
            Point3(-boxHalfWidth, boxHeight, boxHalfWidth),
            Point3( boxHalfWidth, boxHeight, boxHalfWidth),
            Point3( boxHalfWidth, boxHeight, -boxHalfWidth),
            Point3(-boxHalfWidth, boxHeight, -boxHalfWidth));
        scene.addMesh(ceilingMesh);

        // Back wall
        Mesh backWallMesh;
        backWallMesh.materialID = whiteMaterial;
        addQuad(backWallMesh,
            Point3(-boxHalfWidth, 0.0f, -boxHalfWidth),
            Point3(-boxHalfWidth, boxHeight, -boxHalfWidth),
            Point3( boxHalfWidth, boxHeight, -boxHalfWidth),
            Point3( boxHalfWidth, 0.0f, -boxHalfWidth));
        scene.addMesh(backWallMesh);

        // Left wall
        Mesh leftWallMesh;
        leftWallMesh.materialID = redMaterial;
        addQuad(leftWallMesh,
            Point3(-boxHalfWidth, 0.0f, boxHalfWidth),
            Point3(-boxHalfWidth, boxHeight, boxHalfWidth),
            Point3(-boxHalfWidth, boxHeight, -boxHalfWidth),
            Point3(-boxHalfWidth, 0.0f, -boxHalfWidth));
        scene.addMesh(leftWallMesh);

        // Right wall
        Mesh rightWallMesh;
        rightWallMesh.materialID = greenMaterial;
        addQuad(rightWallMesh,
            Point3(boxHalfWidth, 0.0f, -boxHalfWidth),
            Point3(boxHalfWidth, boxHeight, -boxHalfWidth),
            Point3(boxHalfWidth, boxHeight, boxHalfWidth),
            Point3(boxHalfWidth, 0.0f, boxHalfWidth));
        scene.addMesh(rightWallMesh);

        // Ceiling Light
        Mesh lightMesh;
        lightMesh.materialID = lightMaterial;
        float lightHalfWidth = boxHalfWidth * 0.3f;
        float lightHeight = boxHeight - 0.01f;
        addQuad(lightMesh,
            Point3(-lightHalfWidth, lightHeight,  lightHalfWidth),
            Point3( lightHalfWidth, lightHeight,  lightHalfWidth),
            Point3( lightHalfWidth, lightHeight, -lightHalfWidth),
            Point3(-lightHalfWidth, lightHeight, -lightHalfWidth));
        scene.addMesh(lightMesh);

        // Inside Objects
        int matteGreyMaterial = scene.addMaterial(Material::makeDiffuse(Color(0.73f, 0.73f, 0.73f)));
        int metalMaterial = scene.addMaterial(Material::makeMetal(Color(0.8f, 0.8f, 0.8f), 0.1f));

        scene.addSphere(Sphere(Point3(-1.7f, 1.5f, -1.0f), 1.5f, matteGreyMaterial));
        scene.addSphere(Sphere(Point3( 1.7f, 1.0f,  1.0f), 1.0f, metalMaterial));

        return scene;
    }
};