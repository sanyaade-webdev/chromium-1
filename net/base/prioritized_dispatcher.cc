// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/prioritized_dispatcher.h"

#include "base/logging.h"

namespace net {

PrioritizedDispatcher::Limits::Limits(Priority num_priorities,
                                      size_t total_jobs)
    : total_jobs(total_jobs), reserved_slots(num_priorities) {}

PrioritizedDispatcher::Limits::~Limits() {}

PrioritizedDispatcher::PrioritizedDispatcher(const Limits& limits)
    : queue_(limits.reserved_slots.size()),
      max_running_jobs_(limits.reserved_slots.size()),
      num_running_jobs_(0) {
  size_t total = 0;
  for (size_t i = limits.reserved_slots.size(); i > 0; --i) {
    total += limits.reserved_slots[i - 1];
    max_running_jobs_[i - 1] = total;
  }
  // Unreserved slots are available for all priorities.
  DCHECK_LE(total, limits.total_jobs) << "sum(reserved_slots) <= total_jobs";
  size_t spare = limits.total_jobs - total;
  for (size_t i = 0; i < max_running_jobs_.size(); ++i) {
    max_running_jobs_[i] += spare;
  }
}

PrioritizedDispatcher::~PrioritizedDispatcher() {}

PrioritizedDispatcher::Handle PrioritizedDispatcher::Add(
    Job* job, Priority priority) {
  DCHECK(job);
  DCHECK_LT(priority, num_priorities());
  if (num_running_jobs_ < max_running_jobs_[priority]) {
    ++num_running_jobs_;
    job->Start();
    return Handle();
  }
  return queue_.Insert(job, priority);
}

void PrioritizedDispatcher::Cancel(const Handle& handle) {
  queue_.Erase(handle);
}

PrioritizedDispatcher::Job* PrioritizedDispatcher::EvictOldestLowest() {
  Handle handle = queue_.FirstMax();
  if (handle.is_null())
    return NULL;
  Job* job = handle.value();
  Cancel(handle);
  return job;
}

PrioritizedDispatcher::Handle PrioritizedDispatcher::ChangePriority(
    const Handle& handle, Priority priority) {
  DCHECK(!handle.is_null());
  DCHECK_LT(priority, num_priorities());
  DCHECK_GE(num_running_jobs_, max_running_jobs_[handle.priority()]) <<
      "Job should not be in queue when limits permit it to start.";

  if (handle.priority() == priority)
    return handle;

  if (MaybeDispatchJob(handle, priority))
    return Handle();
  Job* job = handle.value();
  queue_.Erase(handle);
  return queue_.Insert(job, priority);
}

void PrioritizedDispatcher::OnJobFinished() {
  DCHECK_GT(num_running_jobs_, 0u);
  --num_running_jobs_;
  Handle handle = queue_.FirstMin();
  if (handle.is_null()) {
    DCHECK_EQ(0u, queue_.size());
    return;
  }
  MaybeDispatchJob(handle, handle.priority());
}

bool PrioritizedDispatcher::MaybeDispatchJob(const Handle& handle,
                                             Priority job_priority) {
  DCHECK_LT(job_priority, num_priorities());
  if (num_running_jobs_ >= max_running_jobs_[job_priority])
    return false;
  Job* job = handle.value();
  queue_.Erase(handle);
  ++num_running_jobs_;
  job->Start();
  return true;
}

}  // namespace net


