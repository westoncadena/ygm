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

  comm_stats() : fd(-1), stats_path(""), stats(nullptr) {}

  comm_stats(int rank) {
    std::cout << "HI" << std::endl;
    stats_path = "trace" + std::to_string(rank);
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
    stats->m_rank = rank;
  }

  ~comm_stats() {
    int result = shm_unlink(stats_path.c_str());
    // if (result == -1) {
    //   std::cerr << "Failed to unlink shared memory object: " << stats_path
    //             << ", result = " << result << std::endl;
    // }
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
};
}  // namespace detail
}  // namespace ygm
