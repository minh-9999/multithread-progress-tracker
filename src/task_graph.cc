

#include "task_graph.hh"

Thread_Pool::Thread_Pool(size_t num_threads)
{
    for (size_t i = 0; i < num_threads; ++i)
    {
        workers.emplace_back([this]
                             {
                while (true)
                {
                    std::function<void()> task;
                    {
                        std::unique_lock lock(queue_mutex);
                        condition.wait(lock, [this] { return !tasks.empty(); });
                        task = std::move(tasks.front());
                        tasks.pop();
                    }

                    task();
                } });
    }
}

Thread_Pool::~Thread_Pool()
{
    for (std::thread &worker : workers)
    {
        worker.detach();
    }
}

TaskGraph::TaskGraph(size_t num_threads) : pool(num_threads) {}

void TaskGraph::addTask(std::shared_ptr<Task<void>> task)
{
    tasks.push_back(std::move(task));
}

bool TaskGraph::has_cycle()
{
    std::unordered_map<Task<void> *, int> visited;

    for (auto &task : tasks)
    {
        // if (detect_cycle(&task, visited))
        if (detect_cycle(task.get(), visited))
            return true; // There are cycles
    }

    return false;
}

bool TaskGraph::detect_cycle(Task<void> *task, std::unordered_map<Task<void> *, int> &visited)
{
    if (visited[task] == 1)
        return true; // Visited

    if (visited[task] == 2)
        return false; // Processing completed

    visited[task] = 1; // Mark visiting
    for (auto &dep : task->dependencies)
    {
        if (detect_cycle(dep, visited))
            return true;
    }

    visited[task] = 2;
    return false;
}

void TaskGraph::execute()
{
    std::unordered_map<Task<void> *, size_t> dependency_count;
    std::queue<Task<void> *> ready_tasks;

    for (auto &task : tasks)
    {
        size_t count = task->dependencies.size();
        // dependency_count[&task] = count;
        dependency_count[task.get()] = count;

        if (count == 0)
            // ready_tasks.push(&task);
            ready_tasks.push(task.get());
    }

    while (!ready_tasks.empty())
    {
        Task<void> *task = ready_tasks.front();
        ready_tasks.pop();
        pool.enqueue([task]
                     { task->execute(); });

        for (auto &next_task : tasks)
        {
            if (std::find(next_task->dependencies.begin(), next_task->dependencies.end(), task) != next_task->dependencies.end())
            {
                // if (--dependency_count[&next_task] == 0)
                // ready_tasks.push(&next_task);

                if (--dependency_count[next_task.get()] == 0)
                    ready_tasks.push(next_task.get());
            }
        }
    }
}

void TaskGraph::wait_all()
{
    for (auto &task : tasks)
    {
        if (task && task->handle)
            task->wait(); // Wait for task to complete (with both resume and wait on cv)
    }
}