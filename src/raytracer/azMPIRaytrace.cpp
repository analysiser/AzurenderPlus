#include "azMPIRaytrace.hpp"
namespace _462{
    
//    void azMPIRaytrace::generateEyeRay(vector<Ray> &eyerays)
//    {
//        printf("generateEyeRay\n");
//        real_t dx = real_t(1)/width;
//        real_t dy = real_t(1)/height;
//        
//        // generate all eye rays
//        for (size_t y = 0; y < height; y++)
//        {
//            for (size_t x = 0; x < width; x++)
//            {
//                // pick a point within the pixel boundaries to fire our
//                // ray through.
//                real_t i = real_t(2)*(real_t(x)+random())*dx - real_t(1);
//                real_t j = real_t(2)*(real_t(y)+random())*dy - real_t(1);
//                
//                Ray r = Ray(scene->camera.get_position(), Ray::get_pixel_dir(i, j));
//                r.color = Color3::Black();
//                r.type = ERayType_Eye;
//                r.depth = 2;
//                r.isHit = false;
//                
//                std::cout<<r.e<<" "<<r.d<<std::endl;
//                
//                int procs = scene->node_size;
////                printf("procs = %d\n", procs);
//                for (int node_id = 0; node_id < procs; node_id++) {
//                    BndBox nodeBBox = scene->nodeBndBox[node_id];
//                    if (nodeBBox.intersect(r, EPSILON, TMAX)) {
//                        // push ray into node's ray list
//                        r.x = x;
//                        r.y = y;
//                        r.color = Color3::Black();
//                        r.depth = 2;
//                        printf("HIT!!!!\n");
////                        currentNodeRayList.push_back(node_id, r);
//                    }
//                }
//                
//                eyerays.push_back(r);
//            }
//        }
//        printf("~generateEyeRay\n");
//    }
    
    
    // check first boundbing box
    int azMPIRaytrace::checkNextBoundingBox(Ray &ray, int procId)
    {
        int procs = scene->node_size;
        assert(procId < procs);
        
        if (procId == procs - 1)    return root;
        
        for (int p = procId + 1; p < procs; p++) {
            BndBox nodeBBox = scene->nodeBndBox[p];
            if (nodeBBox.intersect(ray, EPSILON, TMAX)) {
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
            HitRecord record = getClosestHit(ray, EPSILON, ray.maxt, &isHit, Layer_All);
            
            // if this ray hits anything
            if (isHit)
            {
                // update longest distance
                ray.maxt = record.t;
                ray.hit = record.position;
                ray.hitNormal = record.normal;
                
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
                    ray.color = color * INV_PI;
                    ray.time = tlight;
                    ray.isHit = true;
                }
            }
            
            // TODO:
            // find which of the next node this ray should be sent to
            return checkNextBoundingBox(ray, procId);
        }
        else if (ray.type == ERayType_Shadow || ray.type == ERayType_GIShadow)
        {
            bool isHit = false;
            HitRecord shadowRecord = getClosestHit(ray, EPSILON, ray.maxt, &isHit, Layer_All);
            
            if (isHit)
            {
                ray.color = Color3::Black(); // TODO: ambient
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
            bool isHit = false;
            HitRecord record = getClosestHit(ray, EPSILON, ray.maxt, &isHit, Layer_All);
            
            if (isHit)
            {
                // update longest distance
                ray.maxt = record.t;
                ray.hit = record.position;
                ray.hitNormal = record.normal;
                
                // shade gi hit point
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
                    ray.isHit = true;
                }
            }
            
            return checkNextBoundingBox(ray, procId);
        }
        else
        {
            printf("Bad Type!\n");
            exit(-1);
        }
        
        // TODO: necessary?
        return checkNextBoundingBox(ray, procId);
    }
    
    // generate shadow ray by given input ray, maxt would be used
    void azMPIRaytrace::generateShadowRay(Ray &ray, Ray &shadowRay)
    {
        // input ray's closest hit point
        Vector3 e = ray.hit;
        
        // TODO: multiple light sources
        Light *aLight = scene->get_lights()[0];
//        Vector3 lp = aLight->SamplePointOnLight();
//        Vector3 d = normalize(lp - e);
        Vector3 d = normalize(aLight->getPointToLightDirection(e, aLight->position));
        
        shadowRay = Ray(e, d);
        shadowRay.x = ray.x;
        shadowRay.y = ray.y;
        shadowRay.color = ray.color;
        shadowRay.mint = EPSILON;
        shadowRay.time = 0;
        shadowRay.maxt = ray.time;
        if (ray.type == ERayType_Eye)
            shadowRay.type = ERayType_Shadow;
        else if (ray.type == ERayType_GI)
            shadowRay.type = ERayType_GIShadow;
        else {
            printf("No such shadow ray type for type: %d\n", ray.type);
        }
        shadowRay.isHit = false;
    }
    
    void azMPIRaytrace::generateGIRay(Ray &ray, Ray &giRay)
    {
        if (ray.depth > 0) {
            Vector3 dir = uniformSampleHemisphere(ray.hitNormal);
            giRay = Ray(ray.hit + dir * EPSILON, dir);
            giRay.color = ray.color * INV_PI;
            giRay.x = ray.x;
            giRay.y = ray.y;
            giRay.mint = EPSILON;
            giRay.maxt = TMAX;
            giRay.time = 0;
            giRay.type = ERayType_GI;
            giRay.isHit = false;
            giRay.depth = ray.depth - 1;
        }
        
    }
    
    void azMPIRaytrace::updateFrameBuffer(Ray &ray)
    {
        if (ray.type == ERayType_Eye) {
            int x = ray.x;
            int y = ray.y;
            Color3 color = ray.color;
            color.to_array(&buffer->cbuffer[4 * (y * width + x)]);
        }
        else if (ray.type == ERayType_Shadow) {
            Color3 color = ray.color;
            int x = ray.x;
            int y = ray.y;
            color.to_array(&buffer->cbuffer[4 * (y * width + x)]);
        }
        else if (ray.type == ERayType_GIShadow) {
            int x = ray.x;
            int y = ray.y;
            Color3 color = Color3(&buffer->cbuffer[4 * (y * width + x)]);
            if (color != Color3::Black()) {
                 color = color + color * ray.color * INV_PI;
            }
            else
            {
                color = 0.5F * color + 0.5f * ray.color * INV_PI;
            }
           
            color.to_array(&buffer->cbuffer[4 * (y * width + x)]);
        }
        else {
            printf("No such case yet");
            exit(-1);
        }
    }
    
    
    HitRecord azMPIRaytrace::getClosestHit(Ray &r, real_t t0, real_t t1, bool *isHit, SceneLayer mask)
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