#pragma once
#include "raytracer.hpp"

#include "math/math.hpp"


#include "raytracer/azReflection.hpp"
#include "raytracer/constants.h"

#include "scene/scene.hpp"
#include "scene/ray.hpp"
#include "scene/model.hpp"
#include "scene/triangle.hpp"
#include "scene/sphere.hpp"

#include "utils/Parallel.h"
#include "Utils.h"
#include <mpi.h>

#include <SDL_timer.h>
#include <iostream>

namespace _462 {
    
    class azMPIRaytrace
    {
    public:
        azMPIRaytrace() {}
        azMPIRaytrace(Scene *aScene, FrameBuffer *fbuffer) {
            scene = aScene;
            buffer = fbuffer;
        }
        
        // generate all ray list
        void generateEyeRay(vector<Ray> &eyerays);
        
        // check first boundbing box
        int checkNextBoundingBox(Ray &ray, int procId);
        
        // local ray trace
        int localRaytrace(Ray &ray);
        
        // generate shadow ray by given input ray, maxt would be used
        void generateShadowRay(Ray &ray, Ray &shadowRay);
        
        // TODO: generate gi ray
        void generateGIRay(Ray &ray, Ray &giRay);
        
        // TODO: write to framebuffer
        void updateFrameBuffer(Ray &ray);
        

    private:
        Scene *scene;
        FrameBuffer *buffer;
    };
}