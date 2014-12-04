/**
 * @file model.cpp
 * @brief Model class
 *
 * @author Eric Butler (edbutler)
 * @author Zeyang Li (zeyangl)
 */

#include "scene/model.hpp"
#include "scene/material.hpp"
#include "application/opengl.hpp"
#include "scene/triangle.hpp"
#include <iostream>
#include <cstring>
#include <string>
#include <fstream>
#include <sstream>


namespace _462 {

    Model::Model() : mesh( 0 ), material( 0 ) {
        bvhTree = nullptr;
    }
    Model::~Model() {
    }

    void Model::render() const
    {
        if ( !mesh )
            return;
        if ( material )
            material->set_gl_state();
        mesh->render();
        if ( material )
            material->reset_gl_state();
    }

    void Model::createBoundingBox() const
    {
        if (bvhTree == nullptr) {

            MeshTriangle const *triangles = mesh->get_triangles();
            bvhTree = new azBVHTree(mesh->num_triangles());

            for (size_t i = 0; i < mesh->num_triangles(); i++) {

                Vector3 A = mesh->vertices[triangles[i].vertices[0]].position;
                Vector3 B = mesh->vertices[triangles[i].vertices[1]].position;
                Vector3 C = mesh->vertices[triangles[i].vertices[2]].position;

                BndBox bbox(A);
                bbox.include(B);
                bbox.include(C);

                bvhTree->setLeaf(azBVHTree::azBVNode(bbox, i), i);
            }

            bvhTree->buildBVHTree();
            bbox_local = new BndBox(bvhTree->root()->pMin, bvhTree->root()->pMax);
            bbox_world = new BndBox(matLocalToWorld.transform_bbox(*bbox_local));
        }
    }

    void Model::packetHit(azPacket<Ray> &rays, azPacket<HitRecord> &hitInfo, float t0, float t1) const
    {
        // TODO: frustum-bounding box intersection test
        azPacket<Ray>::Iterator it(rays);
        while (!it.isDone()) {
            auto & aRay = it.getCurItem();
            rays.setMask(it.getCurrentIndex(), bbox_world->intersect(aRay, t0, t1));
            it.moveNext();
        }

        it.reset();

        // If any of the input rays intersect with the bounding box of the model
        if (!it.isDone())
        {
            // Make a local ray list, init segMask list to true if the ray intersects with the model bounding box
            // init index list to -1 as default value
            azPacket<Ray> localRays;
            std::vector<bool> segMask;
            std::vector<std::int64_t> indexList;

            // initialization phase
            for (size_t i = 0; i < rays.size(); i++) {
                // TODO: reduce rays to track
                Ray r = Ray( matWorldToLocal.transform_point(rays[i].e), matWorldToLocal.transform_vector(rays[i].d), rays[i].mint, rays[i].maxt, rays[i].time );
                localRays.add(r);
                segMask.push_back(rays.getMask(i));
                indexList.push_back(-1);
            }

            localRays.setReady();

            // initialization of lambda expression for bvtree leaf node intersection test
            auto rayTriangleIntersectionTest = [this](Ray &rr, real_t tt0, real_t tt1, real_t &tt, INT64 triIndex) {
                MeshTriangle const *triangles = mesh->get_triangles();

                MeshVertex A = mesh->vertices[triangles[triIndex].vertices[0]];
                MeshVertex B = mesh->vertices[triangles[triIndex].vertices[1]];
                MeshVertex C = mesh->vertices[triangles[triIndex].vertices[2]];

                // result.x = beta, result.y = gamma, result.z = t
                Vector3 result = getResultTriangleIntersection(rr, A.position, B.position, C.position);

                if (result.z < tt0 || result.z > tt1) {
                    return false;
                }

                if (result.y < 0 || result.y > 1) {
                    return false;
                }

                if (result.x < 0 || result.x > 1 - result.y) {
                    return false;
                }

                // TODO: useless of tt for packetized ray tracing
                tt = result.z;
                rr.maxt = tt;
                return true;
            };

            // start tracing the tree and get ray packet index list
            bvhTree->getRayPacketIntersectIndexList(localRays, indexList, segMask, t0, t1, rayTriangleIntersectionTest);

            // use indexList for actual generating hitInfo
            // TODO: change hitrecord to mark down geometry/primitive index than to actually store the value
            for (size_t i = 0; i < indexList.size(); i++) {
                auto idx = indexList[i];
                if (idx != -1)
                {
                    auto & ray = rays[i];
                    auto & r = localRays[i];
                    auto & rec = hitInfo[i];

                    MeshTriangle const *triangles = mesh->get_triangles();
                    MeshVertex A = mesh->vertices[triangles[idx].vertices[0]];
                    MeshVertex B = mesh->vertices[triangles[idx].vertices[1]];
                    MeshVertex C = mesh->vertices[triangles[idx].vertices[2]];

                    // result.x = beta, result.y = gamma, result.z = t
                    Vector3 result = getResultTriangleIntersection(r, A.position, B.position, C.position);

                    // TODO: for transparent objects
                    // if the hit record has alreayd hit on a point, judge if the t is smaller
                    if (rec.isHit && result.z > rec.t) {
                        continue;
                    }

                    // TODO: make hit record store the geom/primitive index, this is too expensive to compute onsite
                    rec.position = ray.e + result.z * ray.d;

                    real_t beta = result.x;
                    real_t gamma = result.y;
                    real_t alpha = 1 - beta - gamma;

                    rec.normal = normalize(alpha * (normMat * A.normal) + beta * (normMat * B.normal) + gamma * (normMat * C.normal));

                    // For texture mapping adjustment
                    A.tex_coord = getAdjustTexCoord(A.tex_coord);
                    B.tex_coord = getAdjustTexCoord(B.tex_coord);
                    C.tex_coord = getAdjustTexCoord(C.tex_coord);

                    Vector2 tex_cood_interpolated =
                    alpha * A.tex_coord + beta * B.tex_coord + gamma * C.tex_coord;

                    int width = 0, height = 0;
                    if (material) {
                        material->get_texture_size(&width, &height);
                    }

                    rec.diffuse = material->diffuse;
                    rec.ambient = material->ambient;
                    rec.specular = material->specular;
                    rec.phong = material->phong;

                    rec.texture = material->get_texture_pixel(tex_cood_interpolated.x * width, tex_cood_interpolated.y * height);

                    rec.isHit = true;
                    rec.t = result.z;

                    rec.refractive_index = material->refractive_index;
                }
            }
        }
    }

    bool Model::hit(Ray ray, real_t t0, real_t t1, HitRecord &rec) const
    {
        Ray r = Ray(matWorldToLocal.transform_point(ray.e), matWorldToLocal.transform_vector(ray.d));

        auto rayTriangleIntersectionTest = [this](Ray rr, real_t tt0, real_t tt1, real_t &tt, INT64 triIndex){
            MeshTriangle const *triangles = mesh->get_triangles();

            MeshVertex A = mesh->vertices[triangles[triIndex].vertices[0]];
            MeshVertex B = mesh->vertices[triangles[triIndex].vertices[1]];
            MeshVertex C = mesh->vertices[triangles[triIndex].vertices[2]];

            // result.x = beta, result.y = gamma, result.z = t
            Vector3 result = getResultTriangleIntersection(rr, A.position, B.position, C.position);

            if (result.z < tt0 || result.z > tt1) {
                return false;
            }

            if (result.y < 0 || result.y > 1) {
                return false;
            }

            if (result.x < 0 || result.x > 1 - result.y) {
                return false;
            }

            tt = result.z;
            return true;
        };

        INT64 idx = -1;

        if (bvhTree->getFirstIntersectIndex(r, t0, t1, idx, rayTriangleIntersectionTest)) {

            MeshTriangle const *triangles = mesh->get_triangles();
            MeshVertex A = mesh->vertices[triangles[idx].vertices[0]];
            MeshVertex B = mesh->vertices[triangles[idx].vertices[1]];
            MeshVertex C = mesh->vertices[triangles[idx].vertices[2]];

            // result.x = beta, result.y = gamma, result.z = t
            Vector3 result = getResultTriangleIntersection(r, A.position, B.position, C.position);

            rec.position = ray.e + result.z * ray.d;

            real_t beta = result.x;
            real_t gamma = result.y;
            real_t alpha = 1 - beta - gamma;

            rec.normal = normalize(alpha * (normMat * A.normal) + beta * (normMat * B.normal) + gamma * (normMat * C.normal));

            // For texture mapping adjustment
            A.tex_coord = getAdjustTexCoord(A.tex_coord);
            B.tex_coord = getAdjustTexCoord(B.tex_coord);
            C.tex_coord = getAdjustTexCoord(C.tex_coord);

            Vector2 tex_cood_interpolated =
            alpha * A.tex_coord + beta * B.tex_coord + gamma * C.tex_coord;

            int width = 0, height = 0;
            if (material) {
                material->get_texture_size(&width, &height);
            }

            rec.diffuse = material->diffuse;
            rec.ambient = material->ambient;
            rec.specular = material->specular;
            rec.phong = material->phong;

            rec.texture = material->get_texture_pixel(tex_cood_interpolated.x * width, tex_cood_interpolated.y * height);

            rec.t = result.z;

            rec.refractive_index = material->refractive_index;

            return true;
        }

        return false;

    }

} /* _462 */
