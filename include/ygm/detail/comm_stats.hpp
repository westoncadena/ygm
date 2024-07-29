// Copyright 2019-2021 Lawrence Livermore National Security, LLC and other YGM
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: MIT

#pragma once

#include <mpi.h>

#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

#include <iostream>
#include <string>

namespace ygm {
namespace detail {

struct shared_stats {
  size_t m_rank;

  size_t m_async_count;
  size_t m_barrier_count;
  size_t m_rpc_count;
  size_t m_route_count;

  size_t m_isend_count;
  size_t m_isend_bytes;
  size_t m_isend_test_count;

  size_t m_irecv_count;
  size_t m_irecv_bytes;
  size_t m_irecv_test_count;

  double m_waitsome_isend_irecv_time;
  size_t m_waitsome_isend_irecv_count;

  size_t m_iallreduce_count;
  double m_waitsome_iallreduce_time;
  size_t m_waitsome_iallreduce_count;

  double m_time_start;
};

class comm_stats {
 public:
  class timer {
   public:
    timer(double& _timer) : m_timer(_timer), m_start_time(MPI_Wtime()) {}

    ~timer() { m_timer += (MPI_Wtime() - m_start_time); }

   private:
    double& m_timer;
    double  m_start_time;
  };

  comm_stats()
      : fd(-1), stats_path(""), stats(nullptr), m_time_start(MPI_Wtime()) {}

  ~comm_stats() {
    if (stats != nullptr) {
      if (munmap(stats, sizeof(struct shared_stats)) == -1) {
        std::perror("munmap failed");
      }
    }

    if (fd != -1) {
      if (close(fd) == -1) {
        std::perror("close failed");
      }

      int result = shm_unlink(stats_path.c_str());
      if (result == -1) {
        std::perror("failed to unlink stats shared memory");
      }
    }

    shm_unlink(local_ranks_size_path.c_str());
    shm_unlink(local_ranks_path.c_str());
  }

  void setup(int rank) {
    stats_path = "stats" + std::to_string(rank);
    fd         = shm_open(stats_path.c_str(), O_CREAT | O_TRUNC | O_RDWR, 0600);
    if (fd == -1) {
      std::cerr << "Stats_path = " << stats_path << " File Discriptor = " << fd
                << std::endl;
    }

    if (ftruncate(fd, sizeof(struct shared_stats)) == -1)
      std::cerr << "Failed to ftruncate shared memory" << std::endl;
    ;

    /* Map the object into the caller's address space. */

    stats = static_cast<shared_stats*>(
        mmap(NULL, sizeof(*stats), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0));
    if (stats == MAP_FAILED) std::cerr << "Failed to mmap" << std::endl;

    reset();
    stats->m_rank       = rank;
    stats->m_time_start = m_time_start;
  }

  void shm_local_ranks(int size, std::vector<int>& local_ranks) {
    int fd_size = shm_open(local_ranks_size_path.c_str(),
                           O_CREAT | O_TRUNC | O_RDWR, 0600);
    if (fd_size == -1) {
      std::cerr << "local_ranks_size_path = " << local_ranks_size_path
                << " File Discriptor = " << fd_size << std::endl;
    }

    if (ftruncate(fd_size, sizeof(int)) == -1)
      std::cerr << "Failed to ftruncate shared memory" << std::endl;

    int* shm_size = static_cast<int*>(mmap(
        NULL, sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED, fd_size, 0));
    if (shm_size == MAP_FAILED) std::cerr << "Failed to mmap" << std::endl;

    *shm_size = size;

    // put the array in memory
    int fd_arr =
        shm_open(local_ranks_path.c_str(), O_CREAT | O_TRUNC | O_RDWR, 0600);
    if (fd_arr == -1) {
      std::cerr << "local_ranks_path = " << local_ranks_path
                << " File Discriptor = " << fd_arr << std::endl;
    }

    if (ftruncate(fd_arr, size * sizeof(int)) == -1)
      std::cerr << "Failed to ftruncate shared memory" << std::endl;

    int* shm_arr =
        static_cast<int*>(mmap(NULL, size * sizeof(int), PROT_READ | PROT_WRITE,
                               MAP_SHARED, fd_arr, 0));
    if (shm_arr == MAP_FAILED) std::cerr << "Failed to mmap" << std::endl;

    std::copy(local_ranks.begin(), local_ranks.end(), shm_arr);

    if (close(fd_size) == -1) {
      std::perror("close failed");
    }

    if (close(fd_arr) == -1) {
      std::perror("close failed");
    }
  }

  void isend(int dest, size_t bytes) {
    stats->m_isend_count += 1;
    stats->m_isend_bytes += bytes;
  }

  void irecv(int source, size_t bytes) {
    stats->m_irecv_count += 1;
    stats->m_irecv_bytes += bytes;
  }

  void async(int dest) { stats->m_async_count += 1; }

  void barrier() { stats->m_barrier_count += 1; }

  void rpc_execute() { stats->m_rpc_count += 1; }

  void routing() { stats->m_route_count += 1; }

  void isend_test() { stats->m_isend_test_count += 1; }

  void irecv_test() { stats->m_irecv_test_count += 1; }

  void iallreduce() { stats->m_iallreduce_count += 1; }

  timer waitsome_isend_irecv() {
    stats->m_waitsome_isend_irecv_count += 1;
    return timer(stats->m_waitsome_isend_irecv_time);
  }

  timer waitsome_iallreduce() {
    stats->m_waitsome_iallreduce_count += 1;
    return timer(stats->m_waitsome_iallreduce_time);
  }

  void reset() {
    stats->m_rank                       = -1;
    stats->m_async_count                = 0;
    stats->m_barrier_count              = 0;
    stats->m_rpc_count                  = 0;
    stats->m_route_count                = 0;
    stats->m_isend_count                = 0;
    stats->m_isend_bytes                = 0;
    stats->m_isend_test_count           = 0;
    stats->m_irecv_count                = 0;
    stats->m_irecv_bytes                = 0;
    stats->m_irecv_test_count           = 0;
    stats->m_waitsome_isend_irecv_time  = 0.0f;
    stats->m_waitsome_isend_irecv_count = 0.0f;
    stats->m_iallreduce_count           = 0;
    stats->m_waitsome_iallreduce_time   = 0.0f;
    stats->m_waitsome_iallreduce_count  = 0;
    stats->m_time_start                 = MPI_Wtime();
  }

  size_t get_async_count() const { return stats->m_async_count; }
  size_t get_rpc_count() const { return stats->m_rpc_count; }
  size_t get_route_count() const { return stats->m_route_count; }

  size_t get_isend_count() const { return stats->m_isend_count; }
  size_t get_isend_bytes() const { return stats->m_isend_bytes; }
  size_t get_isend_test_count() const { return stats->m_isend_test_count; }

  size_t get_irecv_count() const { return stats->m_irecv_count; }
  size_t get_irecv_bytes() const { return stats->m_irecv_bytes; }
  size_t get_irecv_test_count() const { return stats->m_irecv_test_count; }

  double get_waitsome_isend_irecv_time() const {
    return stats->m_waitsome_isend_irecv_time;
  }
  size_t get_waitsome_isend_irecv_count() const {
    return stats->m_waitsome_isend_irecv_count;
  }

  size_t get_iallreduce_count() const { return stats->m_iallreduce_count; }
  double get_waitsome_iallreduce_time() const {
    return stats->m_waitsome_iallreduce_time;
  }
  size_t get_waitsome_iallreduce_count() const {
    return stats->m_waitsome_iallreduce_count;
  }

  double get_elapsed_time() const { return MPI_Wtime() - stats->m_time_start; }

 private:
  int                  fd;
  std::string          stats_path;
  struct shared_stats* stats;

  double m_time_start;

  std::string local_ranks_size_path = "local_ranks_size";
  std::string local_ranks_path      = "local_ranks";
};
}  // namespace detail
}  // namespace ygm
