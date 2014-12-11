#include "azMPI.hpp"
#include "MPICommunicate.hpp"

namespace _462 {

  void azMPI::mpiPathTrace(vector<Ray> &eyerays)
  {
    if (procId == 0)
    {
      //            std::vector<Ray> ray_list;
      //            raytracer.generateEyeRay(ray_list);
      //
      for (Ray &r : eyerays)
      {
        int dest = raytracer.checkNextBoundingBox(r, procId);
        if (dest == 0)
        {

          raytracer.updateFrameBuffer(r);

          continue;
        }

        wait_queue.push(r);
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

    for(int i = 0; i < 10; i++)
    {
      Ray *ray = new Ray(&(wait_queue.front()));
      wait_queue.pop();
      int dest = raytracer.checkNextBoundingBox(*ray, procId);
      MPICommunicate::ISendRay(ray, dest);
      send_count++;
    }

    while(1)
    {
      printf("recv_count %d send_count %d\n", recv_count, send_count);
      if (!wait_queue.empty())
      {
        Ray *ray = new Ray(&(wait_queue.front()));
        wait_queue.pop();
        int dest = raytracer.checkNextBoundingBox(*ray, procId);
        MPICommunicate::ISendRay(ray, dest);
        send_count++;
      }
      MPICommunicate::RecvRay(recv_r);
      recv_count++;
      switch(recv_r.type)
      {
        case ERayType_Eye:
          {
            raytracer.updateFrameBuffer(recv_r);
            if (recv_r.isHit)
            {
              Ray *shadowray = new Ray();
              raytracer.generateShadowRay(recv_r, *shadowray);
              MPICommunicate::ISendRay(shadowray,
                  raytracer.checkNextBoundingBox(*shadowray, procId));
              send_count++;
            }
          }
          break;
        case ERayType_Shadow:
          //printf("received some shadow ray back\n");
          raytracer.updateFrameBuffer(recv_r);
          break;
        case ERayType_GI:
          break;
        case ERayType_Terminate:
        default:
          printf("Proc = %d, Master received a wrong ray type = %x\n", procId, recv_r.type);
          exit(-1);
      }

      if (send_count == (recv_count+1))
      {
        Ray terminate;
        terminate.type = ERayType_Terminate;
        MPICommunicate::BroadCast(terminate);
        break;
      }
    }
  }

  void azMPI::run_slave()
  {
    bool finished = false;
    Ray recv_r;
    while(!finished)
    {
      MPICommunicate::RecvRay(recv_r);
      switch(recv_r.type)
      {
        case ERayType_Eye:
        case ERayType_Shadow:
          {
            //printf("rece %d\n", procId);
            int next_node;
            next_node = raytracer.localRaytrace(recv_r);
            Ray *send_ray_buf = new Ray(&recv_r);
            MPICommunicate::ISendRay(send_ray_buf, next_node);
          }
          break;
        case ERayType_GI:
          break;
        case ERayType_Terminate:
          finished = true;
          break;
        default:
          printf("Proc = %d, Slave received a wrong ray type = %d\n", procId, recv_r.type);
          exit(-1);
      }
    }
  }

}  //namespace _46
