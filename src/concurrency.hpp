#pragma once

#include <vector>
#include <mutex>
#include <thread>
#include <queue>
#include <functional>

class Task
{
public:
  // executes some part of the task, returns true if the task is done
  virtual void execute() = 0;

  virtual bool has_work() = 0;

  // blocks until the task is done, while participating in the execution if
  // there is work to be done
  virtual void wait_and_execute() = 0;
};

class RepeatedTask : public Task
{
public:
  RepeatedTask(size_t n_repetitions, std::function<void(size_t)> f);

  void execute() override;
  bool has_work() override;
  void wait_and_execute() override;

private:
  std::atomic<size_t> _n_started_executions;
  std::atomic<size_t> _n_ended_executions;
  size_t _n_repetitions;
  std::function<void(size_t)> _function;
};

class ThreadPoll
{
public:
  ~ThreadPoll();

  static void init_global_thread_pool(size_t thread_count);
  static ThreadPoll *get_global_thread_pool();

  void add(std::shared_ptr<Task> task);

private:
  static std::unique_ptr<ThreadPoll> global_thread_pool;

  ThreadPoll(size_t n);

  std::atomic<bool> _done = false;
  std::vector<std::thread> _threads;
  mutable std::mutex _queue_lock;
  std::condition_variable _cv;
  std::queue<std::shared_ptr<Task>> _job_queue;
};
