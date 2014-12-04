/**
 * @file scene.cpp
 * @brief Function definitions for scenes.
 *
 * @author Eric Butler (edbutler)
 * @author Kristin Siu (kasiu)
 */

#include "scene/scene.hpp"

namespace _462 {
    
    
    Geometry::Geometry():
    position(Vector3::Zero()),
    orientation(Quaternion::Identity()),
    scale(Vector3::Ones())
    {
        
    }
    
    Geometry::~Geometry() {
        if (bbox_local) {
            delete bbox_local;
        }
        if (bbox_world) {
            delete bbox_world;
        }
    }
    
    bool Geometry::initialize()
    {
        orientation = normalize(orientation);
        make_inverse_transformation_matrix(&matWorldToLocal, position, orientation, scale);
        make_transformation_matrix(&matLocalToWorld, position, orientation, scale);
        make_normal_matrix(&normMat, matLocalToWorld);
        
        position_local = matWorldToLocal.transform_point(position);
        
        return true;
    }
    
    Scene::Scene()
    {
        reset();
    }
    
    Scene::~Scene()
    {
        reset();
    }
    
    bool Scene::initialize()
    {
        bool res = true;
        for (unsigned int i = 0; i < num_geometries(); i++)
            res &= geometries[i]->initialize();
        return res;
    }
    
    
    Geometry* const* Scene::get_geometries() const
    {
        return geometries.empty() ? NULL : &geometries[0];
    }
    
    size_t Scene::num_geometries() const
    {
        return geometries.size();
    }
    
//    const SphereLight* Scene::get_lights() const
//    {
////        return point_lights.empty() ? NULL : &point_lights[0];
//        return lights.size();
//    }
//    
//    size_t Scene::num_lights() const
//    {
//        return point_lights.size();
//    }
    
    Material* const* Scene::get_materials() const
    {
        return materials.empty() ? NULL : &materials[0];
    }
    
    size_t Scene::num_materials() const
    {
        return materials.size();
    }
    
    Mesh* const* Scene::get_meshes() const
    {
        return meshes.empty() ? NULL : &meshes[0];
    }
    
    size_t Scene::num_meshes() const
    {
        return meshes.size();
    }
    
    Light* const* Scene::get_lights() const
    {
        return lights.empty() ? NULL : &lights[0];
    }
    
    size_t Scene::num_lights() const
    {
        return lights.size();
    }
    
    void Scene::reset()
    {
        for ( GeometryList::iterator i = geometries.begin(); i != geometries.end(); ++i ) {
            delete *i;
        }
        for ( MaterialList::iterator i = materials.begin(); i != materials.end(); ++i ) {
            delete *i;
        }
        for ( MeshList::iterator i = meshes.begin(); i != meshes.end(); ++i ) {
            delete *i;
        }
        
        geometries.clear();
        materials.clear();
        meshes.clear();
        
        camera = Camera();
        
        background_color = Color3::Black();
        ambient_light = Color3::Black();
        refractive_index = 1.0;
        
    }
    
    void Scene::add_geometry( Geometry* g )
    {
        geometries.push_back( g );
    }
    
    void Scene::add_material( Material* m )
    {
        materials.push_back( m );
    }
    
    void Scene::add_mesh( Mesh* m )
    {
        meshes.push_back( m );
    }

    void Scene::add_lights( Light *l )
    {
        lights.push_back(l);
    }
    
    
} /* _462 */

