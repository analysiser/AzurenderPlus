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
      azMPI(Scene *aScene, FrameBuffer *buffer) : raytracer(aScene, buffer) {

        scene = aScene;
        framebuffer = buffer;

        procs = scene->node_size;
        procId = scene->node_rank;

      }

      void mpiPathTrace();

      // generate eye ray

      // distribute eye ray

      // send ray, TODO: send count

      // recv rays, TODO: recv count
      // if recv eye ray, shade, generate shadow ray, send, generate gi ray, send
      // if shadow ray, call update framebuffer

      // check if end
      bool isFinish();




    private:
      // the scene to trace
      Scene* scene;
      FrameBuffer *framebuffer;

      azMPIRaytrace raytracer;

      int procs;
      int procId;

  };

}


#endif
