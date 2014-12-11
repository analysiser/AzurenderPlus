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
    
    
    // check first boundbing box
    int azMPIRaytrace::checkNextBoundingBox(Ray &ray, int procId)
    {
        int procs = scene->node_size;
        assert(procId < procs);
        if (procId == procs - 1)    return root;
        
        for (int p = procId + 1; p < procs; p++) {
            if (scene->nodeBndBox[p].intersect(ray, EPSILON, TMAX)) {
                return p;
            }
        }
        return root;
    }
    
    // local ray trace
    int azMPIRaytrace::localRaytrace(Ray &ray)
    {
        int procId = scene->node_rank;
        int procs = scene->node_size;
        
        // if the ray sent here is eye ray
        if (ray.type == ERayType_Eye) {
            bool isHit = false;
            HitRecord record = getClosestHit(ray, EPSILON, INFINITY, &isHit, Layer_All);
            
            // if this ray hits anything
            if (isHit)
            {
                // update longest distance
                ray.maxt = record.t;
                
                // shade
                if (record.diffuse != Color3::Black() && record.refractive_index == 0)
                {
                    Color3 color = Color3::Black();
                    // for each light
                    
                    Light *aLight = scene->get_lights()[0];
                    
                    // shade the hit point color direclty
                    Vector3 samplePoint;
                    float tlight;
                    color += record.diffuse * aLight->SampleLight(record.position,
                                                                  record.normal,
                                                                  EPSILON,
                                                                  TMAX,
                                                                  &samplePoint,
                                                                  &tlight);
                    ray.color = color;
                    ray.time = tlight;
                }
            }
            
            // TODO:
            // find which of the next node this ray should be sent to
            return checkNextBoundingBox(ray, procId);
        }
        else if (ray.type == ERayType_Shadow)
        {
            bool isHit = false;
            getClosestHit(ray, EPSILON, ray.time, &isHit, Layer_All);
            
            if (isHit)
            {
                ray.isHit = true;
                return root;
            }
            else
            {
                return checkNextBoundingBox(ray, procId);
            }
        }
        else if (ray.type == ERayType_GI)
        {
            // TODO: GI RAYS
        }
        
        // TODO: necessary?
        return checkNextBoundingBox(ray, procId);
    }
    
    // generate shadow ray by given input ray, maxt would be used
    void azMPIRaytrace::generateShadowRay(Ray &ray, Ray &shadowRay)
    {
        // input ray's closest hit point
        Vector3 e = ray.e + ray.d * ray.maxt;
        
        // TODO: multiple light sources
        Light *aLight = scene->get_lights()[0];
        Vector3 lp = aLight->SamplePointOnLight();
        Vector3 d = normalize(lp - e);
        
        shadowRay = Ray(e, d);
        shadowRay.x = ray.x;
        shadowRay.y = ray.y;
        shadowRay.color = ray.color;
        shadowRay.mint = EPSILON;
        shadowRay.time = 0;
        shadowRay.maxt = ray.time;
        shadowRay.type = ERayType_Shadow;
        shadowRay.isHit = false;
    }
    
    void azMPIRaytrace::generateGIRay(Ray &ray, Ray &giRay)
    {
        
    }
    
    void azMPIRaytrace::updateFrameBuffer(Ray &ray)
    {
        
    }
    
    
    HitRecord azMPIRaytrace::getClosestHit(Ray r, real_t t0, real_t t1, bool *isHit, SceneLayer mask)
    {
        HitRecord closestHitRecord;
        HitRecord tmp;
        real_t t = t1;
        
        Geometry* const* geometries = scene->get_geometries();
        *isHit = false;
        for (size_t i = 0; i < scene->num_geometries(); i++)
        {
            // added layer mask for ignoring layers
            if (geometries[i]->layer ^ mask) {
                
                if (geometries[i]->hit(r, t0, t1, tmp))
                {
                    if (!*isHit) {
                        *isHit = true;
                        t = tmp.t;
                        closestHitRecord = tmp;
                        
                    }
                    else {
                        if (tmp.t < t) {
                            t = tmp.t;
                            closestHitRecord = tmp;
                            
                        }
                    }
                }
            }
            
            
        }
        
        closestHitRecord.t = t;
        closestHitRecord.isHit = *isHit;
        return closestHitRecord;
    }
    
}