#pragma once

#include "../scene/ray.hpp"

namespace _462
{
  class RayList
  {
    public:
      RayList(int num_nodes);
      RayList(void *bytes);
      ~RayList();

      typedef std::vector<std::vector<Ray *>> RayListType;

      void push_back(Ray &ray, int32_t node_id);
      void push_back(Ray *ray, int32_t node_id);
      size_t serialize(char **bytes);

      int *get_ray_size_list();

      RayListType ray_list_;

    private:
      int num_nodes_;
      int *ray_size_list_;
      int num_ray_;

      size_t get_ray_size_list_size()
      {
        return num_nodes_ * sizeof(int);
      }
  };
}  // namespace _462
