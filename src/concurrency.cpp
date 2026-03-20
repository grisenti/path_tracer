#include "concurrency.hpp"

#include "common.hpp"

RepeatedTask::RepeatedTask(size_t n_repetitions, std::function<void(size_t)> f)
    : _n_started_executions(0), _n_repetitions(n_repetitions),
      _function(std::move(f))
{
}

void RepeatedTask::execute()
{
  auto const value = _n_started_executions++;
  if (value < _n_repetitions)
  {
    _function(value);
    if (++_n_ended_executions == _n_repetitions)
      _done_cv.notify_all();
  }
}

bool RepeatedTask::has_work()
{
  return _n_started_executions < _n_repetitions;
}

void RepeatedTask::wait_and_execute()
{
  while (has_work())
    execute();
  std::unique_lock lock(_done_mutex);
  _done_cv.wait(
      lock, [this] { return _n_ended_executions >= _n_repetitions; });
}

std::unique_ptr<ThreadPoll> ThreadPoll::global_thread_pool;

ThreadPoll::~ThreadPoll()
{
  _done = true;
  _cv.notify_all();
  for (auto &t : _threads)
    t.join();
}

void ThreadPoll::init_global_thread_pool(size_t thread_count)
{
  PT_ASSERT(!global_thread_pool);
  // can't use make_unique since the constructor is private
  global_thread_pool
      = std::unique_ptr<ThreadPoll>(new ThreadPoll(thread_count));
}

ThreadPoll *ThreadPoll::get_global_thread_pool()
{
  PT_ASSERT(global_thread_pool);
  return global_thread_pool.get();
}

void ThreadPoll::add(std::shared_ptr<Task> task)
{
  {
    std::lock_guard guard(_queue_lock);
    _job_queue.emplace(std::move(task));
  }
  _cv.notify_all();
}

ThreadPoll::ThreadPoll(size_t n)
{
  _done = false;
  _threads.reserve(n);
  for (size_t i = 0; i < n; ++i)
  {
    _threads.emplace_back(
        [this]
        {
          while (!_done)
          {
            std::unique_lock lock(_queue_lock);
            _cv.wait(lock, [this] { return _done || !_job_queue.empty(); });
            if (!_job_queue.empty())
            {
              auto top = _job_queue.front();
              if (top->has_work())
              {
                lock.unlock();
                top->execute();
              }
              else
              {
                _job_queue.pop();
              }
            }
          }
        });
  }
}