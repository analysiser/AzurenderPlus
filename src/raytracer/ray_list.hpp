#pragma once

#include "../scene/ray.hpp"

namespace _462
{
  class RayList
  {
    public:
      typedef std::vector<std::vector<Ray *>> RayListType;

      RayList(int num_nodes);
      RayList(void *bytes);
      ~RayList();

      void push_back(Ray &ray, int32_t node_id);
      void push_back(Ray *ray, int32_t node_id);
      size_t serialize(char **bytes);

      RayListType &get_ray_list();

    private:
      RayListType ray_list_;
      int num_nodes_;
      int num_ray_;

      // Is the memory owned by the class or outside the class.
      // If outside the class. The creator of the class is responsible
      // for free the memory.
      bool internal_mem_;

      struct ray_list_header
      {
        size_t num_rays;
        ray_list_header(int num_ray) : num_rays(num_ray) {}
        ray_list_header() {}
      };

  };
}  // namespace _462
