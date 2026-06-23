#pragma once

#include <string>
#include <fstream>
#include <iostream>
#include <core/Scene.h>
#include <shading/Material.h>
#include <geometry/Mesh.h>
#include <math/Vec3.h>

#include "../external/json.hpp"

using nlohmann::json;

// Loads a Scene from a JSON file produced by the Python script

struct JsonSceneLoader {
    static Scene load(const std::string& filePath) {
        std::ifstream file(filePath);

        if (!file.is_open()) {
            std::cerr << "ERROR: Could not open the scene file: " << filePath << std::endl;

            return Scene();
        }

        // Parse the JSON file
        json sceneJson;
        file >> sceneJson;

        Scene scene;

        // Load the materials form JSON
        for (const auto& matJson : sceneJson["materials"]) {
            std::string type = matJson["type"].get<std::string>();

            Color albedo(
                matJson["albedo"][0].get<float>(),
                matJson["albedo"][1].get<float>(),
                matJson["albedo"][2].get<float>()
            );

            if (type == "emissive") {
                float strength = matJson["emissionStrength"].get<float>();
                scene.addMaterial(Material::makeEmissive(albedo, strength));
            } else if (type == "metal") {
                float roughness = matJson["roughness"].get<float>();
                scene.addMaterial(Material::makeMetal(albedo, roughness));
            } else {
                scene.addMaterial(Material::makeDiffuse(albedo));
            }
        }

        // Load the meshes from JSON
        for (const auto& meshJson : sceneJson["meshes"]) {
            Mesh mesh;
            mesh.materialID = meshJson["material_id"].get<int>();

            // Read vertex positions
            for (const auto& vertex : meshJson["vertices"]) {
                mesh.vertexPositions.push_back(Point3(
                    vertex[0].get<float>(),
                    vertex[1].get<float>(),
                    vertex[2].get<float>()
                ));
            }

            // Read triangle indices
            for (const auto& index : meshJson["indices"])
                mesh.triangleIndices.push_back(index.get<int>());

            scene.addMesh(mesh);
        }

        // Print loaded JSON content
        int totalTriangles = 0;
        for (const auto& mesh : scene.meshes) 
            totalTriangles += mesh.triangleCount();

        std::cout << "Scene loaded from: " << filePath << std::endl;
        std::cout << " Materials  : " << scene.materials.size() << std::endl;
        std::cout << " Meshes     : " << scene.meshes.size() << std::endl;
        std::cout << " Triangles  : " << totalTriangles << std::endl;

        return scene;
    }
};