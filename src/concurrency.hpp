#pragma once

#include "common.hpp"

#include <condition_variable>
#include <vector>
#include <mutex>
#include <thread>
#include <queue>
#include <functional>
#include <latch>
#include <memory>

// interface for a set of jobs that can be executed independently
class Task
{
public:
  virtual ~Task() = default;

  // executes a job
  virtual void execute() = 0;

  virtual bool has_work() = 0;

  // executes remaining jobs and waits for all jobs to be completed
  virtual void execute_remaining_and_wait() = 0;
};

// single job task
class SingleJob : public Task
{
public:
  // not supposed to be used by itself, rather call RepeatedJob::task
  // TODO: maybe should consider making this private
  static std::shared_ptr<Task> task(std::function<void()> f)
  {
    return std::make_shared<SingleJob>(std::move(f));
  }

  SingleJob(std::function<void()> f);

  void execute() override;
  bool has_work() override;
  void execute_remaining_and_wait() override;

private:
  std::atomic<bool> _job_started;
  std::latch _end_latch;
  std::function<void()> _function;
};

// execute the same job a given number of times
class RepeatedJob : public Task
{
public:
  // f should be thread-safe since it could be called by multiple threads
  // at the same time, although never with the same index
  static std::shared_ptr<Task> task(
      size_t n_repetitions, std::function<void(size_t)> f)
  {
    return std::make_shared<RepeatedJob>(n_repetitions, std::move(f));
  }

  // not supposed to be used by itself, rather call RepeatedJob::task
  // TODO: maybe should consider making this private
  RepeatedJob(size_t n_repetitions, std::function<void(size_t)> f);

  void execute() override;
  bool has_work() override;
  void execute_remaining_and_wait() override;

private:
  std::atomic<size_t> _n_started_executions;
  size_t _n_repetitions;
  std::latch _end_latch;
  std::function<void(size_t)> _function;
};

class JobList : public Task
{
public:
  static std::shared_ptr<Task> task(std::vector<std::function<void()>> fs)
  {
    return std::make_shared<JobList>(std::move(fs));
  }

  // not supposed to be used by itself, rather call RepeatedJob::task
  // TODO: maybe should consider making this private
  JobList(std::vector<std::function<void()>> fs);

  void execute() override;
  bool has_work() override;
  void execute_remaining_and_wait() override;

private:
  std::atomic<size_t> _last_started_job;
  std::latch _end_latch;
  std::vector<std::function<void()>> _functions;
};

class ThreadPool
{
public:
  ~ThreadPool();

  static void init_global_thread_pool(size_t thread_count);
  static ThreadPool *get_global_thread_pool();

  void add(std::shared_ptr<Task> task);

private:
  static std::unique_ptr<ThreadPool> global_thread_pool;

  ThreadPool(size_t n);

  std::atomic<bool> _done = false;
  std::vector<std::thread> _threads;
  std::mutex _queue_lock;
  std::condition_variable _cv;
  std::queue<std::shared_ptr<Task>> _task_queue;
};
