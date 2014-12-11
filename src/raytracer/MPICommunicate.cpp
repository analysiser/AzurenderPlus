#include "MPICommunicate.hpp"


namespace _462
{
    void MPICommunicate::SendRay(Ray &ray, int dest)
    {
        if (ray.type != 0 && ray.type != 1 && ray.type != 2 && ray.type != 3  )
            printf("Send Ray type = %d\n", ray.type);
        MPI_Send(
                 &ray,
                 sizeof(ray),
                 MPI_BYTE,
                 dest,
                 0,
                 MPI_COMM_WORLD);
        return;
    }
    
    void MPICommunicate::ISendRay(Ray *ray, int dest)
    {
        if (ray->type != 0 && ray->type != 1 && ray->type != 2 && ray->type != 3  )
            printf("ISendRay Ray type = %d\n", ray->type);
        MPI_Request req;
        MPI_Isend(
                  ray,
                  sizeof(ray),
                  MPI_BYTE,
                  dest,
                  0,
                  MPI_COMM_WORLD,
                  &req);
        MPI_Request_free(&req);
        return;
    }
    
    void MPICommunicate::RecvRay(Ray &ray)
    {
        if (ray.type != 0 && ray.type != 1 && ray.type != 2 && ray.type != 3  )
            printf("RecvRay Ray type = %d\n", ray.type);
        MPI_Recv(
                 &ray,
                 sizeof(ray),
                 MPI_BYTE,
                 MPI_ANY_SOURCE,
                 0,
                 MPI_COMM_WORLD,
                 NULL);
        return;
    }
}  // namespace _462
