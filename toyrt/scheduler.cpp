#include "scheduler.hpp"
#include "dependencies.hpp"

void EagerScheduler::clear() {
  std::lock_guard<std::mutex> guard(mutex);
  q.clear();
  taskCount = 0;
}

void EagerScheduler::push(TaskPtr task) {
  std::lock_guard<std::mutex> guard(mutex);
  q.push_back(task);
  taskCount++;
  if (recorder) recorder->record(taskCount);
}

bool EagerScheduler::tryPop(TaskPtr& task) {
  std::lock_guard<std::mutex> guard(mutex);
  if (!q.empty()) {
    task = q.front();
    q.pop_front();
    taskCount--;
    if (recorder) recorder->record(taskCount);
    return true;
  }
  return false;
}

void PriorityScheduler::clear() {
  std::lock_guard<std::mutex> guard(mutex);
  if (TaskScheduler::getInstance().verbose())
    printf("%s PriorityScheduler::clear\n",
           TaskScheduler::getInstance().getLocalization().c_str());
  for (int i = 0; i < PRIORITIES; i++) {
    q[i].clear();
  }
}

void PriorityScheduler::push(TaskPtr task) {
  std::lock_guard<std::mutex> guard(mutex);
  if (TaskScheduler::getInstance().verbose())
    printf("%s PriorityScheduler::push %s\n",
           TaskScheduler::getInstance().getLocalization().c_str(),
           task ? task->description().c_str() : "NULL");
  int priority = task ? task->priority : LOW;
  q[priority].push_back(task);
  taskCount++;
  if (recorder) recorder->record(taskCount);
};

bool PriorityScheduler::tryPop(TaskPtr& task) {
  std::lock_guard<std::mutex> guard(mutex);
  for (int i = 0; i < PRIORITIES; i++) {
    if (!q[i].empty()) {
      task = q[i].front();
      q[i].pop_front();
      taskCount--;
      if (recorder) recorder->record(taskCount);
      if (TaskScheduler::getInstance().verbose())
        printf("%s PriorityScheduler::tryPop %s\n",
               TaskScheduler::getInstance().getLocalization().c_str(),
               task ? task->description().c_str() : "NULL");
      return true;
    }
  }
  if (TaskScheduler::getInstance().verbose())
    printf("%s PriorityScheduler::tryPop failed\n",
           TaskScheduler::getInstance().getLocalization().c_str());
  return false;
}
