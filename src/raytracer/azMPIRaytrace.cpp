#include "azMPIRaytrace.hpp"
namespace _462{
    
    void azMPIRaytrace::generateEyeRay(vector<Ray> &eyerays)
    {
        real_t dx = real_t(1)/width;
        real_t dy = real_t(1)/height;
        
        // generate all eye rays
        for (size_t y = 0; y < height; y++)
        {
            for (size_t x = 0; x < width; x++)
            {
                // pick a point within the pixel boundaries to fire our
                // ray through.
                real_t i = real_t(2)*(real_t(x)+random())*dx - real_t(1);
                real_t j = real_t(2)*(real_t(y)+random())*dy - real_t(1);
                
                Ray r = Ray(scene->camera.get_position(), Ray::get_pixel_dir(i, j));
                r.color = Color3::Black();
                r.type = ERayType_Eye;
                r.depth = 2;
                
                eyerays.push_back(r);
            }
        }
    }
    
}