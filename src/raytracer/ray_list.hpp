#pragma once

#include "../scene/ray.hpp"

namespace _462
{
    class RayList
    {
    public:
      typedef std::vector<std::vector<Ray *>> RayListType;

      RayList(int num_nodes);
      RayList(void *bytes, int num_received);
      ~RayList();

      void push_back(Ray &ray, int32_t node_id);
      void push_back(Ray *ray, int32_t node_id);
      size_t serialize(char **bytes);

      RayListType &get_ray_list();

      void get_ray_bin_size(int *list);
      void get_serialize_displacement(int *list);
      void get_send_buf_size_list(int *list);

    private:
      RayListType ray_list_;
      int num_nodes_;
      int num_ray_;

      // Is the memory owned by the class or outside the class.
      // If outside the class. The creator of the class is responsible
      // for free the memory.
      bool internal_mem_;

  };
}  // namespace _462
