#include "ray_list.hpp"

namespace _462
{
    
    RayList::RayList(int num_nodes)
    : num_nodes_(num_nodes),
    ray_size_list_(new int[num_nodes]),
    num_ray_(0)
    {
    }
    
    RayList::RayList(void *bytes)
    {
        int offset = 0;
        memcpy(ray_size_list_, bytes, get_ray_size_list_size());
        offset += get_ray_size_list_size();
        
        int sum = 0;
        for (int i = 0; i < num_nodes_; i++)
        {
            sum += ray_size_list_[i];
        }
        
        for (int i = 0; i < sum; i++)
        {
            Ray *r = new Ray(reinterpret_cast<Ray *>((Ray *)bytes + offset));
            push_back(r, 0);
            offset += sizeof(Ray);
        }
        
    }
    
    RayList::~RayList()
    {
        for (int i = 0; i < ray_list_.size(); i++)
        {
            for (int j = 0; j < ray_list_[i].size(); j++)
            {
                if (ray_list_[i][j] != NULL)
                    delete ray_list_[i][j];
            }
        }
        delete [] ray_size_list_;
    }
    
    void RayList::push_back(Ray &ray, int32_t node_id)
    {
        Ray *new_ray = new Ray(&ray);
        return push_back(new_ray, node_id);
    }
    
    void RayList::push_back(Ray *ray, int32_t node_id)
    {
        ray_size_list_[node_id]++;
        num_ray_++;
        return ray_list_[node_id].push_back(ray);
    }
    
    size_t RayList::serialize(char **bytes)
    {
        // Calculate the size needed
        size_t size = num_ray_ * sizeof(Ray);
        size += get_ray_size_list_size();
        *bytes = new char[size];
        int offset = 0;
        memcpy(*bytes, ray_size_list_, get_ray_size_list_size());
        offset += get_ray_size_list_size();
        for (int i = 0; i < ray_list_.size(); i++)
        {
            for (int j = 0; j < ray_list_[i].size(); j++)
            {
                Ray *r = ray_list_[i][j];
                memcpy(*bytes + offset, r, sizeof(Ray));
                offset += sizeof(Ray);
            }
        }
    }
    
    int *RayList::get_ray_size_list()
    {
        return ray_size_list_;
    }
    
} // namespace _462
