
#pragma once

#include "../scene/ray.hpp"
#include <unordered_map>
#include <algorithm>

namespace _462
{
  class RayMap
  {
    public:
      typedef struct
      {
        ERayType ray_type;
        int32_t depth;
        int32_t x;
        int32_t y;
      } key_t;

      typedef Ray value_t;

      typedef std::unordered_map<key_t, value_t, hash, eq> ray_map_t;

      RayMap();
      ~RayMap();

      ray_map_t &GetRayMap()
      {
        return ray_map;
      }

    private:
      struct hash
      {
        std::size_t operator()(const key_t &key) const
        {
          using std::size_t;
          using std::hash;
          using std::string;
          return ((hash<int>()(key.ray_type)
                ^ (hash<int>()(key.depth))
                ^ (hash<int>()(key.x))
                ^ (hash<int>()(key.y))
                );
        }
      };

      struct eq
      {
        bool operator()(const key_t &lhs, const key_t &rhs) const
        {
          if (
              lhs.ray_type == rhs.ray_type &&
              lhs.depth == rhs.depth &&
              lhs.x == rhs.x &&
              lhs.y == rhs.y
             )
            return true;
          else
            return false;
        }
      };

      ray_map_t ray_map;
  };
}  // namespace _462
