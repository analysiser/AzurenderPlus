
#include "master.hpp"

namespace _462
{
  void Master::GenerateEyeRay(std::vector<Ray> &eyerays)
  {
    int wstep = width / scene->node_size;
    int hstep = height;
    real_t dx = real_t(1)/width;
    real_t dy = real_t(1)/height;

    for (int y = 0; y < height; y++) {
      for (int x = 0; x < width; x++) {
        real_t i = real_t(2)*(real_t(x)+random())*dx - real_t(1);
        real_t j = real_t(2)*(real_t(y)+random())*dy - real_t(1);

        Ray r = Ray(scene->camera.get_position(), Ray::get_pixel_dir(i, j));
        r.x = x;
        r.y = y;
        r.color = Color3::Black();
        r.depth = 2;
        r.source = procId;
        r.raytype = eRayType_EyeRay;

        eyerays.push_back(r);
      }
    }
  }

  void Master::ExchangeBoundingBox()
  {
    scene->nodeBndBox = new BndBox[procs];
    BndBox curNodeBndBox = BndBox(Vector3::Zero());

    // TODO: this is wrong
    size_t recv_size = sizeof(curNodeBndBox) * procs;

    int status = MPI_Allgather(
        NULL,
        0,
        MPI_BYTE,
        scene->nodeBndBox,
        recv_size,
        MPI_BYTE,
        0, // root
        MPI_COMM_WORLD);

    if (status != 0)
    {
      printf("MPI error: %d\n", status);
      throw exception();
    }
  }

  void Master::Run()
  {
    bool no_more_ray = false;

    while(!no_more_ray)
    {
      MPI_Request req;
      Ray ray;
      MPI_Irecv(&ray, sizeof(Ray), MPI_BYTE, MPI_ANY_SOURCE, 0, MPI_COMM_WORLD, &req);
      MPI_Wait(&req, MPI_STATUS_IGNORE);

      switch(ray.raytype)
      {
        case eRayType_EyeRay:
          break;
        case eRayType_EyeRayReply:
          break;
        case eRayType_ShadowRay:
          break;
        case eRayType_ShadowRayReply:
          break;
        case eRayType_DiffuseRay:
          break;
        case eRayType_DiffuseRayReply:
          break;
        default:
          // should not reach
          return;
      }
    }
  }
}  //namespace  _462
