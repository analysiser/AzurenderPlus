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

namespace _462 {

  class azMPI {

    public:

      azMPI() {}
      azMPI(
          Scene *aScene,
          FrameBuffer *buffer,
          size_t width,
          size_t height)
        : raytracer(aScene,buffer, width, height),
        send_count(0),
        recv_count(0)
      {

        scene = aScene;
        framebuffer = buffer;

        procs = scene->node_size;
        procId = scene->node_rank;

      }

      void mpiPathTrace();

      void run_master();
      void run_slave();

    private:
      // the scene to trace
      Scene* scene;
      FrameBuffer *framebuffer;

      azMPIRaytrace raytracer;

      int procs;
      int procId;

      int send_count;
      int recv_count;

  };

}


#endif
