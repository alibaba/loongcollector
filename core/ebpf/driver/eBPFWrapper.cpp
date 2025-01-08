
extern "C" {
#include <coolbpf/coolbpf.h>
#include <coolbpf/bpf/libbpf.h>
};

#include "eBPFWrapper.h"

#include <iostream>
#include <atomic>

namespace logtail {
namespace ebpf {

void* PerfThreadWoker(void *ctx, const std::string& name, std::atomic<bool>& flag) {
  int err;
  struct perf_buffer *pb = NULL;
  struct perf_buffer_opts pb_opts = {};
  struct perf_thread_arguments *args = (struct perf_thread_arguments *)ctx;
  int timeout_ms = args->timeout_ms == 0 ? 100 : args->timeout_ms;

  pb_opts.sample_cb = args->sample_cb;
  pb_opts.ctx = args->ctx;
  pb_opts.lost_cb = args->lost_cb;
  pb = perf_buffer__new(args->mapfd, args->pg_cnt == 0 ? 128 : args->pg_cnt, &pb_opts);
  free(args);

  err = libbpf_get_error(pb);
  if (err)
  {
      std::cout << "error new perf buffer: " << strerror(-err) << std::endl;
      return nullptr;
  }

  if (!pb)
  {
      err = -errno;
      std::cout << "failed to open perf buffer: " << err << std::endl;
      return nullptr;
  }
  std::cout << "successfully new perf buffer:" << name << std::endl;

  while (flag) {
    std::cout << "begin poll perf buffer " << name << std::endl;
      err = perf_buffer__poll(pb, timeout_ms);
      if (err < 0 && err != -EINTR)
      {
          std::cout << "error polling perf buffer: " << strerror(-err) << std::endl;
          goto cleanup;
      }

      if (err == -EINTR)
          goto cleanup;
      /* reset err to return 0 if exiting */
      err = 0;
  }
cleanup:
  std::cout << "exit perf worker for " << name << std::endl;
  perf_buffer__free(pb);
  return nullptr;
}
}
}
