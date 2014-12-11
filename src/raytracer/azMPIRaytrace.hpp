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
    
    static const int root = 0;
    
    class azMPIRaytrace
    {
    public:
        azMPIRaytrace() {}
        azMPIRaytrace(Scene *aScene, FrameBuffer *fbuffer, size_t width, size_t height) {
            scene = aScene;
            buffer = fbuffer;
            this->width = width;
            this->height = height;
        }
        
        // generate all ray list
        void generateEyeRay(vector<Ray> &eyerays);
        
        // check first boundbing box
        int checkNextBoundingBox(Ray &ray, int procId);
        
        /*!
         @brief Do local ray tracing, 
         @param ray     ray to trace
         @return next bounding box to check
         */
        int localRaytrace(Ray &ray);
        
        // generate shadow ray by given input ray, maxt would be used
        void generateShadowRay(Ray &ray, Ray &shadowRay);
        
        // TODO: generate gi ray
        void generateGIRay(Ray &ray, Ray &giRay);
        
        // TODO: write to framebuffer
        void updateFrameBuffer(Ray &ray);
        

    private:
        
        // retrieve the closest hit record
        HitRecord getClosestHit(Ray &r, real_t t0, real_t t1, bool *isHit, SceneLayer mask);
        
        Scene *scene;
        FrameBuffer *buffer;
        
        // the dimensions of the image to trace
        size_t width, height;
    };
}