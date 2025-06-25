#pragma once

#include "task_graph.hh"
#include "WorkStealing.hh"

struct AdaptiveTaskGraph : public TaskGraph
{
    AdaptiveTaskGraph(size_t num_threads = thread::hardware_concurrency())
        : TaskGraph(num_threads) {}

    void execute()
    {
        const size_t dynamic_threads = thread::hardware_concurrency();
        WorkStealingThreadPool pool(dynamic_threads);

        // Store the remaining dependencies for each task
        unordered_map<Task<void> *, size_t> dependency_count;

        // Backup: each task will have a list of tasks that depend on it
        unordered_map<Task<void> *, vector<Task<void> *>> reverse_dependencies;

        queue<Task<void> *> ready_tasks;

        // Initialize dependency_count and reverse_dependencies
        for (auto &task_ptr : tasks)
        {
            Task<void> *task = task_ptr.get();
            size_t count = task->dependencies.size();
            dependency_count[task] = count;

            // If no dependencies, ready to run
            if (count == 0)
                ready_tasks.push(task);

            // Create reverse mapping
            for (Task<void> *dep : task->dependencies)
            {
                reverse_dependencies[dep].push_back(task);
            }
        }

        // Start processing ready tasks
        while (!ready_tasks.empty())
        {
            Task<void> *task = ready_tasks.front();
            ready_tasks.pop();

            // Move task into pool for execution
            pool.enqueue(std::move(*task));

            // Find tasks that depend on current task
            for (Task<void> *dependent_task : reverse_dependencies[task])
            {
                if (--dependency_count[dependent_task] == 0)
                {
                    ready_tasks.push(dependent_task);
                }
            }
        }

        wait_all(); // Wait for all tasks to complete before the execute() function exits
    }
};
