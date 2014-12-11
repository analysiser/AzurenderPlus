#pragma once
#include <mpi.h>
#include "scene/ray.hpp"

namespace _462
{
    class MPICommunicate
    {
    public:
      static void SendRay(Ray &ray, int dest);
      static void ISendRay(Ray *ray, int dest);
      static void RecvRay(Ray &ray);
      static void BroadCast(Ray &ray);
  };
}  // namespace _462
