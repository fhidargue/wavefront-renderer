#include <iostream>
#include <cstdlib>
#include "Vec3.h"
#include "Ray.h"
#include "Camera.h"
#include "Image.h"
#include "Scene.h"
#include "HitRecord.h"
#include "Material.h"

using std::cout;
using std::endl;
using std::srand;

// Trace a light ray
Color traceRay(const Ray& ray, const Scene& scene, int maxDepth) {
    if (maxDepth <= 0)
        return Color(0.0f, 0.0f, 0.0f);

    HitRecord record;

    if (scene.hit(ray, 0.001f, 1e9f, record)) {
        const Material& material = scene.getMaterial(record);
        
        Color emittedLight = material.emitted();

        // Logic for when the ray is scattered
        Color attenuation;
        Ray scattered;

        if (material.scatter(ray, record, attenuation, scattered)) {
            Color bouncedLight = traceRay(scattered, scene, maxDepth - 1);

            return emittedLight + attenuation * bouncedLight;
        }

        return emittedLight;
    }
}

int main() {
    srand(42);

    const int WIDTH = 800;
    const int HEIGHT = 450;
    const int SAMPLES = 32; 
    const int MAX_DEPTH = 8; 

    Scene scene;

    // Build materials
    int diffuseGray = scene.addMaterial(Material::makeDiffuse(Color(0.5f, 0.5f, 0.5f)));
    int diffuseRed = scene.addMaterial(Material::makeDiffuse(Color(0.8f, 0.2f, 0.2f)));
    int metalSilver = scene.addMaterial(Material::makeMetal(Color(0.8f, 0.8f, 0.8f), 0.1f));
    int metalGold = scene.addMaterial(Material::makeMetal(Color(0.8f, 0.6f, 0.2f), 0.3f));
    int lightMat = scene.addMaterial(Material::makeEmissive(Color(1.0f, 0.9f, 0.8f), 3.0f));

    // Build scene
    scene.addSphere(Sphere(Point3( 0.0f, -100.5f, -1.0f), 100.0f, diffuseGray));  // floor
    scene.addSphere(Sphere(Point3( 0.0f, 0.0f, -1.0f), 0.5f, diffuseRed));  
    scene.addSphere(Sphere(Point3(-1.1f, 0.0f, -1.0f), 0.5f, metalSilver));
    scene.addSphere(Sphere(Point3( 1.1f, 0.0f, -1.0f), 0.5f, metalGold));   
    scene.addSphere(Sphere(Point3( 0.0f, 1.5f, -1.0f), 0.3f, lightMat));    

    Image image(WIDTH, HEIGHT);
    Camera camera(WIDTH, HEIGHT);

    cout << "Rendering " << WIDTH << "x" << HEIGHT << " image..." << endl;

    for (int y = 0; y < HEIGHT; ++y) {
        // Progress indicator
        if (y % 50 == 0)
            cout << "Row " << y << " / " << HEIGHT << endl;

        for (int x = 0; x < WIDTH; ++x)
        {
            Color pixelColor(0.0f, 0.0f, 0.0f);

            // Multiple samples per pixel to reduce noise
            for (int sample = 0; sample < SAMPLES; ++sample) {
                float u = (static_cast<float>(x) + randomFloat()) / (WIDTH  - 1);
                float v = 1.0f - (static_cast<float>(y) + randomFloat()) / (HEIGHT - 1);

                Ray ray = camera.getRay(u, v);
                pixelColor = pixelColor + traceRay(ray, scene, MAX_DEPTH);
            }

            // Average the samples
            pixelColor = pixelColor / static_cast<float>(SAMPLES);

            // Convert linear to sRGB for gamma correction
            pixelColor.x = std::sqrt(pixelColor.x);
            pixelColor.y = std::sqrt(pixelColor.y);
            pixelColor.z = std::sqrt(pixelColor.z);

            image.setPixel(x, y, pixelColor);
        }
    }

    image.writePPMFile("output/render.ppm");

    return 0;
}