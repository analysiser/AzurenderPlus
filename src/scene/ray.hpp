#ifndef _462_SCENE_RAY_HPP_
#define _462_SCENE_RAY_HPP_

#include "math/vector.hpp"
#include "math/quaternion.hpp"
#include "math/matrix.hpp"
#include "math/camera.hpp"
#include "scene/material.hpp"
#include "scene/mesh.hpp"
#include "raytracer/Photon.hpp"
#include <string>
#include <vector>


namespace _462 {
    
    using namespace std;
    
    enum ERayType
    {
        ERayType_Eye = 0,
        ERayType_Shadow = 1,
        ERayType_GI = 2,
        
    };
    
    struct Ray
    {
        
    public:
        Vector3 e;
        Vector3 d;
        Ray();
        Ray(Ray *ray);
        Ray(Vector3 e, Vector3 d);
        Ray(Vector3 e, Vector3 d, float start, float end, float time);
        
        // Photon carried by ray
        Photon photon;
        
        static Vector3 get_pixel_dir(real_t x, real_t y);
        static void init(const Camera& camera);
        
        Color3 color;
        float mint, maxt, time;
        int x, y;
        int depth;
        ERayType type;
        bool isHit;
    };
    
    typedef std::vector<std::vector<Ray> > NodeRayVector;
    class RayBucket {
    public:
        RayBucket() {}
        ~RayBucket() {
            for (int i = 0; i < node_size_; i++) {
                nodeRayVector[i].clear();
            }
            nodeRayVector.clear();
            node_size_ = 0;
        }
        
        RayBucket(int node_size)
        {
            assert(node_size > 0);
            node_size_ = node_size;
            for (int i = 0; i < node_size; i++) {
                std::vector<Ray> aList;
                nodeRayVector.push_back(aList);
            }
        }
        
        // push a ray to group numbered "rank"
        void push_back(int rank, const Ray &r)
        {
            assert(rank < node_size_);
            nodeRayVector[rank].push_back(r);
        }
        
        /*! 
         @brief Call this only when you done of pushing all elements, it is used 
                for generate basic data for open MPI_Alltoallv sending
         @param out raybucket: c-style node ray array
         @param out sendoffset: sender buffer displacements of j group from raybucket
         @param out sendcounts: sender buffer element counts of j group
         @note  Very important! Numbers used in the sendoffset and sendcounts are COUNTS
                These two value array cannot by used directly for sending MPI information
         */
        void mpi_datagen(Ray **raybucket, int **sendoffset, int **sendcounts)
        {
            size_t size = nodeRayVector.size();
            if (size == 0) {
                printf("No rays for current node!\n");
                return;
            }
            *sendcounts = new int[size];
            *sendoffset = new int[size];
            size_t total = 0;
            
            for (size_t i = 0; i < size; i++) {
                (*sendcounts)[i] = nodeRayVector[i].size();
                (*sendoffset)[i] = total;
                total += (*sendcounts)[i];
            }
            
            *raybucket = new Ray[total];
            
            for (size_t i = 0; i < size; i++) {
                std::copy(nodeRayVector[i].begin(), nodeRayVector[i].end(), *raybucket + (*sendoffset)[i]);
            }
        }
        
    private:
        NodeRayVector nodeRayVector;
        int node_size_;
        
    };
    
    struct Intersection
    {
        float t;
        uint32_t shapeId;
        uint32_t primitiveId;
    };
    
    /**
     * @brief Data structure for intersection
     */
    struct HitRecord
    {
        Vector3 position;       // world position
        Vector3 normal;         // world normal
        
        Color3 diffuse;         // Surface diffusive color
        Color3 ambient;         // Surface ambien color
        Color3 specular;        // Surface specular color
        Color3 texture;         // Surface texture color
        
        real_t t;               // Hit time
        
        real_t refractive_index;    // refractive index
        
        int phong;           // p value for blinn-phong model
        
        bool isHit;
        
        // Constructor
        HitRecord()
        {
            position = Vector3::Zero();
            normal = Vector3::Zero();
            diffuse = Color3::White();
            ambient = Color3::White();
            specular = Color3::White();
            
            t = std::numeric_limits<float>::max();
            phong = 0;
            isHit = false;
        }
        
        Color3 getPhotonLambertianColor(Vector3 direction, Color3 photonColor)
        {
            Vector3 l = direction;//normalize(direction);
            Vector3 n = this->normal;
            
            real_t dot_nl = dot(n, l);
            
            Color3 lambertian = this->diffuse * photonColor * (dot_nl > 0 ? dot_nl : 0);
            return lambertian;
        }
    };
    
    
    template<typename T>
    class azPacket
    {
    public:
        azPacket(){size_ = 0;}
        
        azPacket(size_t size) {
            size_ = size;
            T t;
            list_ = vector<T>(size_, t);
            
            setReady();
        }
        
        void add(T &t)
        {
            list_.push_back(t);
        }
        
        void setReady()
        {
            size_ = list_.size();
            active_ = vector<bool>(size_, true);   
        }
        
        void clear()
        {
            size_ = 0;
            active_.clear();
            list_.clear();
        }
        
        size_t size(){ return size_; }
        
        T& operator[](const size_t idx)
        {
            assert(idx < size_);
            return list_[idx];
        }
        
        void setMask(size_t idx, bool active) {
            assert(idx < size_);
            active_[idx] = active;
        }
        
        bool getMask(size_t idx) {
            assert(idx < size_);
            return active_[idx];
        }
        
        class Iterator
        {
        private:
            azPacket<T> packet_;
            size_t cur_;
            bool isDone_;
        public:
            Iterator(azPacket<T> &packet)
            {
                packet_ = packet;
                reset();
            }
            
            void reset()
            {
                cur_ = 0;
                isDone_ = true;
                
                for (size_t i = 0; i < packet_.size(); i++)
                {
                    if (packet_.getMask(i))
                    {
                        isDone_ = false;
                        break;
                    }
                }
            }
            
            bool isDone()
            {
                return (cur_ >= packet_.size());
            }
            
            bool isValid()
            {
//                printf("%d, %d\n", cur_, packet_.size());
                return (cur_ < packet_.size());
            }
            
            void moveNext()
            {
                cur_++;
                while (isValid())
                {
                    if (!packet_.getMask(cur_))
                        cur_++;
                    else
                        break;
                }
            }
            
            T& getCurItem()
            {
                return packet_[cur_];
            }
            
            T& operator[](const size_t &idx)
            {
                return packet_[idx];
            }
            
            size_t getCurrentIndex()
            {
                return cur_;
            }
        };
        
    private:
        size_t size_;
        vector<T> list_;
        vector<bool> active_;
    };
    
}
#endif
