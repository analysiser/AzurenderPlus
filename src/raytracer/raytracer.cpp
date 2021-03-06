/**
 * @file raytacer.cpp
 * @brief Raytracer class
 *
 */

#include "raytracer.hpp"

#include "math/math.hpp"

#include "raytracer/azReflection.hpp"
#include "raytracer/constants.h"

#include "scene/scene.hpp"
#include "scene/model.hpp"
#include "scene/triangle.hpp"
#include "scene/sphere.hpp"

#include "utils/Parallel.h"
#include <mpi.h>

#include <SDL_timer.h>
#include <iostream>

#include "ray_list.hpp"

#define RAYTRACE_DEPTH          5
#define PHOTON_TRACE_DEPTH      5


#define EPSILON                     1e-12
#define TMAX                        400

#define PROB_DABSORB                0.5F
#define INDIRECT_PHOTON_NEEDED      500000      // 200000   // 500000
#define CAUSTICS_PHOTON_NEEDED      200000      // 50000    // 200000

#define NUM_SAMPLE_PER_LIGHT        1           // if I do so many times of raytracing, i dont need high number of samples

// Gaussian filter constants
#define E                               (2.718)
#define ALPHA                           (0.918)
#define BETA                            (1.953)
#define ONE_MINUS_E_TO_MINUS_BETA       (0.858)

#define TOTAL_ITERATION                 100  // 300
#define SMALL_NODE_GRANULARITY          128

#define PHOTON_QUERY_RADIUS             (0.000375)     // 0.000272
#define TWO_MUL_RADIUS                  (0.00075)
#define FOUR_BY_THREE                   (1.333333)
#define SPHERE_VOLUME                   (2.21e-10)

// Cone filter constants    jensen P67, k >= 1 is a filter constant characterizing the filter
#define CONE_K                          (1)

#define ENABLE_PATH_TRACING_GI          false
#define PT_GI_SAMPLE                    (1)

#define ENABLE_PHOTON_MAPPING           false
#define C_PHOTON_MODE                   1
#define SIMPLE_SMALL_NODE               1

#define ENABLE_DOF                      false
#define DOF_T                           (9.2f)
#define DOF_R                           (0.6f)
#define DOF_SAMPLE                      5

// dof on focus is 9.2f
// r = 0.6

namespace _462 {

    // max number of threads OpenMP can use. Change this if you like.
#define MAX_THREADS 8

    float dof_t = DOF_T;

    static const unsigned STEP_SIZE = 16;

    Raytracer::Raytracer()
    : scene(0), width(0), height(0) { }

    // random real_t in [0, 1)
    static inline real_t random()
    {
        return real_t(rand())/RAND_MAX;
    }

    Raytracer::~Raytracer() { }

    /**
     * Initializes the raytracer for the given scene. Overrides any previous
     * initializations. May be invoked before a previous raytrace completes.
     * @param scene The scene to raytrace.
     * @param width The width of the image being raytraced.
     * @param height The height of the image being raytraced.
     * @return true on success, false on error. The raytrace will abort if
     *  false is returned.
     */
    bool Raytracer::initialize(Scene* scene, size_t num_samples,
                               size_t width, size_t height)
    {
        /*
         * omp_set_num_threads sets the maximum number of threads OpenMP will
         * use at once.
         */
#ifndef __APPLE__
#ifdef OPENMP
        omp_set_num_threads(MAX_THREADS);
#endif
#endif
        this->scene = scene;
        this->num_samples = num_samples;
        this->width = width;
        this->height = height;

        current_row = 0;
        num_iteration = 1;  //

        Ray::init(scene->camera);
        scene->initialize();

        //----------------------------------------
        // initialize direction conversion tables, from Jensen's implementation
        //----------------------------------------
        for (int i = 0; i < 256; i++)
        {
            double angle = double(i) * (1.0/256.0) * M_PI;
            costheta[i] = cos( angle );
            sintheta[i] = sin( angle );
            cosphi[i]   = cos( 2.0 * angle );
            sinphi[i]   = sin( 2.0 * angle );
        }

        // Construction of BVH tree and bounding volumes
        int start_time = SDL_GetTicks();

        // Initialization of bounding boxes
        for (size_t i = 0; i < scene->num_geometries(); i++) {
            scene->get_geometries()[i]->createBoundingBox();
        }

        // send node bounding boxes to all other nodes
        mpiStageNodeBoundingBox(scene->node_size, scene->node_rank);

        // initialize debug variables
        radius_clear = 0;
        radius_shadow = 0;
        clear_count = 0;
        shadow_count = 0;
        acc_pass_spent = 0;
        acc_kdtree_cons = 0;


        int end_time = SDL_GetTicks();
        int time_diff = end_time - start_time;
        printf("construct BVH time: %d ms\n", time_diff);


        raytraceColorBuffer = new Color3[width * height];
        for (size_t i = 0; i < width * height; i++) {
            raytraceColorBuffer[i] = Color3::Black();
        }



//        cout<<sizeof(Intersection)<<endl;


        // test:
        printf("c photon size = %ld, photon size = %ld, Vector3 size = %ld, Color size = %ld, TP size = %ld\n", sizeof(cPhoton),sizeof(Photon), sizeof(Vector3), sizeof(Color3), sizeof(unsigned char));
        // test end

#if ENABLE_PHOTON_MAPPING
            parallelPhotonScatter(scene);
    #if C_PHOTON_MODE
            cPhotonKDTreeConstruction();
    #else
            kdtreeConstruction();
    #endif

#endif
        return true;
    }

    // scatter photons
    void Raytracer::photonScatter(const Scene* scene)
    {
        photon_indirect_list.clear();
        photon_caustic_list.clear();

        // assuming there is only one light source
//        SphereLight l = scene->get_lights()[0];

        unsigned int start = SDL_GetTicks();

        // scatter indirect
        while ((photon_indirect_list.size() < INDIRECT_PHOTON_NEEDED))
        {
            //            Vector3 p = samplePointOnUnitSphere();
            //            Ray photonRay = Ray(p + l.position, p);
            for (size_t i = 0; i < scene->num_lights(); i++) {
                Light *aLight = scene->get_lights()[i];
                Ray photonRay = aLight->getRandomRayFromLight();
                photonRay.photon.mask = 0;
                photonRay.photon.setColor(aLight->color);
                photonTrace(photonRay, EPSILON, TMAX, PHOTON_TRACE_DEPTH);
            }
        }


        unsigned int end = SDL_GetTicks();
        printf("Finished Scattering Indirect : %d ms\n", end - start);

        start = SDL_GetTicks();

        // scatter cautics
        while ((photon_caustic_list.size() < CAUSTICS_PHOTON_NEEDED))
        {
            for (size_t i = 0; i < scene->num_lights(); i++) {
                Light *aLight = scene->get_lights()[i];
                Ray photonRay = aLight->getRandomRayFromLight();//getPhotonEmissionRayFromLight(aLight);
                photonRay.photon.mask = 0;
                photonRay.photon.setColor(aLight->color);
                photonTrace(photonRay, EPSILON, TMAX, PHOTON_TRACE_DEPTH);

            }
            //            Vector3 p = samplePointOnUnitSphere();
            //            Ray photonRay = Ray(p + l.position, p);
//            Ray photonRay = getPhotonEmissionRayFromLight(scene->get_lights()[0]);
//            // Make color only the portion of light
//            photonRay.photon.mask = 0;
//            photonRay.photon.setColor(l->color);// * (real_t(1)/(real_t)(CAUSTICS_PHOTON_NEEDED + INDIRECT_PHOTON_NEEDED));
//            photonTrace(photonRay, EPSILON, TMAX, PHOTON_TRACE_DEPTH);
        }


        end = SDL_GetTicks();
        printf("Finished Scattering Caustics : %d ms\n", end - start);

        printf("Finished Scattering, indirect num = %ld, caustic num = %ld\n", photon_indirect_list.size(), photon_caustic_list.size());

        std::vector<Photon> tmp_indirect(photon_indirect_list.size() + 1);
        std::vector<Photon> tmp_caustic(photon_caustic_list.size() + 1);

        start = SDL_GetTicks();
        balance(1, tmp_indirect, photon_indirect_list);
        end = SDL_GetTicks();
        printf("Finished Balancing Indirect : %d ms\n", end - start);

        start = SDL_GetTicks();
        balance(1, tmp_caustic, photon_caustic_list);
        end = SDL_GetTicks();
        printf("Finished Balancing Caustics : %d ms\n", end - start);


        kdtree_photon_indirect_list = tmp_indirect;
        kdtree_photon_caustic_list = tmp_caustic;

        printf("Finished Balancing!\n");
    }

    void Raytracer::kdtreeConstruction()
    {
        unsigned int start = 0, end = 0, acc = 0;

        // balancing kdtree and copy back
        std::vector<Photon> tmp_indirect(photon_indirect_list.size() + 1);
        std::vector<Photon> tmp_caustic(photon_caustic_list.size() + 1);

        start = SDL_GetTicks();
        balance(1, tmp_indirect, photon_indirect_list);
        end = SDL_GetTicks();
        acc += end - start;
        printf("Finished Construct Indirect KD-Tree : %d ms\n", end - start);

        start = SDL_GetTicks();
        balance(1, tmp_caustic, photon_caustic_list);
        end = SDL_GetTicks();
        acc += end - start;
        printf("Finished Construct Caustics KD-Tree : %d ms\n", end - start);

        kdtree_photon_indirect_list = tmp_indirect;
        kdtree_photon_caustic_list = tmp_caustic;

        printf("Finished KD-Tree Construction! Total time = %d ms\n", acc);
    }

    // c style photon kdtree construction
    void Raytracer::cPhotonKDTreeConstruction()
    {
        unsigned int start = 0, end = 0;

        start = SDL_GetTicks();
        // clear old c style kdtree
//        kdtree_cphoton_indirect.clear();
//        kdtree_cphoton_caustics.clear();

        cphoton_indirect_data.clear();
        cphoton_caustics_data.clear();

        std::vector<cPhoton> c_indirect(photon_indirect_list.size());
        std::vector<cPhoton> c_caustics(photon_caustic_list.size());

        // copy from normal photon to c photon
        for (size_t i = 0; i < photon_indirect_list.size(); i++)
        {
            photon_indirect_list[i].position.to_array(c_indirect[i].position);
            c_indirect[i].index = i;
            c_indirect[i].splitAxis = -1;
        }

        for (size_t i = 0; i < photon_caustic_list.size(); i++)
        {
            photon_caustic_list[i].position.to_array(c_caustics[i].position);
            c_caustics[i].index = i;
            c_caustics[i].splitAxis = -1;
        }


        cphoton_indirect_data = c_indirect;
        cphoton_caustics_data = c_caustics;

        vvh_indirect_root = new KDNode;
        vvh_caustics_root = new KDNode;

        // construct vvh based kd-tree
        vvhKDTreeConstruction(cphoton_indirect_data, vvh_indirect_root);
        vvhKDTreeConstruction(cphoton_caustics_data, vvh_caustics_root);

        // construct ispc friendly cphoton data
        ispc_cphoton_indirect_data.size = cphoton_indirect_data.size();
        ispc_cphoton_indirect_data.posx = new float[cphoton_indirect_data.size()];
        ispc_cphoton_indirect_data.posy = new float[cphoton_indirect_data.size()];
        ispc_cphoton_indirect_data.posz = new float[cphoton_indirect_data.size()];

        ispc_cphoton_indirect_data.bitmap = new int8_t[cphoton_indirect_data.size()];

        ispc_cphoton_caustics_data.size = cphoton_caustics_data.size();
        ispc_cphoton_caustics_data.posx = new float[cphoton_caustics_data.size()];
        ispc_cphoton_caustics_data.posy = new float[cphoton_caustics_data.size()];
        ispc_cphoton_caustics_data.posz = new float[cphoton_caustics_data.size()];
        ispc_cphoton_caustics_data.bitmap = new int8_t[cphoton_caustics_data.size()];


        for (size_t i = 0; i < cphoton_indirect_data.size(); i++)
        {
            ispc_cphoton_indirect_data.posx[i] = cphoton_indirect_data[i].position[0];
            ispc_cphoton_indirect_data.posy[i] = cphoton_indirect_data[i].position[1];
            ispc_cphoton_indirect_data.posz[i] = cphoton_indirect_data[i].position[2];
            ispc_cphoton_indirect_data.bitmap[i] = 0;
        }

        for (size_t i = 0; i < cphoton_caustics_data.size(); i++)
        {
            ispc_cphoton_caustics_data.posx[i] = cphoton_caustics_data[i].position[0];
            ispc_cphoton_caustics_data.posy[i] = cphoton_caustics_data[i].position[1];
            ispc_cphoton_caustics_data.posz[i] = cphoton_caustics_data[i].position[2];
            ispc_cphoton_caustics_data.bitmap[i] = 0;
        }

        end = SDL_GetTicks();
        acc_kdtree_cons += end - start;
        printf("c photon KDTree Construction Time = %d ms\n", end - start);
    }

    /**
     * Generate a ray from given (x, y) coordinates in screen space.
     * @param scene The scene to trace.
     * @param x The x-coordinate of the pixel to trace.
     * @param y The y-coordinate of the pixel to trace.
     * @param dx delta x
     * @param dy delta y
     */
    Ray Raytracer::generateEyeRay(const Vector3 cameraPosition, size_t x, size_t y, float dx, float dy)
    {
        // pick a point within the pixel boundaries to fire our
        // ray through.
        float i = float(2)*(float(x)+random())*dx - float(1);
        float j = float(2)*(float(y)+random())*dy - float(1);

        return Ray(cameraPosition, Ray::get_pixel_dir(i, j));
    }

    /**
     * Performs a raytrace on the given pixel on the current scene.
     * The pixel is relative to the bottom-left corner of the image.
     * @param scene The scene to trace.
     * @param x The x-coordinate of the pixel to trace.
     * @param y The y-coordinate of the pixel to trace.
     * @param width The width of the screen in pixels.
     * @param height The height of the screen in pixels.
     * @return The color of that pixel in the final image.
     */
    Color3 Raytracer::trace_pixel(const Scene* scene,
                                  size_t x,
                                  size_t y,
                                  size_t width,
                                  size_t height)
    {
        assert(x < width);
        assert(y < height);

        real_t dx = real_t(1)/width;
        real_t dy = real_t(1)/height;

        Color3 res = Color3::Black();
        unsigned int iter;
        for (iter = 0; iter < num_samples; iter++)
        {
            // pick a point within the pixel boundaries to fire our
            // ray through.
            real_t i = real_t(2)*(real_t(x)+random())*dx - real_t(1);
            real_t j = real_t(2)*(real_t(y)+random())*dy - real_t(1);

#if ENABLE_DOF
            Ray r = Ray(scene->camera.get_position(), Ray::get_pixel_dir(i, j));
            res += trace(r, EPSILON, TMAX, RAYTRACE_DEPTH);

            for (int i = 0; i < DOF_SAMPLE - 1; i++) {
                double random_r = DOF_R * random_uniform();
                double random_theta = 2 * PI * random_uniform();
                double random_x = random_r * cos(random_theta);
                double random_y = random_r * sin(random_theta);
                Vector3 focus   = r.e + r.d * dof_t;
                Vector3 right   = cross(scene->camera.get_direction(), scene->camera.get_up());
                Vector3 cam     = scene->camera.get_position() + DOF_R * random_x * right + DOF_R * random_y * scene->camera.get_up();
                Ray sample_ray = Ray(cam, normalize(focus - cam));
                res += trace(sample_ray, EPSILON, TMAX, RAYTRACE_DEPTH);
            }

            res *= 1.0/(float)DOF_SAMPLE;

#else
            // Entrance
            Ray r = Ray(scene->camera.get_position(), Ray::get_pixel_dir(i, j));
            res += trace(r, EPSILON, TMAX, RAYTRACE_DEPTH);
#endif

        }
        return res*(float(1)/float(num_samples));
    }

    bool Raytracer::PacketizedRayTrace(unsigned char* buffer)
    {
        pass_start = SDL_GetTicks();

        float dx = float(1)/width;
        float dy = float(1)/height;

        // TODO: parallel ray generation
        // Ray generation
        while (current_row < height) {

            for (current_col = 0; current_col < width; current_col += STEP_SIZE) {
                // Start making packets
                azPacket<Ray> rayPacket;
                // For each STEP rows
                for (size_t i = 0; i < STEP_SIZE; i++) {
                    size_t y = current_row + i;
                    y = std::min(y, height - 1);
                    // For each STEP cols
                    for (size_t j = 0; j < STEP_SIZE; j++) {
                        size_t x = current_col + j;
                        x = std::min(x, width - 1);

                        // TODO: generate ray packets
                        Ray eyeRay = generateEyeRay(scene->camera.get_position(), x, y, dx, dy);
                        rayPacket.add(eyeRay);
                    }
                }

                // TODO: scheduler
                // Finished a STEP^2 rays packet
                rayPacket.setReady();
                azPacket<HitRecord> hitInfoPacket(STEP_SIZE * STEP_SIZE);
                PacketizedRayIntersection(rayPacket, hitInfoPacket, EPSILON, INFINITY);

                // TODO: shadow ray
                // TODO: deferred shading
                for (size_t i = 0; i < hitInfoPacket.size(); i++) {
                    int index = i;
                    auto &hitInfo = hitInfoPacket[i];
                    auto &ray = rayPacket[i];

                    if (hitInfo.isHit)
                    {
//                        printf("%d\n", index);
                        size_t x = current_col + index%STEP_SIZE;
                        size_t y = current_row + index/STEP_SIZE;
                        x = std::min(x, width - 1);
                        y = std::min(y, height - 1);
//                        std::cout<<x<<" "<<y<<std::endl;

                        Color3 color = shade(ray, hitInfo, EPSILON, INFINITY, 1);
                        raytraceColorBuffer[(y * width + x)] += color;
                        Color3 progressiveColor = raytraceColorBuffer[(y * width + x)] * ((1.0)/(num_iteration));
                        progressiveColor = clamp(progressiveColor, 0.0, 1.0);

                        progressiveColor.to_array(&buffer[4 * (y * width + x)]);
                    }


                }
            }
            current_row += STEP_SIZE;
        }

        // Reset raytracer for another render pass
        if (num_iteration < TOTAL_ITERATION)
        {
            pass_end = SDL_GetTicks();
            acc_pass_spent += pass_end - pass_start;
            printf("Done One Pass! Iteration = %d, Pass spent = %dms\n", num_iteration, (pass_end - pass_start));
            current_row = 0;
            current_col = 0;
            num_iteration++;
            return false;
        }

        return true;
    }

    bool Raytracer::mpiTrace(FrameBuffer &buffer, unsigned char *dibuffer, unsigned char *gibuffer, real_t* /* max_time */)
    {
        std::vector<Ray> eyerays;
        std::vector<Ray> shadowrays;
        std::vector<Ray> girays;
        
        double start, end;
        // generate and redistribute eye rays through open mpi to different nodes
        start = MPI_Wtime();
        mpiStageDistributeEyeRays(scene->node_size, scene->node_rank, &eyerays);
        end = MPI_Wtime();

        printf("[thread %d] Distribute eye ray state took %f sec\n", scene->node_rank, end - start);

        // Take in distributed eye rays, do local ray tracing, distribute shadow rays to corresponding nodes
        start = MPI_Wtime();
        mpiStageLocalRayTrace(scene->node_size, scene->node_rank, eyerays, &shadowrays, &girays, true);
        end = MPI_Wtime();

        printf("[thread %d] LocalRayTracing state took %f sec\n", scene->node_rank, end - start);

        start = MPI_Wtime();
        mpiStageShadowRayTracing(scene->node_size, scene->node_rank, buffer, shadowrays);
        end = MPI_Wtime();

        printf("[thread %d] Shadow state took %f sec\n", scene->node_rank, end - start);
        
        // merge direct illumination buffer
        start = MPI_Wtime();
        mpiMergeFrameBufferToBuffer(scene->node_size, scene->node_rank, buffer, dibuffer);
        end = MPI_Wtime();
        printf("[thread %d] Merge Framebuffer took %f sec\n", scene->node_rank, end - start);
        
        buffer.cleanbuffer(width, height);

        
        while (!mpiShouldStop(scene->node_size, scene->node_rank, shadowrays.size(), girays.size()))
        {
            
//            eyerays.clear();
//            eyerays = girays;
//            girays.clear();
//            shadowrays.clear();
            eyerays = girays;
            
            mpiStageLocalRayTrace(scene->node_size, scene->node_rank, eyerays, &shadowrays, &girays, false);
            
            
            mpiStageShadowRayTracing(scene->node_size, scene->node_rank, buffer, shadowrays);
            
            // TODO: merge global illumination buffer
            mpiMergeFrameBufferToBuffer(scene->node_size, scene->node_rank, buffer, gibuffer);
            
            buffer.cleanbuffer(width, height);
            
            mergebuffers(dibuffer, gibuffer, width, height);
            
        }
        

        return true;
    }

    /**
     * Raytraces some portion of the scene. Should raytrace for about
     * max_time duration and then return, even if the raytrace is not copmlete.
     * The results should be placed in the given buffer.
     * @param buffer The buffer into which to place the color data. It is
     *  32-bit RGBA (4 bytes per pixel), in row-major order.
     * @param max_time, If non-null, the maximum suggested time this
     *  function raytrace before returning, in seconds. If null, the raytrace
     *  should run to completion.
     * @return true if the raytrace is complete, false if there is more
     *  work to be done.
     */
    bool Raytracer::raytrace(unsigned char* buffer, real_t* max_time)
    {
        pass_start = SDL_GetTicks();
        // the time in milliseconds that we should stop
        unsigned int end_time = 0;
        bool is_done;

        if (max_time)
        {
            // convert duration to milliseconds
            unsigned int duration = (unsigned int) (*max_time * 1000);
            end_time = SDL_GetTicks() + duration;
        }

        // until time is up, run the raytrace. we render an entire group of
        // rows at once for simplicity and efficiency.
        for (; !max_time || end_time > SDL_GetTicks(); current_row += STEP_SIZE)
        {
//            if (end_time > SDL_GetTicks())
//            {
//                std::cout<<"FINISH"<<std::endl;
//            }
            // we're done if we finish the last row
            is_done = current_row >= height;
            // break if we finish
            if (is_done) break;

            int loop_upper = std::min(current_row + STEP_SIZE, height);
            for (int c_row = current_row; c_row < loop_upper; c_row++)
            {
                for (size_t x = 0; x < width; x++)
                {
                    // trace a pixel
                    // Packet entrance
                    Color3 color = trace_pixel(scene, x, c_row, width, height);

                    raytraceColorBuffer[(c_row * width + x)] += color;
                    Color3 progressiveColor = raytraceColorBuffer[(c_row * width + x)] * ((1.0)/(num_iteration));
                    progressiveColor = clamp(progressiveColor, 0.0, 1.0);

                    progressiveColor.to_array(&buffer[4 * (c_row * width + x)]);
                }
                // Make packets
            }
        }

        if (is_done)
        {
            if (num_iteration < TOTAL_ITERATION)
            {
                pass_end = SDL_GetTicks();
                acc_pass_spent += pass_end - pass_start;
                printf("Done One Pass! Iteration = %d, Pass spent = %dms\n", num_iteration, (pass_end - pass_start));

#if ENABLE_PHOTON_MAPPING
                printf("Pass photon search spent: indirect = %dms, caustics = %dms\n", acc_iphoton_search_time, acc_cphoton_search_time);
#endif
                // add postprocessing kernal to raytraceColorBuffer
                current_row = 0;
#if ENABLE_PHOTON_MAPPING
    #if C_PHOTON_MODE
                    parallelPhotonScatter(scene);

                    cPhotonKDTreeConstruction();
    #else
                    kdtreeConstruction();
    #endif
#endif

                is_done = false;
                num_iteration++;


            }
            else
            {
                pass_end = SDL_GetTicks();
                master_end = SDL_GetTicks();

                printf("Done Progressive Photon Mapping! Iteration = %d, Total Spent = %dms\n", num_iteration, master_end - master_start);
//                perPixelRender(buffer);

                // debug varibale update
                printf("average clear radius = %f, average shadow radius = %f, Average KDTree Construction %dms , Average Pass Spent = %dms\n", radius_clear/(float)clear_count, radius_shadow/(float)shadow_count, acc_kdtree_cons/num_iteration, acc_pass_spent/num_iteration);
            }

        }

        return is_done;
    }

//    void Raytracer::perPixelRender(unsigned char* buffer)
//    {
//        printf("Final Rendering\n");
//        for (size_t y = 0; y < height; y++)
//        {
//            for (size_t x = 0; x < width; x++)
//            {
//                Color3 color = raytraceColorBuffer[(y * width + x)] * (1.0/(real_t)(TOTAL_ITERATION));
//                color.to_array(&buffer[4 * (y * width + x)]);
//            }
//        }
//    }

    /**
     * Do Photon tracing, store photons according to their type into indirect map or caustics map
     * @param   ray         incoming ray
     * @param   t0          lower limit of t
     * @param   t1          upper limit of t
     * @param   depth       maximun depth
     */
    void Raytracer::photonTrace(Ray ray, real_t t0, real_t t1, int depth)
    {
        if (depth == 0) {
            return;
        }

        bool isHit = false;
        HitRecord record = getClosestHit(ray, t0, t1, &isHit, Layer_All);

        if (isHit)
        {
            // refractive
            if (record.refractive_index > 0) {

                float ni = float(1.0);
                float nt = (float)record.refractive_index;
                float cos_theta = dot(ray.d, record.normal);

                float reflectivity = azFresnel::FresnelDielectricEvaluate(cos_theta, ni, nt);
                float transmity    = 1.f - reflectivity;

                if (reflectivity > 0.f) {
                    Vector3 reflectDirection = azReflection::reflect(ray.d, record.normal);
                    reflectDirection = normalize(reflectDirection);

                    Ray reflectRay = Ray(record.position + EPSILON * reflectDirection, reflectDirection);

                    reflectRay.photon = ray.photon;
                    reflectRay.photon.setColor(reflectRay.photon.getColor() * record.specular * reflectivity);
                    //                    reflectRay.photon.color *= record.specular;
                    reflectRay.photon.mask |= 0x2;
                    photonTrace(reflectRay, t0, t1, depth - 1);

                }

                if (transmity > 0.f) {

                    Vector3 refractDirection = azReflection::refract(ray.d, record.normal, ni, nt);
                    refractDirection = normalize(refractDirection);

                    // create refractive reflection for photon
                    Ray refractRay = Ray(record.position + EPSILON * refractDirection , refractDirection);
                    refractRay.photon = ray.photon;
                    refractRay.photon.setColor(refractRay.photon.getColor() * record.specular * transmity);
                    //                    refractRay.photon.color *= record.specular;
                    refractRay.photon.mask |= 0x2;
                    photonTrace(refractRay, t0, t1, depth-1);
                }
            }
            // specular reflective
            else if (record.specular != Color3::Black())
            {
                // Pure reflective surface
                if (record.diffuse == Color3::Black()) {

                    Vector3 reflectDirection = azReflection::reflect(ray.d, record.normal);
                    reflectDirection = normalize(reflectDirection);
                    Ray reflectRay = Ray(record.position + reflectDirection * EPSILON, reflectDirection);
                    reflectRay.photon = ray.photon;
                    reflectRay.photon.setColor(reflectRay.photon.getColor() * record.specular);
//                    reflectRay.photon.color *= record.specular;
                    reflectRay.photon.mask |= 0x2;
                    photonTrace(reflectRay, t0, t1, depth - 1);

                }
                // Hit on a surface that is both reflective and diffusive
                else
                {
                    real_t prob = random();
                    // Then there is a possibility of whether reflecting or absorbing
                    if (prob < 0.5) {
                        Vector3 reflectDirection = azReflection::reflect(ray.d, record.normal);
                        Ray reflectRay = Ray(record.position + reflectDirection * EPSILON, reflectDirection);
                        reflectRay.photon = ray.photon;
                        reflectRay.photon.setColor(reflectRay.photon.getColor() * record.specular);
//                        reflectRay.photon.color *= record.specular;
                        reflectRay.photon.mask |= 0x2;
                        photonTrace(reflectRay, t0, t1, depth - 1);
                    }
                    else {
                        // absorb
                        if (photon_indirect_list.size() < INDIRECT_PHOTON_NEEDED)
                        {
                            ray.photon.position = record.position;
//                            ray.photon.direction = -ray.d;
                            setPhotonDirection(ray.photon, -ray.d);
//                            ray.photon.setDirection(-ray.d);
//                            ray.photon.color = ray.photon.color;// * record.diffuse;
//                            ray.photon.color = ray.photon.color/
//                            ray.photon.normal = record.normal;
                            photon_indirect_list.push_back(ray.photon);
                        }

                    }
                }
            }
            // diffusive
            else {
                // direct illumination, do not store
                if (ray.photon.mask == 0x0) {
                    // consider don't do direct illumination
                    // if remove this, global photons could be faster but caustics are getting far slower
                    real_t prob = random();
                    if (prob > PROB_DABSORB) {
                        Ray photonRay = Ray(record.position, uniformSampleHemisphere(record.normal));
                        photonRay.photon = ray.photon;
                        photonRay.photon.mask |= 0x1;
//                        photonRay.photon.color = ray.photon.color * record.diffuse;
                        photonRay.photon.setColor(ray.photon.getColor() * record.diffuse);
//                        photonRay.photon.setColor(ray.photon.getColor()  * (real_t(1)/real_t(1.0 - PROB_DABSORB)));

//                        ray.photon.color *= real_t(1)/real_t(1.0 - PROB_DABSORB);
//                        ray.photon.setColor(ray.photon.getColor() * (real_t(1)/real_t(1.0 - PROB_DABSORB)));

                        photonTrace(photonRay, t0, t1, depth-1);
                    }
//                    else
//                    {
//                        // absorb
//                        if (photon_indirect_list.size() < INDIRECT_PHOTON_NEEDED)
//                        {
//                            ray.photon.position = record.position;
//                            ray.photon.direction = -ray.d;
//                            ray.photon.normal = record.normal;
//                            photon_indirect_list.push_back(ray.photon);
//                        }
//                    }
                }
                // caustics
                else if (ray.photon.mask == 0x2) {
//                    printf("nice mask!\n");
                    if (photon_caustic_list.size() < CAUSTICS_PHOTON_NEEDED) {
                        ray.photon.position = record.position;
//                        ray.photon.direction = -ray.d;
                        setPhotonDirection(ray.photon, -ray.d);
//                        ray.photon.normal = (record.normal);
                        photon_caustic_list.push_back(ray.photon);

                    }
                }
                // indirect illumination
                else {
                    real_t prob = random();
                    if (prob < PROB_DABSORB) {
                        // Store photon in indirect illumination map
                        if (photon_indirect_list.size() < INDIRECT_PHOTON_NEEDED) {
                            ray.photon.position = (record.position);
//                            ray.photon.direction = (-ray.d);
                            setPhotonDirection(ray.photon, -ray.d);
//                            ray.photon.normal = (record.normal);
//                            ray.photon.color *= real_t(1)/real_t(PROB_DABSORB);
                            ray.photon.setColor(ray.photon.getColor() * (real_t(1)/real_t(PROB_DABSORB)));
                            photon_indirect_list.push_back(ray.photon);
                        }
                    }
                    else {
                        // Generate a diffusive reflect
                        Ray photonRay = Ray(record.position, uniformSampleHemisphere(record.normal));
                        photonRay.photon = ray.photon;
                        photonRay.photon.mask |= 0x1;
//                        photonRay.photon.color = (ray.photon.color * record.diffuse);
                        photonRay.photon.setColor(ray.photon.getColor() * record.diffuse);
//                        ray.photon.color *= real_t(1)/real_t(1.0 - PROB_DABSORB);
                        ray.photon.setColor(ray.photon.getColor() * (real_t(1)/real_t(1.0 - PROB_DABSORB)));
                        photonTrace(photonRay, t0, t1, depth-1);
                    }
                }
            }
        }
    }

    /**
     * @brief   Ray tracing a given ray, return the color of the hiting point on surface
     * @param   ray     Ray to trace
     * @param   t0      Lower limit of t
     * @param   t1      Upper limit of t
     * @param   depth   Recursion depth
     * @return  Color3  Shading color of hit point, if there is no hit,
     *                  then background color
     */
    Color3 Raytracer::trace(Ray ray, real_t t0, real_t t1, int depth)
    {
        bool isHit = false;
        HitRecord record = getClosestHit(ray, t0, t1, &isHit, Layer_All);
        if (isHit) {
            return shade(ray, record, t0, t1, depth);
        }
        else {
            return scene->background_color;
        }
    }

    /**
     * @brief   Shading a hit point on surface
     * @param   ray     Ray to trace
     * @param   record  Hit record
     * @param   t0      Lower limit of t
     * @param   t1      Upper limit of t
     * @param   depth   Recursion depth
     * @return  Color3  Shading color of hit point, after calculating diffusive, ambient, reflection etc.
     */
    Color3 Raytracer::shade(Ray ray, HitRecord record, real_t t0, real_t t1, int depth)
    {
        Color3 radiance = Color3::Black();

        if (depth == 0) {
            return radiance;
        }

        if (record.diffuse != Color3::Black() && record.refractive_index == 0) {

            // Trace each light source for direct illumination
            radiance += shade_direct_illumination(record, t0, t1);

#if ENABLE_PATH_TRACING_GI
            // Xiao debug, 8/11/2014, at SIGGRAPH
            // TODO: Actually apply BRDF for path tracing
            // try to simulate lambertian BRDF model for monte carlo path tracing
            Color3 indirectRadiance = Color3::Black();
            for (int i = 0; i < PT_GI_SAMPLE; i++) {
                Vector3 dir = uniformSampleHemisphere(record.normal);
                Ray secondRay = Ray(record.position + dir * EPSILON, dir);

                Color3 Li = record.diffuse * trace(secondRay, t0, t1, depth - 1);

                indirectRadiance += Li;

            }
            radiance += indirectRadiance * (1.f/float(PT_GI_SAMPLE));

#else
#endif
            // normal
#if ENABLE_PHOTON_MAPPING
                int coeef = 25;
    #if C_PHOTON_MODE
                if (CAUSTICS_PHOTON_NEEDED + INDIRECT_PHOTON_NEEDED > 0)
                {
                    Color3 photonRadiance = shade_cphotons(record, PHOTON_QUERY_RADIUS, 0.001) * (2.0/(CAUSTICS_PHOTON_NEEDED + INDIRECT_PHOTON_NEEDED)) * coeef;
                    photonRadiance = clamp(photonRadiance, 0, 1.0);
                    radiance += photonRadiance;
                    radiance = clamp(radiance, 0, 1.0);
                }
    #endif
#endif

        }

        return record.texture * radiance;
    }

    /**
     * @brief Do direct illumination for ray tracing
     */
    Color3 Raytracer::shade_direct_illumination(HitRecord &record, real_t t0, real_t t1)
    {
        Color3 res = Color3::Black();


        for (size_t i = 0; i < scene->num_lights(); i++) {

            // make random points on a light, this would make egdes of shadows smoother
            size_t sample_num_per_light = NUM_SAMPLE_PER_LIGHT;
            for (size_t j = 0; j < sample_num_per_light; j++)
            {
                Light *aLight = scene->get_lights()[i];

                Vector3 d_shadowRay_normolized = aLight->getPointToLightDirection(record.position,
                                                                                  aLight->SamplePointOnLight());

                Ray shadowRay = Ray(record.position + EPSILON * d_shadowRay_normolized, d_shadowRay_normolized);
                bool isHit = false;
                HitRecord shadowRecord = getClosestHit(shadowRay, t0, t1, &isHit, (Layer_IgnoreShadowRay));

                if (isHit) {
                    res += record.diffuse * aLight->SampleLight(record.position, record.normal, t0, std::min(shadowRecord.t, t1), nullptr, nullptr);
                }
                else {
                    res += record.diffuse * aLight->SampleLight(record.position, record.normal, t0, t1, nullptr, nullptr);
                }
            }

            res *= (1.f/(float)sample_num_per_light);

            // !!! XIAO LI DEBUG 8/20/2014  MAYBE NOT RIGHT
            // This is only for lambertian model
            // TODO: make independent material and texture class for handling shaders!
            // which involves different BxDF functions and PDF for evaluting
            // and make shading be local
            res *= INV_PI;
        }

        return res;
    }

    // Shading of caustics
    Color3 Raytracer::shade_caustics(HitRecord &record, real_t radius, size_t num_samples)
    {
        Color3 causticsColor = Color3::Black();
        std::vector<Photon> nearestCausticPhotons;

        if (record.refractive_index == 0)
        {
            // sample radius
//            real_t sampleSquaredRadiusCaustics = 0.04;   // 0.1 // 0.001 // 0.0225
//            real_t maxSquaredDistCaustics = 0.001;      // 0.001 // 0.001
            size_t maxPhotonsEstimate = num_samples;

            // gather samples
            // TODO: doing nearest neighbor search with KD Tree
            float maxSearchSquaredRadius = radius * 0.9;
            while (nearestCausticPhotons.size() == 0) {
                maxSearchSquaredRadius /= 0.9;
                locatePhotons(1, record.position, kdtree_photon_caustic_list, nearestCausticPhotons, maxSearchSquaredRadius, maxPhotonsEstimate);
            }


            // calculate radiance
            for (size_t i = 0; i < nearestCausticPhotons.size(); i++)
            {
                causticsColor +=
                record.getPhotonLambertianColor(getPhotonDirection(nearestCausticPhotons[i]), nearestCausticPhotons[i].getColor());
//                record.getPhotonLambertianColor(nearestCausticPhotons[i].direction, nearestCausticPhotons[i].getColor())
//                * getConeFilterWeight(sqrt(nearestCausticPhotons[i].squaredDistance), sqrt(sampleSquaredRadiusCaustics));
                //* getGaussianFilterWeight(nearestCausticPhotons[i].squaredDistance, maxPhotonsEstimate);
            }

            // color/= PI*r^2
//            causticsColor *= (real_t(1)/((real_t(1) - real_t(2)/(real_t(3) * CONE_K)) * (PI * maxSquaredDistCaustics))) * (1.0/(CAUSTICS_PHOTON_NEEDED));
            causticsColor = causticsColor * (real_t(1)/(PI * maxSearchSquaredRadius));// * 0.0001;// * (1.0/(CAUSTICS_PHOTON_NEEDED));
        }

        nearestCausticPhotons.clear();

        return causticsColor;

    }

    Color3 Raytracer::shade_cphotons(HitRecord &record, real_t radius, size_t num_samples)
    {
        Color3 color = Color3::Black();
        std::vector<int> nearestPhotonIndices;
        std::vector<Photon> sourcePhotons;

        if (record.refractive_index == 0)
        {
            size_t maxPhotonsEstimate = num_samples;
            float maxSearchSquaredRadius = radius;// * 0.9;

            unsigned int start, end;
            if (sourcePhotons.size() == 0)
            {
//                maxSearchSquaredRadius /= 0.9;
                start = SDL_GetTicks();
                vvhcPhotonLocate(record.position,
                                 vvh_indirect_root,
                                 ispc_cphoton_indirect_data,
                                 cphoton_indirect_data,
                                 nearestPhotonIndices,
                                 maxSearchSquaredRadius,
                                 maxPhotonsEstimate);

                // convert cphoton to normal photon
                for (size_t i = 0; i < nearestPhotonIndices.size(); i++)
                {
                    sourcePhotons.push_back(photon_indirect_list[nearestPhotonIndices[i]]);
                }
                nearestPhotonIndices.clear();
                end = SDL_GetTicks();
                acc_iphoton_search_time += end - start;

                start = SDL_GetTicks();
                vvhcPhotonLocate(record.position,
                                 vvh_caustics_root,
                                 ispc_cphoton_caustics_data,
                                 cphoton_caustics_data,
                                 nearestPhotonIndices,
                                 maxSearchSquaredRadius,
                                 maxPhotonsEstimate);

                // convert cphoton to normal photon
                for (size_t i = 0; i < nearestPhotonIndices.size(); i++)
                {
                    sourcePhotons.push_back(photon_caustic_list[nearestPhotonIndices[i]]);
                }
                nearestPhotonIndices.clear();
                end = SDL_GetTicks();
                acc_cphoton_search_time += end - start;
            }

//            printf("source photon = %ld\n",sourcePhotons.size());
            // shade
            for (size_t i = 0; i < sourcePhotons.size(); i++)
            {
//                std::cout<<sourcePhotons[i].getColor()<<std::endl;
                color +=
                record.getPhotonLambertianColor(getPhotonDirection(sourcePhotons[i]), sourcePhotons[i].getColor());
            }

            color *= (real_t(1)/(PI * maxSearchSquaredRadius));
        }

        return color;
    }

    void Raytracer::PacketizedRayIntersection(azPacket<Ray> &rayPacket, azPacket<HitRecord> &recordPacket, float t0, float t1)
    {
        Geometry* const* geometries = scene->get_geometries();
        for (size_t i = 0; i < scene->num_geometries(); i++)
        {
            // set packet is ready to do intersection tests with a new geometry
            rayPacket.setReady();
            geometries[i]->packetHit(rayPacket, recordPacket, t0, t1);
        }
    }


    /**
     * Retrieve the closest hit record
     * @param r             incoming ray
     * @param t0            lower limit of t
     * @param t1            upper limit of t
     * @param *isHit        indicate if there is a hit incident
     * @param mask          layer to ignore when doing the nearest surface test
     * @return HitRecord    the closest hit record
     */
    HitRecord Raytracer::getClosestHit(Ray r, real_t t0, real_t t1, bool *isHit, SceneLayer mask)
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

    Vector3 Raytracer::samplePointOnUnitSphere()
    {
        real_t x = _462::random_gaussian();
        real_t y = _462::random_gaussian();
        real_t z = _462::random_gaussian();

        Vector3 ran = Vector3(x, y, z);
        return normalize(ran);
    }

    Vector3 Raytracer::samplePointOnUnitSphereUniform()
    {
        real_t x = _462::random_uniform() * 2 - 1;
        real_t y = _462::random_uniform() * 2 - 1;
        real_t z = _462::random_uniform() * 2 - 1;

//        real_t x = _462::random_uniform();
//        real_t y = _462::random_uniform();
//        real_t z = _462::random_uniform();

        Vector3 ran = Vector3(x, y, z);
        return normalize(ran);
    }

    Vector3 Raytracer::uniformSampleHemisphere(const Vector3& normal)
    {
//        Vector3 newDir = samplePointOnUnitSphereUniform();
//        Vector3 newDir = samplePointOnUnitSphere();
        Vector3 newDir = uniformSampleSphere(random_uniform(), random_uniform());
        if (dot(newDir, normal) < 0.0) {
            newDir = -newDir;
        }
        return normalize(newDir);
        return newDir;
    }

    inline real_t getGaussianFilterWeight(real_t dist_sqr, real_t radius_sqr)
    {
        real_t power = -(BETA * 0.5F * (dist_sqr/radius_sqr));
        real_t numerator = 1.0F - pow(E, power);
        real_t denominator = ONE_MINUS_E_TO_MINUS_BETA;
        return ALPHA * (1.0F - (numerator/denominator));
    }

    inline real_t getConeFilterWeight(real_t dist_x_p, real_t dist_max)
    {
        return (real_t(1) - dist_x_p/(CONE_K * dist_max));
    }

    void Raytracer::balance(size_t index, std::vector<Photon> &balancedKDTree, std::vector<Photon> &list)
    {
        if (index == 1) {
            assert((balancedKDTree.size() == (list.size() + 1)));
        }

        if (list.size() == 1) {
            balancedKDTree[index] = list[0];
            return;
        }

        // If there is no data in photon list, do nothing and return
        if (list.size() == 0) {
            // actually program should not run to here, this should be checked on subdivision
            return;
        }

        // get the surrounding cube
        // O(N)
        Vector3 max = Vector3(-INFINITY, -INFINITY, -INFINITY);
        Vector3 min = Vector3(INFINITY, INFINITY, INFINITY);
        for (std::vector<Photon>::iterator it = list.begin(); it != list.end(); it++)
        {
            // calculate box
            max.x = it->position.x >= max.x ? it->position.x : max.x;
            max.y = it->position.y >= max.y ? it->position.y : max.y;
            max.z = it->position.z >= max.z ? it->position.z : max.z;

            min.x = it->position.x <= min.x ? it->position.x : min.x;
            min.y = it->position.y <= min.y ? it->position.y : min.y;
            min.z = it->position.z <= min.z ? it->position.z : min.z;
        }

        char splitAxis = -1;
        Vector3 diff = Vector3(max.x - min.x, max.y - min.y, max.z - min.z);
        if ((diff.x >= diff.y) && (diff.x >= diff.z))
            splitAxis = 0;
        else if ((diff.y >= diff.x) && (diff.y >= diff.z))
            splitAxis = 1;
        else if ((diff.z >= diff.x) && (diff.z >= diff.y))
            splitAxis = 2;

        // Sorting the vector
        bool (*comparator)(const Photon &a, const Photon &b) = NULL;

        switch (splitAxis) {
            case 0:
                comparator = photonComparatorX;
                break;
            case 1:
                comparator = photonComparatorY;
                break;
            case 2:
                comparator = photonComparatorZ;
                break;

            default:
                break;
        }

        // O(NlogN)
        std::sort(list.begin(), list.end(), comparator);

        // On Left-balancing Binary Trees, J. Andreas Brentzen (jab@imm.dtu.dk)
        size_t N = list.size();
        size_t exp = (size_t)log2(N);
        size_t M = pow(2, exp);
        size_t R = N - (M - 1);
        size_t LT, RT;
        if (R <= M/2)
        {
            LT = (M - 2)/2 + R;
            RT = (M - 2)/2;
        }
        else
        {
            LT = (M - 2)/2 + M/2;
            RT = (M - 2)/2 + R - M/2;
        }
        size_t const median = LT;

        // if more than One data
        Photon p = list[median];
        p.splitAxis = splitAxis;
        assert(index < balancedKDTree.size());
        balancedKDTree[index] = p;

//        printf("LT = %d, RT = %d\n", LT, RT);

        if (LT > 0) {
            std::vector<Photon> leftList(list.begin(), list.begin() + median);
            balance(2 * index, balancedKDTree, leftList);
        }

        if (RT > 0) {
            std::vector<Photon> rightList(list.begin() + median + 1, list.end());
            balance(2 * index + 1, balancedKDTree, rightList);
        }

        list.clear();
    }


    // TODO: change to c style kdtree, and use ispc for parallelism
    void Raytracer::cPhotonBalance(size_t index, std::vector<cPhoton> &balancedKDTree, std::vector<cPhoton> &list)
    {
        if (index == 1) {
            assert((balancedKDTree.size() == (list.size() + 1)));
        }

        if (list.size() == 1) {
            balancedKDTree[index] = list[0];
            return;
        }

        // If there is no data in photon list, do nothing and return
        if (list.size() == 0) {
            // actually program should not run to here, this should be checked on subdivision
            return;
        }


        // get the surrounding cube
        // O(N)
        Vector3 max = Vector3(-INFINITY, -INFINITY, -INFINITY);
        Vector3 min = Vector3(INFINITY, INFINITY, INFINITY);
        // TODO: parallel
        for (std::vector<cPhoton>::iterator it = list.begin(); it != list.end(); it++)
        {
            // calculate box
            max.x = it->position[0] >= max.x ? it->position[0] : max.x;
            max.y = it->position[1] >= max.y ? it->position[1] : max.y;
            max.z = it->position[2] >= max.z ? it->position[2] : max.z;

            min.x = it->position[0] <= min.x ? it->position[0] : min.x;
            min.y = it->position[1] <= min.y ? it->position[1] : min.y;
            min.z = it->position[2] <= min.z ? it->position[2] : min.z;
        }

        char splitAxis = -1;
        Vector3 diff = Vector3(max.x - min.x, max.y - min.y, max.z - min.z);
        if ((diff.x >= diff.y) && (diff.x >= diff.z))
            splitAxis = 0;
        else if ((diff.y >= diff.x) && (diff.y >= diff.z))
            splitAxis = 1;
        else if ((diff.z >= diff.x) && (diff.z >= diff.y))
            splitAxis = 2;

        // Sorting the vector
        bool (*comparator)(const cPhoton &a, const cPhoton &b) = NULL;

        switch (splitAxis) {
            case 0:
                comparator = cPhotonComparatorX;
                break;
            case 1:
                comparator = cPhotonComparatorY;
                break;
            case 2:
                comparator = cPhotonComparatorZ;
                break;

            default:
                break;
        }

        // O(NlogN)
        std::sort(list.begin(), list.end(), comparator);

        // On Left-balancing Binary Trees, J. Andreas Brentzen (jab@imm.dtu.dk)
        size_t N = list.size();
        size_t exp = (size_t)log2(N);
        size_t M = pow(2, exp);
        size_t R = N - (M - 1);
        size_t LT, RT;
        if (R <= M/2)
        {
            LT = (M - 2)/2 + R;
            RT = (M - 2)/2;
        }
        else
        {
            LT = (M - 2)/2 + M/2;
            RT = (M - 2)/2 + R - M/2;
        }
        size_t const median = LT;

        // if more than One data
        cPhoton p = list[median];
        p.splitAxis = splitAxis;
        assert(index < balancedKDTree.size());
        balancedKDTree[index] = p;

//        printf("LT = %d, RT = %d\n", LT, RT);

        if (LT > 0) {
            std::vector<cPhoton> leftList(list.begin(), list.begin() + median);
            cPhotonBalance(2 * index, balancedKDTree, leftList);
        }

        if (RT > 0) {
            std::vector<cPhoton> rightList(list.begin() + median + 1, list.end());
            cPhotonBalance(2 * index + 1, balancedKDTree, rightList);
        }

        list.clear();

    }

    /**
     @brief locate the nearest photon list of given hit position
     @param p               balancedKDTree index, starts search from root node where p = 1
     @param position        hit position
     @param balancedKDTree  balanced kd tree, stored as a left-balanced kd tree, vector
     @param nearestPhotons  nearest photons found at hit position
     @param sqrDist         smallest squared distance
     @param maxNum          maximum number of photons needed
     */
    void Raytracer::locatePhotons(size_t p,
                                  Vector3 position,
                                  std::vector<Photon> &balancedKDTree,
                                  std::vector<Photon> &nearestPhotons,
                                  float &sqrDist,
                                  size_t maxNum)
    {
        // examine child nodes
        Photon photon = balancedKDTree[p];

        if (2 * p + 1 < balancedKDTree.size())
        {
            assert(photon.splitAxis != -1);
            Vector3 diff = position - photon.position;
            real_t diffToPlane = 0.0;
            switch (photon.splitAxis) {
                case 0:
                    diffToPlane = diff.x;
                    break;
                case 1:
                    diffToPlane = diff.y;
                    break;
                case 2:
                    diffToPlane = diff.z;
                    break;
                default:
                    assert(0);
                    break;
            }
            real_t sqrDiffToPlane = diffToPlane * diffToPlane;

            if (diffToPlane < 0)
            {
                // search left subtree
                locatePhotons(2 * p, position, balancedKDTree, nearestPhotons, sqrDist, maxNum);
                if (sqrDiffToPlane < sqrDist)
                {
                    // check right subtree
                    locatePhotons(2 * p + 1, position, balancedKDTree, nearestPhotons, sqrDist, maxNum);
                }
            }
            else
            {
                // search right subtree
                locatePhotons(2 * p + 1, position, balancedKDTree, nearestPhotons, sqrDist, maxNum);
                if (sqrDiffToPlane < sqrDist)
                {
                    // check left subtree
                    locatePhotons(2 * p, position, balancedKDTree, nearestPhotons, sqrDist, maxNum);
                }
            }

        }

        // compute true squared distance to photon
        real_t sqrDistPhoton = squared_distance(position, photon.position);
        if (sqrDistPhoton <= sqrDist)
        {
            nearestPhotons.push_back(photon);
//            photon.squaredDistance = sqrDistPhoton;
//            if (nearestPhotons.size() < maxNum)
//            {
//                nearestPhotons.push_back(photon);
//                std::push_heap(nearestPhotons.begin(), nearestPhotons.end());
//            }
//            else
//            {
//                if (sqrDistPhoton < nearestPhotons.front().squaredDistance)
//                {
//                    std::pop_heap(nearestPhotons.begin(), nearestPhotons.end());
//                    nearestPhotons.pop_back();
//                    nearestPhotons.push_back(photon);
//                    std::push_heap(nearestPhotons.begin(), nearestPhotons.end());
//
//                    sqrDist = nearestPhotons.front().squaredDistance;
//                }
//            }
        }
    }

    // c style locate photons
    void Raytracer::cPhotonLocate(size_t p,
                                  Vector3 position,
                                  std::vector<cPhoton> &balancedKDTree,
                                  std::vector<cPhoton> &nearestPhotons,
                                  float &sqrDist,
                                  size_t maxNum)
    {
        assert(balancedKDTree.size() > 0);

        // examine child nodes
        cPhoton cphoton = balancedKDTree[p];
        Vector3 cphotonpos(cphoton.position);
//        Photon photon = dataSource[cphoton.index];

        if (2 * p + 1 < balancedKDTree.size())
        {
            assert(cphoton.splitAxis != -1);
            Vector3 diff = position - cphotonpos;
            real_t diffToPlane = 0.0;
            switch (cphoton.splitAxis) {
                case 0:
                    diffToPlane = diff.x;
                    break;
                case 1:
                    diffToPlane = diff.y;
                    break;
                case 2:
                    diffToPlane = diff.z;
                    break;
                default:
                    assert(0);
                    break;
            }
            float sqrDiffToPlane = diffToPlane * diffToPlane;

            if (diffToPlane < 0)
            {
                // search left subtree
                cPhotonLocate(2 * p, position, balancedKDTree, nearestPhotons, sqrDist, maxNum);
                if (sqrDiffToPlane < sqrDist)
                {
                    // check right subtree
                    cPhotonLocate(2 * p + 1, position, balancedKDTree, nearestPhotons, sqrDist, maxNum);
                }
            }
            else
            {
                // search right subtree
                cPhotonLocate(2 * p + 1, position, balancedKDTree, nearestPhotons, sqrDist, maxNum);
                if (sqrDiffToPlane < sqrDist)
                {
                    // check left subtree
                    cPhotonLocate(2 * p, position, balancedKDTree, nearestPhotons, sqrDist, maxNum);
                }
            }

        }

        // compute true squared distance to photon
        real_t sqrDistPhoton = squared_distance(position, cphotonpos);
        if (sqrDistPhoton <= sqrDist)
        {
            nearestPhotons.push_back(cphoton);
        }
    }

    void Raytracer::applyGammaHDR(Color3 &color)
    {
        real_t A = (0.5);
        real_t gamma = (0.5);

        color.r = A * pow(color.r, gamma);
        color.g = A * pow(color.g, gamma);
        color.b = A * pow(color.b, gamma);
    }

    void Raytracer::setPhotonDirection(Photon &photon, Vector3 dir)
    {
        // from jensen's implementation
        int theta = int( acos(dir.z)*(256.0/M_PI) );
        if (theta > 255)
            photon.theta = 255;
        else
            photon.theta = (unsigned char)theta;

        int phi = int( atan2(dir.y,dir.x) * (256.0 / (2.0 * M_PI)) );
        if (phi > 255)
            photon.phi = 255;
        else if (phi < 0)
            photon.phi = (unsigned char)(phi + 256);
        else
            photon.phi = (unsigned char)phi;
    }

    Vector3 Raytracer::getPhotonDirection(Photon &photon)
    {
        return Vector3(sintheta[photon.theta] * cosphi[photon.phi],
                       sintheta[photon.theta] * sinphi[photon.phi],
                       costheta[photon.theta]);
    }

    void Raytracer::vvhKDTreePreprocess(std::vector<cPhoton>& /*source*/,
                                        std::vector<metaCPhoton>& /*sortedSource*/)
    {
//        std::vector<cPhoton> datax = source;
//        std::vector<cPhoton> datay = source;
//        std::vector<cPhoton> dataz = source;
//
//        std::sort(datax.begin(), datax.end(), cPhotonComparatorX);
//        std::sort(datay.begin(), datay.end(), cPhotonComparatorY);
//        std::sort(dataz.begin(), dataz.end(), cPhotonComparatorZ);

    }

    /// vvh
    void Raytracer::vvhKDTreeConstruction(std::vector<cPhoton> &list, KDNode *root)
    {
        if (list.size() < 1) {
            return;
        }

        // VVH kdtree data
        std::vector<KDNode *> nodeList;
        std::vector<KDNode *> activeList;
        std::vector<KDNode *> smallList;
        std::vector<KDNode *> nextList;

        nodeList.clear();
        activeList.clear();
        smallList.clear();
        nextList.clear();

        // create root node
//        KDNode root;
//        root = new KDNode;
        root->isLeaf = false;
        root->head = 0;
        root->tail = list.size();

        activeList.push_back(root);

        // large node stage
        while (activeList.size() > 0)
        {
//            printf("active list size = %ld\n",activeList.size());
            vvhProcessLargeNode(activeList, smallList, nextList, list);
            nodeList.insert(nodeList.end(), activeList.begin(), activeList.end());

            // swap
            std::vector<KDNode *> tmp = activeList;
            activeList = nextList;
            nextList = tmp;

            nextList.clear();
        }

        // TODO
        // small node stage
        vvhPreprocessSmallNodes(smallList, nodeList, list);
        activeList = smallList;
        while (activeList.size() > 0)
        {
            vvhProcessSmallNode(activeList, nextList, list);
            nodeList.insert(nodeList.end(), activeList.begin(), activeList.end());

            // swap
            std::vector<KDNode *> tmp = activeList;
            activeList = nextList;
            nextList = tmp;

            nextList.clear();
        }

        // kd-tree output stage
        vvhPreorderTraversal(nodeList, list);
    }

    void Raytracer::vvhProcessLargeNode(std::vector<KDNode *> &activeList,
                                        std::vector<KDNode *> &smallList,
                                        std::vector<KDNode *> &nextList,
                                        std::vector<cPhoton> &list)
    {
        for (size_t i = 0; i < activeList.size(); i++)
        {
            // get the surrounding cube
            // O(N)
            Vector3 max = Vector3(-INFINITY, -INFINITY, -INFINITY);
            Vector3 min = Vector3(INFINITY, INFINITY, INFINITY);

            KDNode *node = activeList[i];
            int begin = node->head;
            int end = node->tail;

//            printf("node %d, begin = %d, end = %d\n", i, begin, end);
            for (int j = begin; j < end; j++)
            {
                // calculate box
                max.x = list[j].position[0] >= max.x ? list[j].position[0] : max.x;
                max.y = list[j].position[1] >= max.y ? list[j].position[1] : max.y;
                max.z = list[j].position[2] >= max.z ? list[j].position[2] : max.z;

                min.x = list[j].position[0] <= min.x ? list[j].position[0] : min.x;
                min.y = list[j].position[1] <= min.y ? list[j].position[1] : min.y;
                min.z = list[j].position[2] <= min.z ? list[j].position[2] : min.z;
            }

            int splitAxis = -1;
            Vector3 diff = Vector3(max.x - min.x, max.y - min.y, max.z - min.z);
            if ((diff.x >= diff.y) && (diff.x >= diff.z))
                splitAxis = 0;
            else if ((diff.y >= diff.x) && (diff.y >= diff.z))
                splitAxis = 1;
            else if ((diff.z >= diff.x) && (diff.z >= diff.y))
                splitAxis = 2;

            // Sorting the vector
            bool (*comparator)(const cPhoton &a, const cPhoton &b) = NULL;

            switch (splitAxis) {
                case 0:
                    comparator = cPhotonComparatorX;
                    break;
                case 1:
                    comparator = cPhotonComparatorY;
                    break;
                case 2:
                    comparator = cPhotonComparatorZ;
                    break;

                default:
                    break;
            }

            // O(NlogN)
//            std::sort(list.begin() + begin, list.begin() + end, comparator);
            int median = node->head + (node->tail - node->head)/2;
            std::nth_element(list.begin() + begin, list.begin() + median, list.begin() + end, comparator);


            // Split node i at spatial median of the longest axis
            node->cphotonIndex = median;
            node->splitAxis = splitAxis;
            node->splitValue = list[node->cphotonIndex].position[splitAxis];
            list[node->cphotonIndex].splitAxis = splitAxis;

            KDNode *lch = new KDNode;
            KDNode *rch = new KDNode;

            lch->isLeaf = false;
            lch->head = begin;
            lch->tail = median;

            rch->isLeaf = false;
            rch->head = median + 1;
            rch->tail = end;

            node->left = lch;
            node->right = rch;

            if (lch->tail - lch->head <= SMALL_NODE_GRANULARITY)
            {
#if SIMPLE_SMALL_NODE
                lch->isLeaf = true;
#endif
                smallList.push_back(lch);
            }
            else
                nextList.push_back(lch);

            if (rch->tail - rch->head <= SMALL_NODE_GRANULARITY)
            {
#if SIMPLE_SMALL_NODE
                rch->isLeaf = true;
#endif
                smallList.push_back(rch);
            }
            else
                nextList.push_back(rch);
        }
    }

    void Raytracer::vvhPreprocessSmallNodes(std::vector<KDNode *> &smallList,
                                            std::vector<KDNode *> &nodeList,
                                            std::vector<cPhoton> & /*list*/)
    {
        // temporary treat all small nodes as leaf nodes
#if SIMPLE_SMALL_NODE
        nodeList.insert(nodeList.end(), smallList.begin(), smallList.end());
        smallList.clear();
#endif
    }

    void Raytracer::vvhProcessSmallNode(std::vector<KDNode *> &activelist,
                                        std::vector<KDNode *> &nextList,
                                        std::vector<cPhoton> &list)
    {
        for (size_t i = 0; i < activelist.size(); i++)
        {
            KDNode *node = activelist[i];
            float VVH0 = node->tail - node->head;
            std::vector<cPhoton> data(node->tail - node->head);
            std::copy(list.begin() + node->head, list.begin() + node->tail, data.begin());
            char splitAxis = -1;
            if (vvhComputeVVH(data, VVH0, splitAxis))
            {
                // need split
                std::copy(data.begin(), data.end(), list.begin() + node->head);
                node->splitAxis = splitAxis;

                int begin = node->head;
                int end = node->tail;
                int median = node->head + (node->tail - node->head)/2;

                // Split node i at spatial median of the longest axis
                node->cphotonIndex = median;
                node->splitAxis = splitAxis;
                node->splitValue = list[node->cphotonIndex].position[(int)splitAxis];
                list[node->cphotonIndex].splitAxis = splitAxis;

                KDNode *lch = new KDNode;
                KDNode *rch = new KDNode;

                lch->isLeaf = false;
                lch->head = begin;
                lch->tail = median;

                rch->isLeaf = false;
                rch->head = median + 1;
                rch->tail = end;

                node->left = lch;
                node->right = rch;

                nextList.push_back(lch);
                nextList.push_back(rch);

            }
            else
            {
                activelist[i]->isLeaf = true;
            }

        }
    }

    bool Raytracer::vvhComputeVVH(std::vector<cPhoton> &data, float &VVH0, char &axis)
    {
        float VVH = VVH0;
        bool ret = false;
        char splitAxis = -1;
        std::vector<cPhoton> xdata = data;
        std::vector<cPhoton> ydata = data;
        std::vector<cPhoton> zdata = data;
        std::sort(xdata.begin(), xdata.end(), cPhotonComparatorX);
        std::sort(ydata.begin(), ydata.end(), cPhotonComparatorY);
        std::sort(zdata.begin(), zdata.end(), cPhotonComparatorZ);

        Vector3 a(xdata[0].position[0], ydata[0].position[1], zdata[0].position[2]);
        Vector3 b(xdata[xdata.size() - 1].position[0], ydata[ydata.size() - 1].position[1], zdata[zdata.size() - 1].position[2]);
        float v = vvhComputeVolume(a, b);

        // x axis
        for (size_t i = 1; i < xdata.size()-1; i++)
        {
            Vector3 la(xdata[0].position);
            Vector3 lb(xdata[i-1].position);
            Vector3 ra(xdata[i+1].position);
            Vector3 rb(xdata[xdata.size()-1].position);
            float vl = vvhComputeVolume(la, lb);
            float vr = vvhComputeVolume(ra, rb);
            float vvh_i = 1 + vl/v + vr/v;
            if (vvh_i < VVH) {
                VVH = vvh_i;
                splitAxis = 0;
            }
        }

        // y axis
        for (size_t i = 1; i < ydata.size()-1; i++)
        {
            Vector3 la(ydata[0].position);
            Vector3 lb(ydata[i-1].position);
            Vector3 ra(ydata[i+1].position);
            Vector3 rb(ydata[ydata.size()-1].position);
            float vl = vvhComputeVolume(la, lb);
            float vr = vvhComputeVolume(ra, rb);
            float vvh_i = 1 + vl/v + vr/v;
            if (vvh_i < VVH) {
                VVH = vvh_i;
                splitAxis = 1;
            }
        }

        // z axis
        for (size_t i = 1; i < zdata.size()-1; i++)
        {
            Vector3 la(zdata[0].position);
            Vector3 lb(zdata[i-1].position);
            Vector3 ra(zdata[i+1].position);
            Vector3 rb(zdata[zdata.size()-1].position);
            float vl = vvhComputeVolume(la, lb);
            float vr = vvhComputeVolume(ra, rb);
            float vvh_i = 1 + vl/v + vr/v;
            if (vvh_i < VVH) {
                VVH = vvh_i;
                splitAxis = 2;
            }
        }

        axis = splitAxis;

        // need split
        if (VVH < VVH0)
        {
            switch (splitAxis) {
                case 0:
                    data = xdata;
                    ret = true;
                    break;
                case 1:
                    data = ydata;
                    ret = true;
                    break;
                case 2:
                    data = zdata;
                    ret = true;
                    break;

                default:
                    break;
            }
        }

        return ret;
    }

    float Raytracer::vvhComputeVolume(Vector3 a, Vector3 b)
    {
        if (a == b)
            return SPHERE_VOLUME;

        Vector3 diff = b - a;
        return diff.x * diff.y * diff.z + TWO_MUL_RADIUS * (diff.x * diff.y + diff.x * diff.z + diff.y + diff.z ) + SPHERE_VOLUME;
    }

    void Raytracer::vvhPreorderTraversal(std::vector<KDNode *>& /*nodeList*/,
                                         std::vector<cPhoton> & /*list*/ )
    {
//        for (std::vector<KDNode *>::iterator it = nodeList.begin(); it != nodeList.end(); it++)
//        {
//            KDNode *node = *it;
//            printf("head = %d, tail = %d\n",node->head, node->tail);
//        }
//        exit(0);
    }

    void Raytracer::vvhcPhotonLocate(Vector3 position,
                                     KDNode *node,
                                     ispcCPhotonData &ispcCphotonData,
                                     std::vector<cPhoton> &cPhotonList,
                                     std::vector<int> &nearestPhotonsIndices,
                                     float &sqrDist,
                                     size_t maxNum)
    {
        assert(node != NULL);

        if (cPhotonList.size() < 1) {
            return;
        }

        // if the node is leaf, check children for get nearstPhotons
        if (node->isLeaf)
        {
            int begin = node->head;
            int end = node->tail;
            std::vector<cPhoton>::iterator it;

            for (int i = begin; i < end; i++)
            {
                cPhoton cphoton = cPhotonList[i];
                Vector3 cphotonpos(cphoton.position);

                // compute true squared distance to photon
                real_t sqrDistPhoton = squared_distance(position, cphotonpos);
                if (sqrDistPhoton <= sqrDist)
                {
                    nearestPhotonsIndices.push_back(cphoton.index);
                }
            }
        }
        // if the node is a splitting node
        else
        {
            cPhoton cphoton = cPhotonList[node->cphotonIndex];
            Vector3 cphotonpos(cphoton.position);

            Vector3 diff = position - cphotonpos;

            real_t diffToPlane = 0.0;
            switch (cphoton.splitAxis) {
                case 0:
                    diffToPlane = diff.x;
                    break;
                case 1:
                    diffToPlane = diff.y;
                    break;
                case 2:
                    diffToPlane = diff.z;
                    break;
                default:
                    assert(0);
                    break;
            }
            real_t sqrDiffToPlane = diffToPlane * diffToPlane;

            if (diffToPlane < 0)
            {
                // search left subtree
                vvhcPhotonLocate(position,
                                 node->left,
                                 ispcCphotonData,
                                 cPhotonList,
                                 nearestPhotonsIndices,
                                 sqrDist,
                                 maxNum);

                if (sqrDiffToPlane < sqrDist)
                {
                    // check right subtree
                    vvhcPhotonLocate(position,
                                     node->right,
                                     ispcCphotonData,
                                     cPhotonList,
                                     nearestPhotonsIndices,
                                     sqrDist,
                                     maxNum);
                }
            }
            else
            {
                // search right subtree
                vvhcPhotonLocate(position,
                                 node->right,
                                 ispcCphotonData,
                                 cPhotonList,
                                 nearestPhotonsIndices,
                                 sqrDist,
                                 maxNum);

                if (sqrDiffToPlane < sqrDist)
                {
                    // check left subtree
                    vvhcPhotonLocate(position,
                                     node->left,
                                     ispcCphotonData,
                                     cPhotonList,
                                     nearestPhotonsIndices,
                                     sqrDist,
                                     maxNum);
                }
            }

            // compute true squared distance to photon
            real_t sqrDistPhoton = squared_distance(position, cphotonpos);
            if (sqrDistPhoton <= sqrDist)
            {
                nearestPhotonsIndices.push_back(cphoton.index);
            }
        }
    }

    // synchronize root bounding boxes of all nodes
    void Raytracer::mpiStageNodeBoundingBox(int procs, int /* procId */)
    {
        scene->nodeBndBox = new BndBox[procs];
        BndBox curNodeBndBox = BndBox(Vector3::Zero());
        // 1.create node bounding box
        if (scene->num_geometries()) {
            curNodeBndBox = BndBox(scene->get_geometries()[0]->bbox_world.pMin, scene->get_geometries()[0]->bbox_world.pMax);
            for (size_t i = 1; i < scene->num_geometries(); i++) {
                curNodeBndBox = curNodeBndBox.expand(curNodeBndBox, BndBox(scene->get_geometries()[i]->bbox_world.pMin,
                                                            scene->get_geometries()[i]->bbox_world.pMax));
            }
        }

        int sendsize = sizeof(curNodeBndBox);
        int status = MPI_Allgather(&curNodeBndBox, sendsize, MPI_BYTE, scene->nodeBndBox, sendsize, MPI_BYTE, MPI_COMM_WORLD);
        if (status != 0)
        {
            printf("MPI error: %d\n", status);
            throw exception();
        }
    }
    
    void Raytracer::mpiStageDistributeEyeRays(int procs, int procId, std::vector<Ray> *eyerays)
    {
        int wstep = width / scene->node_size;
        int hstep = height;
        real_t dx = real_t(1)/width;
        real_t dy = real_t(1)/height;
        
        // node ray list to send out rays
        RayBucket currentNodeRayList(procs);
        
        // Generate all eye rays in screen region, bin them
        for (int y = 0; y < hstep; y++) {
            for (int x = wstep * procId; x < wstep * (procId + 1); x++) {
                
                // pick a point within the pixel boundaries to fire our
                // ray through.
                real_t i = real_t(2)*(real_t(x)+random())*dx - real_t(1);
                real_t j = real_t(2)*(real_t(y)+random())*dy - real_t(1);
                
                Ray r = Ray(scene->camera.get_position(), Ray::get_pixel_dir(i, j));
                for (int node_id = 0; node_id < procs; node_id++) {
                    BndBox nodeBBox = scene->nodeBndBox[node_id];
                    if (nodeBBox.intersect(r, EPSILON, TMAX)) {
                        // push ray into node's ray list
                        r.x = x;
                        r.y = y;
                        r.color = Color3::Black();
                        r.depth = 2;
                        currentNodeRayList.push_back(node_id, r);
                    }
                }
            }
        }
        
        // all to all distribute eye rays
        mpiAlltoallRayDistribution(procs, procId, currentNodeRayList, eyerays);
        
    }
    
    // each node do local raytracing, generate shadow rays, do shadowray-node boundingbox
    // test, distribute shadow rays, maintain local lookup table, send shadow rays to other nodes
    void Raytracer::mpiStageLocalRayTrace(int procs,
                                          int procId,
                                          std::vector<Ray> &eyerays,
                                          std::vector<Ray> *shadowRays,
                                          std::vector<Ray> *giRays,
                                          bool iseyeray)
    {
        // shadow ray list used for shading
        RayBucket nodeShadowRayList(procs);
        RayBucket nodeGIRayList(procs);
        
        // for each eye ray
        for (size_t i = 0; i < eyerays.size(); i++) {
            Ray ray = eyerays[i];
            bool isHit = false;
            HitRecord record = getClosestHit(ray, EPSILON, TMAX, &isHit, Layer_All);
            
            // calculate zbuffer value
            if (isHit) {
                
                // since time is the data used for depth buffer test,
                // only eye ray should update this value
                if (iseyeray)
                {
                    ray.time = record.t;
                }
                
                if (record.diffuse != Color3::Black() && record.refractive_index == 0)
                {
                    // for each light
                    for (size_t li = 0; li < scene->num_lights(); li++) {
                        
                        Light *aLight = scene->get_lights()[li];
                        // TODO: optimize
                        // shade the hit point color direclty
                        Vector3 samplePoint;
                        float tlight;
                        Color3 shadingColor = record.diffuse * aLight->SampleLight(record.position,
                                                                                   record.normal,
                                                                                   EPSILON,
                                                                                   TMAX,
                                                                                   &samplePoint,
                                                                                   &tlight);
                        shadingColor *= INV_PI;
                        
                        // for each light sample
                        // TODO: we sample only one point per light for now
                        Vector3 d_shadowRay_normolized = normalize(aLight->getPointToLightDirection(record.position, samplePoint));
                        
                        Ray shadowRay = Ray(record.position, d_shadowRay_normolized);
                        shadowRay.x = ray.x;
                        shadowRay.y = ray.y;
                        shadowRay.maxt = tlight;
                        shadowRay.lightIndex = li;
                        shadowRay.depth = ray.depth - 1;
                        shadowRay.color = shadingColor;
                        shadowRay.time = ray.time;
                        shadowRay.source = procId;
                        
                        // for each node bounding box
                        for (int bidx = 0; bidx < procs; bidx++) {
                            BndBox nodeBndBox = scene->nodeBndBox[bidx];
                            // if shadow ray hits any bounding boxes, send to other nodes for shading
                            if (nodeBndBox.intersect(shadowRay, EPSILON, shadowRay.maxt)) {
                                nodeShadowRayList.push_back(bidx, shadowRay);
                            }
                            // if shadow not hit any bounding boxes,
                            // send to self for shading, or the data will lose
                            else {
                                nodeShadowRayList.push_back(procId, shadowRay);
                            }
                        }
                        
                        if (ray.depth > 0) {
                            // generate second rays
                            Vector3 dir = uniformSampleHemisphere(record.normal);
                            Ray secondRay = Ray(record.position, dir);
                            secondRay.x = ray.x;
                            secondRay.y = ray.y;
                            secondRay.depth = ray.depth - 1;
                            secondRay.color = shadingColor;
                            secondRay.time = ray.time;
                            
                            // for each node bounding box
                            for (int bidx = 0; bidx < procs; bidx++) {
                                BndBox nodeBndBox = scene->nodeBndBox[bidx];
                                if (nodeBndBox.intersect(secondRay, EPSILON, TMAX)) {
                                    nodeGIRayList.push_back(bidx, secondRay);
                                }
                            }
                        }
                    }
                }
            }
        }
        
        mpiAlltoallRayDistribution(procs, procId, nodeShadowRayList, shadowRays);
        mpiAlltoallRayDistribution(procs, procId, nodeGIRayList, giRays);
    }

    // each node takes in shadow ray, do local ray tracing, maintain shadow ray
    // hit records, send records to corresponding nodes
    void Raytracer::mpiStageShadowRayTracing(int /*procs*/, int /*procId*/, FrameBuffer &buffer, std::vector<Ray> &shadowrays)
    {
        for (size_t i = 0; i < shadowrays.size(); i++)
        {
            Ray ray = shadowrays[i];
            // TODO: light index might be different?
            bool isHit = false;
            /*HitRecord shadowRecord =*/ getClosestHit(ray, EPSILON, ray.maxt, &isHit, (Layer_IgnoreShadowRay));
            
            int x = ray.x;
            int y = ray.y;

            int bufferIndex = y * width + x;
            if (isHit)
            {
                if (ray.time < buffer.zbuffer[bufferIndex]) {
                    buffer.shadowMap[bufferIndex] = 255;
                    ray.color.to_array(&buffer.cbuffer[4 * bufferIndex]);
                    buffer.zbuffer[bufferIndex] = ray.time;
                }
            }
            else
            {
                if (ray.time < buffer.zbuffer[bufferIndex]) {
                    buffer.shadowMap[bufferIndex] = 0;
                    ray.color.to_array(&buffer.cbuffer[4 * bufferIndex]);
                    buffer.zbuffer[bufferIndex] = ray.time;
                }
            }
        }
        
    }
    
    void Raytracer::mpiMergeFrameBufferToBuffer(int procs, int procId, FrameBuffer &buffer, unsigned char *rootbuffer)
    {
        assert(rootbuffer);
        
        // gather buffer from all other nodes to rbuf, zbuf
        size_t screensize = width * height;
        size_t buffersize = BUFFER_SIZE(width, height);
        
        unsigned char *rbuf = (unsigned char *)malloc(buffersize * (procs)); // color buffer
        real_t *zbuf = (real_t *)malloc(screensize * (procs) * sizeof(real_t));           // depth buffer
        char *sbuf = (char *)malloc(screensize *(procs) * sizeof(char));              // shadow map buffer
        
        MPI_Gather(buffer.cbuffer, buffersize, MPI_BYTE, rbuf, buffersize, MPI_BYTE, 0, MPI_COMM_WORLD);
        MPI_Gather(buffer.zbuffer, screensize, MPI_DOUBLE, zbuf, screensize, MPI_DOUBLE, 0, MPI_COMM_WORLD);
        MPI_Gather(buffer.shadowMap, screensize, MPI_UNSIGNED_CHAR, sbuf, screensize, MPI_UNSIGNED_CHAR, 0, MPI_COMM_WORLD);
        
        // merge buffers from all nodes
        // TODO: make it clearer
        for (size_t i = 0; i < width * height; i++) {
            
            // pre process shadow map
            real_t zmin = std::numeric_limits<real_t>::max();
            int zminId = -1;
            Color3 color = Color3::Black();
            
            for (int j = 0; j < procs; j++) {
                int index = j * screensize + i;
//                if (procId == 0)
//                    printf("id = %d\n", index);
                if (zbuf[index] < zmin) {
                    
                    zmin = zbuf[index];
                    zminId = index;
                }
                else if ( fabs(zbuf[index] - zmin) <= EPSILON) {
                    // if same but on is in shadow
                    if (sbuf[zminId] == 0)
                    {
                        zmin = zbuf[index];
                        zminId = index;
                    }
                }
            }
            
            
            
            if (zminId == -1)
            {
                color = scene->background_color;
                // DEBUG
                //                    color = Color3::White();
            }
            else
            {
                
                if (sbuf[zminId] != 0) {
                    color = Color3::Black(); // TODO: ambient
                }
                else {
                    color = Color3(&rbuf[zminId * 4]);
                    
                }
                
                // DEBUG, depth buffer
                //                    float value = zbuf[zminId]/19.42f;
                //                    value = clamp(value, 0.0f, 1.0f);
                //                    color = Color3(value, value, value);
            }
            
            color.to_array(&rootbuffer[i*4]);
        }        
        free(rbuf);
        free(zbuf);
        free(sbuf);
        
//        assert(rootbuffer);
//        
//        if (procId == 0) {
//            
//            // gather buffer from all other nodes to rbuf, zbuf
//            size_t screensize = width * height;
//            size_t buffersize = BUFFER_SIZE(width, height);
//            
//            unsigned char *rbuf = (unsigned char *)malloc(buffersize * (procs)); // color buffer
//            real_t *zbuf = (real_t *)malloc(screensize * (procs) * sizeof(real_t));           // depth buffer
//            char *sbuf = (char *)malloc(screensize *(procs) * sizeof(char));              // shadow map buffer
//            
//            assert(rbuf && zbuf && sbuf);
//            
//            for (int i = 1; i < procs; i++)
//            {
//                MPI_Recv(rbuf + (i) * buffersize, buffersize, MPI_UNSIGNED_CHAR, i, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
//                MPI_Recv(zbuf + (i) * screensize, screensize, MPI_REAL, i, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
//                MPI_Recv(sbuf + (i) * screensize, screensize, MPI_CHAR, i, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
//            }
//            
//            MPI_Barrier(MPI_COMM_WORLD);
//            
//            std::copy_n(buffer.cbuffer, buffersize, rbuf);
//            std::copy_n(buffer.zbuffer, screensize, zbuf);
//            std::copy_n(buffer.shadowMap, screensize, sbuf);
//            
//            printf("**************\n");
//            // merge buffers from all nodes
//            // TODO: make it clearer
//            for (size_t i = 0; i < width * height; i++) {
//                
//                // pre process shadow map
//                real_t zmin = std::numeric_limits<real_t>::max();
//                int zminId = -1;
//                Color3 color = Color3::Black();
//                
//                
//                printf("id = %d\n", i);
//                for (int j = 0; j < procs; j++) {
//                    int index = j * screensize + i;
//                    if (zbuf[index] < zmin) {
//                        zmin = zbuf[index];
//                        zminId = index;
//                    }
//                    else if ( fabs(zbuf[index] - zmin) <= EPSILON) {
//                        // if same but on is in shadow
//                        if (sbuf[zminId] == 0)
//                        {
//                            zmin = zbuf[index];
//                            zminId = index;
//                        }
//                    }
//                }
//                
//                
//
//                if (zminId == -1)
//                {
//                    color = scene->background_color;
//                    // DEBUG
////                    color = Color3::White();
//                }
//                else
//                {
//                    
//                    if (sbuf[zminId] != 0) {
//                        color = Color3::Black(); // TODO: ambient
//                    }
//                    else {
//                        color = Color3(&rbuf[zminId * 4]);
//
//                    }
//                    
//                    // DEBUG, depth buffer
////                    float value = zbuf[zminId]/19.42f;
////                    value = clamp(value, 0.0f, 1.0f);
////                    color = Color3(value, value, value);
//                }
//                
//                color.to_array(&rootbuffer[i*4]);
//            }
//            printf("~~~~~~~~~~~~~~~~\n");
//            
//            free(rbuf);
//            free(zbuf);
//            free(sbuf);
//            
//        }
//        else {
//            MPI_Send((unsigned char *)buffer.cbuffer, BUFFER_SIZE(width, height), MPI_UNSIGNED_CHAR, 0, 0, MPI_COMM_WORLD);
//            MPI_Send((real_t *)buffer.zbuffer, width * height, MPI_REAL, 0, 0, MPI_COMM_WORLD);
//            MPI_Send((char *)buffer.shadowMap, width * height, MPI_CHAR, 0, 0, MPI_COMM_WORLD);
//            
//            MPI_Barrier(MPI_COMM_WORLD);
//        }
    }
    
    void Raytracer::mpiAlltoallRayDistribution(int procs, int /* procId */, RayBucket &inputRayBucket, std::vector<Ray> *outputRayList)
    {
        Ray *sendbuf;
        int *sendcounts;
        int *sendoffsets;
        // generate data to send
        inputRayBucket.mpi_datagen(&sendbuf, &sendoffsets, &sendcounts);
        
        // but first we need to send how many data each node are going to receive
        int status = -1;
        int *recvcounts = new int[procs];
        status = MPI_Alltoall(sendcounts, 1, MPI_INT, recvcounts, 1, MPI_INT, MPI_COMM_WORLD);
        if (status != 0) {
            printf("Fail to send and receive ray count info!\n");
            throw exception();
        }
        
        // Received ray buffer
        int *recvoffsets = new int[procs];
        int total = 0;
        for (int i = 0; i < procs; i++) {
            recvoffsets[i] = total;
            total += recvcounts[i];
        }
        Ray *recvbuf = new Ray[total];
        
        
        // reset counts, offsets for byte sized send/recv
        size_t raysize = sizeof(Ray);
        for (int i = 0; i < procs; i++) {
            sendcounts[i] *= raysize;
            sendoffsets[i] *= raysize;
            
            recvcounts[i] *= raysize;
            recvoffsets[i] *= raysize;
        }
        
        // send all rays by MPI alltoallv
        status = MPI_Alltoallv(sendbuf, sendcounts, sendoffsets, MPI_BYTE, recvbuf, recvcounts, recvoffsets, MPI_BYTE, MPI_COMM_WORLD);
        if (status != 0) {
            printf("Fail to send and receive rays!\n");
            throw exception();
        }
        
        std::vector<Ray> recvRayList(total);
        std::copy(recvbuf, recvbuf+total, recvRayList.begin());
        *outputRayList = recvRayList;
        
        delete sendbuf;
        delete sendcounts;
        delete sendoffsets;
        
        delete recvbuf;
        delete recvcounts;
        delete recvoffsets;
    }
    
    
    void Raytracer::mergebuffers(unsigned char *dibuffer, unsigned char *gibuffer, int width, int height)
    {
        assert(width > 0 && height > 0);
        assert(dibuffer);
        if (!gibuffer) {
            return;
        }
        
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                int index = 4 * (y * width + x);
                Color3 diColor = Color3(&dibuffer[index]);
                Color3 giColor = Color3(&gibuffer[index]);
                Color3 res = diColor + INV_PI * giColor;
                res.to_array(&dibuffer[index]);
            }
        }
    }
    
    bool Raytracer::mpiShouldStop(int procs, int procId, int shadowRaySize, int giRaySize)
    {
        int size = shadowRaySize + giRaySize;
        int *recvsize = new int[procs];
        
        int status = MPI_Allgather(&size, 1, MPI_INT, recvsize, 1, MPI_INT, MPI_COMM_WORLD);
        if (status != 0) {
            printf("Fail to send and receive next ray size info!\n");
            throw exception();
        }
        
        for (int i = 0; i < procs; i++) {
            if (recvsize[i] > 0)
                return false;
        }
        return true;
    }
    


} /* _462 */
