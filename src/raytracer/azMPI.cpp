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
                
                MPICommunicate::ISendRay(&r, dest);
                send_count++;
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
                    if (recv_r.isHit)
                    {
                        Ray *shadowray = new Ray();
                        raytracer.generateShadowRay(recv_r, *shadowray);
                        MPICommunicate::ISendRay(shadowray,
                                                 raytracer.checkNextBoundingBox(*shadowray, procId));
                    }
                    send_count++;
                }
                    break;
                case ERayType_Shadow:
                    raytracer.updateFrameBuffer(recv_r);
                    break;
                case ERayType_GI:
                    break;
                case ERayType_Terminate:
                default:
                    printf("Proc = %d, Master received a wrong ray type = %x\n", procId, recv_r.type);
                    exit(-1);
            }
            
            printf("send_count = %d, recv_count = %d\n", send_count, recv_count);
            if (send_count == recv_count)
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
            printf("received something \n");
            switch(recv_r.type)
            {
                case ERayType_Eye:
                case ERayType_Shadow:
                {
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
