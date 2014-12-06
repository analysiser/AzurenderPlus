#include "ray_list.hpp"
#include <cstring>

namespace _462
{
  RayList::RayList(int num_nodes)
    : num_nodes_(num_nodes),
    num_ray_(0),
    internal_mem_(true)
  {
  }

  RayList::RayList(void *bytes, int num_received)
    :internal_mem_(false)
  {
    int offset = 0;

    for (int i = 0; i < num_received; i++)
    {
      Ray *r = reinterpret_cast<Ray *>(reinterpret_cast<char *>(bytes) + offset);
      push_back(r, 0);
      offset += sizeof(Ray);
    }
  }

  RayList::~RayList()
  {
    if (!internal_mem_)
    {
      return;
    }
    for (size_t i = 0; i < ray_list_.size(); i++)
    {
      for (size_t j = 0; j < ray_list_[i].size(); j++)
      {
        if (ray_list_[i][j] != NULL)
          delete ray_list_[i][j];
      }
    }
  }

  void RayList::push_back(Ray &ray, int32_t node_id)
  {
    Ray *new_ray = new Ray(ray);
    return push_back(new_ray, node_id);
  }

  void RayList::push_back(Ray *ray, int32_t node_id)
  {
    num_ray_++;
    return ray_list_[node_id].push_back(ray);
  }

  size_t RayList::serialize(char **bytes)
  {
    // Calculate the size needed
    size_t size = num_ray_ * sizeof(Ray);
    *bytes = new char[size];
    int offset = 0;

    for (size_t i = 0; i < ray_list_.size(); i++)
    {
      for (size_t j = 0; j < ray_list_[i].size(); j++)
      {
        Ray *r = ray_list_[i][j];
        memcpy(*bytes + offset, r, sizeof(Ray));
        offset += sizeof(Ray);
      }
    }
    return size;
  }

  RayList::RayListType &RayList::get_ray_list()
  {
    return ray_list_;
  }

  void RayList::get_ray_bin_size(int *list)
  {
    for (int i = 0; i < num_nodes_; i++)
    {
      list[i] = ray_list_[i].size();
    }
    return;
  }


  void RayList::get_serialize_displacement(int *list)
  {
    list[0] = 0;
    for (int i = 0; i < num_nodes_-1; i++)
    {
      list[i+1] = list[i] + (ray_list_[i].size()) * sizeof(Ray) + 1;
    }
    return;
  }

  void RayList::get_send_buf_size_list(int *list)
  {
    for (int i = 0; i < num_nodes_; i++)
    {
      list[i] = (ray_list_[i].size()) * sizeof(Ray);
    }
    return;
  }

} // namespace _462
