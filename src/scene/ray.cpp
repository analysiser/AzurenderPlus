#include "ray.hpp"

namespace _462 {
    
    Vector3 Ray::dir;
    Vector3 Ray::up;
    real_t Ray::AR;
    
    Vector3 Ray::cR;
    Vector3 Ray::cU;
    real_t Ray::dist;
    Vector3 Ray::pos;
    
    Ray::Ray(){}
    
    Ray::Ray(Ray *ray)
    {
        Ray(ray->e, ray->d);
        
        this->depth = ray->depth;
        this->type = ray->type;
        
        this->x = ray->x;
        this->y = ray->y;
        this->color = ray->color;
        
        this->mint = ray->mint;
        this->maxt = ray->maxt;
        this->time = ray->time;
        
        this->isHit = ray->isHit;
        
        this->hit = ray->hit;
        this->hitNormal = ray->hitNormal;
    }
    
    Ray::Ray(Vector3 e, Vector3 d)
    {
        this->e = e;
        this->d = d;
        this->mint = 0.f;
        this->maxt = INFINITY;
        this->time = 0.f;
        
        this->hit = Vector3::Zero();
        this->hitNormal = Vector3::Zero();
    }
    
    Ray::Ray(Vector3 e, Vector3 d, float start, float end, float time)
    {
        this->e = e;
        this->d = d;
        this->mint = start;
        this->maxt = end;
        this->time = time;
    }
    
    void Ray::init(const Camera& camera)
    {
        Ray::dir = camera.get_direction();
        Ray::up = camera.get_up();
        Ray::AR = camera.get_aspect_ratio();
        Ray::cR = cross(dir, up);
        Ray::cU = cross(cR, dir);
        Ray::pos = camera.get_position();
        Ray::dist = tan(camera.get_fov_radians()/2.0);
    }
    
    Vector3 Ray::get_pixel_dir(real_t ni, real_t nj)
    {
        return normalize(dir + dist*(nj*cU + AR*ni*cR));
    }
    
}
