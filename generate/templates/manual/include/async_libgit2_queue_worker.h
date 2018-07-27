#ifndef ASYNC_LIBGIT2_QUEUE_WORKER_H
#define ASYNC_LIBGIT2_QUEUE_WORKER_H

#include <napi.h>
#include <uv.h>
#include <uv.h>
#include "../include/thread_pool.h"
#include "../include/nodegit.h"


// Runs WorkComplete of the scheduled AsyncWorker,
// and destroys it.  This is run in the uv_default_loop event loop.
inline void AsyncLibgit2Complete (void* data) {
  Napi::AsyncWorker *worker = static_cast<Napi::AsyncWorker*>(data);
  worker->WorkComplete();
  worker->Destroy();
}

// Runs Execute of the scheduled AyncWorker on the dedicated libgit2 thread /
// event loop, and schedules the WorkComplete callback to run on the
// uv_default_loop event loop
inline void AsyncLibgit2Execute (void *vworker) {
  // execute the worker
  Napi::AsyncWorker *worker = static_cast<Napi::AsyncWorker*>(vworker);
  worker->Execute();
}

// Schedules the AsyncWorker to run on the dedicated libgit2 thread / event loop,
// and on completion AsyncLibgit2Complete on the default loop
inline void AsyncLibgit2QueueWorker (Napi::AsyncWorker* worker) {
  libgit2ThreadPool.QueueWork(AsyncLibgit2Execute, AsyncLibgit2Complete, worker);
}

#endif
