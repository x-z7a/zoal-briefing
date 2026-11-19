#ifndef ZOAL_BRIEFING_MAIN_THREAD_QUEUE_HPP
#define ZOAL_BRIEFING_MAIN_THREAD_QUEUE_HPP

#include <functional>
#include <mutex>
#include <utility>
#include <vector>

namespace zoal_briefing {

// Work that must run on X-Plane's main thread (every XPLM call), posted from
// any thread and run by the flight loop with drain().
class MainThreadQueue {
public:
  using Task = std::function<void()>;

  void post(Task task) {
    std::lock_guard<std::mutex> lock(mutex_);
    tasks_.push_back(std::move(task));
  }

  // Runs everything posted so far. Tasks posted while draining run next time.
  void drain() {
    std::vector<Task> tasks;
    {
      std::lock_guard<std::mutex> lock(mutex_);
      tasks.swap(tasks_);
    }
    for (auto &task : tasks) {
      task();
    }
  }

  void clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    tasks_.clear();
  }

private:
  std::mutex mutex_;
  std::vector<Task> tasks_;
};

} // namespace zoal_briefing

#endif // ZOAL_BRIEFING_MAIN_THREAD_QUEUE_HPP
