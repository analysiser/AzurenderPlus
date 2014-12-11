#ifndef _AZMPI_HPP_
#define _AZMPI_HPP_

#include "raytracer.hpp"

#include "math/math.hpp"


#include "raytracer/azReflection.hpp"
#include "raytracer/azMPIRaytrace.hpp"
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

#include <queue>

namespace _462 {

  class azMPI {

    public:

      azMPI() {}
      ~azMPI() {}
      azMPI(
          Scene *aScene,
          FrameBuffer &buffer,
          size_t width,
          size_t height) :
        procs(aScene->node_size),
        procId(aScene->node_rank),
        raytracer(aScene, &buffer, width, height),
        result_list(procs)
    {

    }

      void mpiPathTrace(vector<Ray> &eyerays);

      void run();

    private:
      // the scene to trace
      int procs;
      int procId;
      std::queue<Ray> wait_queue;
      azMPIRaytrace raytracer;
      RayBucket result_list;

      void slave_process_ray();
      void master_process_ray();

      // exchange ray
      void exchange_ray();
      // exchange work load
      bool exchange_workload();

  };

}


#endif
