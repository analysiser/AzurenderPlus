#include "azMPI.hpp"
#include "MPICommunicate.hpp"

namespace _462 {
    
    void azMPI::mpiPathTrace(vector<Ray> &eyerays)
    {
        if (procId == 0)
        {
            for (Ray &r : eyerays)
            {
                int dest = raytracer.checkNextBoundingBox(r, procId);
                if (dest == 0)
                {
                    raytracer.updateFrameBuffer(r);
                    continue;
                }
                result_list.push_back(dest, r);
            }
            
        }
        run();
        return;
    }
    
    void azMPI::run()
    {
        bool finish = false;
        while(!finish)
        {
            if (procId == 0)
            {
                master_process_ray();
            }
            else
            {
                slave_process_ray();
            }
            exchange_ray();
            finish = exchange_workload();
        }
    }
    
    
    void azMPI::master_process_ray()
    {
        while(!wait_queue.empty())
        {
            Ray &recv_r = wait_queue.front();
            switch(recv_r.type)
            {
                case ERayType_Eye:
                    raytracer.updateFrameBuffer(recv_r);
                    break;
                case ERayType_Shadow:
                    raytracer.updateFrameBuffer(recv_r);
                    break;
                case ERayType_GIShadow:
                    raytracer.updateFrameBuffer(recv_r);
                    break;
                case ERayType_GI:
                    // You didnt hit anything
//                    printf("Shouldn't receive ERayType_GI\n");
//                    exit(10);
                    break;
                default:
                    exit(-1);
            }
            wait_queue.pop();
        }
    }
    
    void azMPI::slave_process_ray()
    {
        while(!wait_queue.empty())
        {
            Ray &recv_r = wait_queue.front();
            switch(recv_r.type)
            {
                case ERayType_Eye:
                {
                    int next_node;
                    next_node = raytracer.localRaytrace(recv_r);
                    if (next_node == root && recv_r.isHit)
                    {
                        // generate shadow ray
                        // check bounding box
                        // if self add wait queue
                        // else push result list
                        Ray shadowRay;
                        raytracer.generateShadowRay(recv_r, shadowRay);
                        
                        int next_node = raytracer.checkNextBoundingBox(shadowRay, root);
                        if (next_node == procId) {
//                            shadowRay.source = next_node;
                            wait_queue.push(shadowRay);
                        }
                        else {
//                            shadowRay.source = next_node;
                            result_list.push_back(next_node, shadowRay);
                        }
                        
                        if (recv_r.depth > 0)
                        {
                            for (int i = 0; i < 10; i++) {
                                Ray giRay;
                                raytracer.generateGIRay(recv_r, giRay);
                                giRay.color *= 0.1f;
                                
                                next_node = raytracer.checkNextBoundingBox(giRay, root);
                                if (next_node == procId) {
                                    wait_queue.push(giRay);
                                }
                                else {
                                    result_list.push_back(next_node, giRay);
                                }
                            }
                        }
                        
                    }
                    else {
                        result_list.push_back(next_node, recv_r);
                    }
                }
                    break;
                case ERayType_Shadow:
                case ERayType_GIShadow:
                {
                    int next_node;
                    next_node = raytracer.localRaytrace(recv_r);
                    result_list.push_back(next_node, recv_r);
                }
                    break;
                case ERayType_GI:
                {
                    int next_node;
                    next_node = raytracer.localRaytrace(recv_r);
                    // finished getting GI ray hit
                    if (next_node == root && recv_r.isHit)
                    {
                        // generate shadow ray
                        // check bounding box
                        // if self add wait queue
                        // else push result list
                        Ray shadowRay;
                        raytracer.generateShadowRay(recv_r, shadowRay);
                        
                        int next_node = raytracer.checkNextBoundingBox(shadowRay, root);
                        if (next_node == procId) {
                            wait_queue.push(shadowRay);
                        }
                        else {
                            result_list.push_back(next_node, shadowRay);
                        }
                        
//                        if (recv_r.depth > 0)
//                        {
//                            Ray giRay;
//                            raytracer.generateGIRay(recv_r, giRay);
//                            
//                            next_node = raytracer.checkNextBoundingBox(giRay, root);
//                            if (next_node == procId) {
//                                wait_queue.push(giRay);
//                            }
//                            else {
//                                result_list.push_back(next_node, giRay);
//                            }
//                        }
                        
                    }
                    else {
                        result_list.push_back(next_node, recv_r);
                    }
                }
                    break;
                case ERayType_Terminate:
                    break;
                default:
                    exit(-23);
            }
            wait_queue.pop();
        }
    }
    
    void azMPI::exchange_ray()
    {
        Ray *sendbuf;
        int *sendcounts;
        int *sendoffsets;
        // generate data to send
        result_list.mpi_datagen(&sendbuf, &sendoffsets, &sendcounts);
        
        // but first we need to send how many data each node are going to receive
        int status = -1;
        int *recvcounts = new int[procs];
        status = MPI_Alltoall(sendcounts, 1, MPI_INT, recvcounts, 1, MPI_INT, MPI_COMM_WORLD);
        if (status != 0) {
            printf("Fail to send and receive ray count info!\n");
            throw exception();
        }
        
        // Received ray buffer
        int *recvoffsets = new int[procs];
        int total = 0;
        for (int i = 0; i < procs; i++) {
            recvoffsets[i] = total;
            total += recvcounts[i];
        }
        Ray *recvbuf = new Ray[total];
        
        
        // reset counts, offsets for byte sized send/recv
        size_t raysize = sizeof(Ray);
        for (int i = 0; i < procs; i++) {
            sendcounts[i] *= raysize;
            sendoffsets[i] *= raysize;
            
            recvcounts[i] *= raysize;
            recvoffsets[i] *= raysize;
        }
        
        // send all rays by MPI alltoallv
        status = MPI_Alltoallv(sendbuf, sendcounts, sendoffsets, MPI_BYTE, recvbuf, recvcounts, recvoffsets, MPI_BYTE, MPI_COMM_WORLD);
        if (status != 0) {
            printf("Fail to send and receive rays!\n");
            throw exception();
        }
        
        std::vector<Ray> recvRayList(total);
        std::copy(recvbuf, recvbuf+total, recvRayList.begin());
        
        for(Ray &r: recvRayList)
        {
            wait_queue.push(r);
        }
        
        delete sendbuf;
        delete sendcounts;
        delete sendoffsets;
        
        delete recvbuf;
        delete recvcounts;
        delete recvoffsets;
        
        result_list.clean();
    }
    
    bool azMPI::exchange_workload()
    {
        int status = -1;
        std::vector<int> workload(procs, wait_queue.size());
        std::vector<int> recvcounts(procs, 0);
        status = MPI_Alltoall(workload.data(), 1, MPI_INT, recvcounts.data(), 1, MPI_INT, MPI_COMM_WORLD);
        if (status != 0) {
            printf("Fail to send and receive ray count info!\n");
            throw exception();
        }
        
        int total_workload = wait_queue.size();
        for(int &load : recvcounts)
        {
            total_workload += load;
        }
        
        return (total_workload == 0);
    }
    
}  //namespace _46
