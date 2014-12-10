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
        azMPIRaytrace(Scene *aScene) { scene = aScene; }
        
        // 
        
        
    private:
        Scene *scene;
    };
}