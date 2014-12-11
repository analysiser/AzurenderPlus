#include "azMPI.hpp"
#include "MPICommunicate.hpp"

namespace _462 {

  void azMPI::mpiPathTrace()
  {
    if (procId == 0)
    {
      std::vector<Ray> ray_list;
      raytracer.generateEyeRay(ray_list);

      for (Ray &r: ray_list)
      {
        int dest = raytracer.checkNextBoundingBox(r, procId);
        MPICommunicate::ISendRay(&r, dest);
        sendcounts++;
      }

      run_master();
    }
    else
    {
      run_slave();
    }
    return;
  }

  void azMPI::run_master()
  {
    Ray recv_r;
    while(1)
    {
      MPICommunicate::RecvRay(recv_r);
      recv_count++;
      switch(recv_r.type)
      {
        case ERayType_Eye:
          {
          raytracer.updateFrameBuffer(recv_r);
          Ray *shadowray = new Ray();
          raytracer.generateShadowRay(recv_r, *shadowray);
          MPICommunicate::ISendRay(shadowray,
              raytracer.checkNextBoundingBox(*shadowray, procId));
          sendcounts++;
          }
          break;
        case ERayType_Shadow:
          raytracer.updateFrameBuffer(recv_r);
          break;
        case ERayType_GI:
          break;
        default:
          exit(-1);
      }

      if (sendcounts == recv_count)
      {
        break;
      }
    }
  }

  void azMPI::run_slave()
  {
    Ray recv_r;
    while(1)
    {
      MPICommunicate::RecvRay(recv_r);
      switch(recv_r.type)
      {
        case ERayType_Eye:
          {
          int next_node;
          next_node = raytracer.localRaytrace(recv_r);
          Ray *send_ray_buf = new Ray(&recv_r);
          MPICommunicate::ISendRay(send_ray_buf, next_node);
          }
          break;
        case ERayType_Shadow:
          break;
        case ERayType_GI:
          break;
        default:
          exit(-1);
      }
    }
  }

}