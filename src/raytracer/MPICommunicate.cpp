#include "MPICommunicate.hpp"


namespace _462
{
  void MPICommunicate::SendRay(Ray &ray, int dest)
  {
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
    MPI_Request req;
    MPI_Isend(
        ray,
        sizeof(Ray),
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


  void MPICommunicate::BroadCast(Ray &ray)
  {
    MPI_Bcast(
        &ray,
        sizeof(ray),
        MPI_BYTE,
        0,
        MPI_COMM_WORLD);
    return;
  }

}  // namespace _462
