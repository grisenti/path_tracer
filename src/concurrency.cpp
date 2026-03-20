#include "concurrency.hpp"

#include "common.hpp"

SingleJobTask::SingleJobTask(std::function<void()> f)
    : _job_started(false), _end_latch(1), _function(std::move(f))
{
}

void SingleJobTask::execute()
{
  auto expected = false;
  if (_job_started.compare_exchange_strong(
          expected, true, std::memory_order_release))
  {
    _function();
    _end_latch.count_down();
  }
}

bool SingleJobTask::has_work()
{
  return !_job_started.load(std::memory_order_acquire);
}

void SingleJobTask::wait_and_execute()
{
  execute();
  _end_latch.wait();
}

RepeatedTask::RepeatedTask(size_t n_repetitions, std::function<void(size_t)> f)
    : _n_started_executions(0), _n_repetitions(n_repetitions),
      _end_latch(n_repetitions), _function(std::move(f))
{
}

void RepeatedTask::execute()
{
  auto const value = _n_started_executions++;
  if (value < _n_repetitions)
  {
    _function(value);
    _end_latch.count_down();
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
  _end_latch.wait();
}

std::unique_ptr<ThreadPoll> ThreadPoll::global_thread_pool;

ThreadPoll::~ThreadPoll()
{
  {
    std::lock_guard guard(_queue_lock);
    _done = true;
  }
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
    _task_queue.emplace(std::move(task));
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
            _cv.wait(lock, [this] { return _done || !_task_queue.empty(); });
            if (!_task_queue.empty())
            {
              auto top = _task_queue.front();
              if (top->has_work())
              {
                lock.unlock();
                top->execute();
              }
              else
              {
                _task_queue.pop();
              }
            }
          }
        });
  }
}