/**
 * @file model.hpp
 * @brief Model class
 *
 * @author Eric Butler (edbutler)
 */

#ifndef _462_SCENE_MODEL_HPP_
#define _462_SCENE_MODEL_HPP_

#include "scene/azBVHTree.hpp"
#include "scene/mesh.hpp"
#include "scene/scene.hpp"
#include "scene/BndBox.hpp"

#include <iostream>
#include <set>
#include <vector>
#include <map>
#include <list>

namespace _462 {

    class azBVHTree;
    /**
     * A mesh of triangles.
     */
    class Model : public Geometry
    {
    public:

        const Mesh* mesh;
        const Material* material;

        mutable azBVHTree *bvhTree;

//         BndBox *modelBndBox;

        Model();

        virtual ~Model();

        virtual void render() const;

        // Create the bounding box for the whole mesh
        virtual void createBoundingBox() const;

        // Override of virtual function from Geometry
        virtual bool hit(Ray ray, real_t t0, real_t t1, HitRecord &rec) const;

        // Override of virtual function for packetized ray hit
        virtual void packetHit(azPacket<Ray> &rays, azPacket<HitRecord> &hitInfo, float t0, float t1) const;

    };

} /* _462 */

#endif /* _462_SCENE_MODEL_HPP_ */
