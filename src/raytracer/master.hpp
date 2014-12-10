#pragma once

#include <vector>
#include "../scene/ray.hpp"

namespace _462
{
  class Master
  {
    public:
      Master(Scene *scene);
      ~Master();

      void ExchangeBoundingBox();
      void GenerateEyeRay(std::vector<Ray> &eyerays);
      void BinEyeRay(std::vector<Ray> &eyerays, RayList &ray_list);

      void Run();

    private:
      void mpiProcessEyeRay(Ray &ray);
      void mpiProcessShadowRay(Ray &ray);
      void mpiProcessDiffuseRay(Ray &ray);
      void mpiProcessLastRay(Ray &ray);

      Scene* scene;
      int procs;
      int procId;
  }
}  // namespace _462
