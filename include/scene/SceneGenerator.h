#pragma once

#include <vector>
#include <cstdlib>
#include <core/Scene.h>
#include <shading/Material.h>
#include <shading/Texture.h>
#include <geometry/Sphere.h>
#include <math/Random.h>

using std::vector;

struct SceneGenerator {
    static Scene generateGridScene(int gridSize, int materialCount, int textureSize = 0) {
        Scene scene;
        vector<int> textureIDs;

        if (textureSize > 0) {
            for (int i = 0; i < materialCount; ++i) {
                Texture tex = TextureGenerator::generateNoise(textureSize);
                int texID = scene.addTexture(tex);
                textureIDs.push_back(texID);
            }
        }

        vector<int> materialIDs;

        for (int i = 0; i < materialCount; ++i) {
            MaterialType type = static_cast<MaterialType>(i % 3);

            Color randomColor(
                0.3f + randomFloat() * 0.6f,
                0.3f + randomFloat() * 0.6f,
                0.3f + randomFloat() * 0.6f
            );

            int assignedTexture = textureSize > 0 ? textureIDs[i] : -1;

            int id;
            if (type == MaterialType::Diffuse) {
                id = scene.addMaterial(Material::makeDiffuse(randomColor, assignedTexture));
            } else if (type == MaterialType::Metal) {
                float roughness = randomFloat() * 0.5f;
                id = scene.addMaterial(Material::makeMetal(randomColor, roughness, assignedTexture));
            } else {
                id = scene.addMaterial(Material::makeEmissive(randomColor, 1.0f + randomFloat() * 2.0f));
            }

            materialIDs.push_back(id);
        }

        int groundMat = scene.addMaterial(Material::makeDiffuse(Color(0.5f, 0.5f, 0.5f)));
        scene.addSphere(Sphere(Point3(0.0f, -1000.5f, -1.0f), 1000.0f, groundMat));

        float spacing = 1.2f;
        float radius = 0.45f;
        float offset = (gridSize - 1) * spacing * 0.5f;

        for (int row = 0; row < gridSize; ++row) {
            for (int col = 0; col < gridSize; ++col) {
                float x = col * spacing - offset;
                float z = -1.0f - row * spacing;
                float y = 0.0f;

                int chosenMaterial = materialIDs[std::rand() % materialIDs.size()];
                scene.addSphere(Sphere(Point3(x, y, z), radius, chosenMaterial));
            }
        }

        return scene;
    }
};