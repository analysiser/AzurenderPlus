#ifndef _462_SCENE_RAY_HPP_
#define _462_SCENE_RAY_HPP_

#include "math/vector.hpp"
#include "math/quaternion.hpp"
#include "math/matrix.hpp"
#include "math/camera.hpp"
#include "scene/material.hpp"
#include "scene/mesh.hpp"
#include "raytracer/Photon.hpp"
#include <string>
#include <vector>


namespace _462 {
    
    using namespace std;
    
    class Ray
    {
        
    public:
        Vector3 e;
        Vector3 d;
        Ray();
        Ray(Vector3 e, Vector3 d);
        
        // Photon carried by ray
        Photon photon;
        
        static Vector3 get_pixel_dir(real_t x, real_t y);
        static void init(const Camera& camera);
    };
    
    class azRayPacket
    {
    public:
        
        azRayPacket(const vector<Ray> list) { rayList = list; }
        azRayPacket(unsigned int size) { rayList = vector<Ray>(size); }
        
        void add(const Ray r)
        {
            rayList.push_back(r);
        }
        
    private:
        vector<Ray> rayList;
    };
    
}
#endif
