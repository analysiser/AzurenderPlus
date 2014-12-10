#include "azMPIRaytrace.hpp"
namespace _462{
    
    void azMPIRaytrace::generateEyeRay(vector<Ray> &eyerays)
    {
//        int wstep = width / scene->node_size;
//        int hstep = height;
//        real_t dx = real_t(1)/width;
//        real_t dy = real_t(1)/height;
//        
//        // node ray list to send out rays
//        RayBucket currentNodeRayList(procs);
//        
//        // Generate all eye rays in screen region, bin them
//        for (int y = 0; y < hstep; y++) {
//            for (int x = wstep * procId; x < wstep * (procId + 1); x++) {
//                
//                // pick a point within the pixel boundaries to fire our
//                // ray through.
//                real_t i = real_t(2)*(real_t(x)+random())*dx - real_t(1);
//                real_t j = real_t(2)*(real_t(y)+random())*dy - real_t(1);
//                
//                Ray r = Ray(scene->camera.get_position(), Ray::get_pixel_dir(i, j));
//                for (int node_id = 0; node_id < procs; node_id++) {
//                    BndBox nodeBBox = scene->nodeBndBox[node_id];
//                    if (nodeBBox.intersect(r, EPSILON, TMAX)) {
//                        // push ray into node's ray list
//                        r.x = x;
//                        r.y = y;
//                        r.color = Color3::Black();
//                        r.depth = 2;
//                        currentNodeRayList.push_back(node_id, r);
//                    }
//                }
//            }
//        }
    }
    
}