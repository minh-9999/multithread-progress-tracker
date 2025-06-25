#pragma once

#include <coroutine>
#include <mutex>
#include <optional>
#include <utility>
#include <vector>

#include "process.hh"

template <typename T = void>
struct Task;

struct TaskPromiseBase
{
    std::coroutine_handle<> continuation;
    std::mutex mtx;
    std::condition_variable cv;
    bool ready = false;
};

template <typename T>
struct TaskPromise : public TaskPromiseBase
{
    std::optional<T> value;       // Coroutine return result
    std::exception_ptr exception; // Exception if coroutine throws an error

    Task<T> get_return_object(); // Function returns Task object from promise

    // Coroutine starts without suspending
    std::suspend_never initial_suspend() { return {}; }

    // Coroutine will suspend at the end and continue to execute the next
    // coroutine (if any)
    auto final_suspend() noexcept
    {
        struct awaiter
        {
            std::coroutine_handle<TaskPromise> handle;

            // Not ready, so coroutine will actually suspend
            bool await_ready() noexcept { return false; }

            // Coroutine suspends here; continues previously called coroutine if any
            void await_suspend(std::coroutine_handle<TaskPromise> h) noexcept
            {
                TaskPromise &promise = h.promise();

                {
                    std::unique_lock lock(promise.mtx);
                    promise.ready = true; // Mark completed
                }
                promise.cv.notify_all();

                if (promise.continuation)
                    promise.continuation.resume(); // Continue to next coroutine
            }

            // If there is an error in the coroutine, throw an exception
            void await_resume() noexcept
            {
                if (handle.promise().exception)
                    std::rethrow_exception(handle.promise().exception);
            }
        };

        // return awaiter{};

        // Returns the awaiter that will be called in final_suspend
        return awaiter{std::coroutine_handle<TaskPromise>::from_promise(*this)};
    }

    // Coroutine throws error without handling -> saves exception
    void unhandled_exception() { exception = std::current_exception(); }

    // Return value from coroutine
    void return_value(T v)
    {
        assert(!value.has_value());
        value = std::move(v);
    }
};

/*
- Use for coroutines without co_return <value>, just co_return;.
- Still able to save and re-throw exceptions.
- Coordinate coroutine start/end like TaskPromise<T>
*/
template <>
struct TaskPromise<void> : public TaskPromiseBase
{
    std::exception_ptr exception; // Exception if coroutine throws an error

    Task<void> get_return_object();

    std::suspend_never initial_suspend() { return {}; }

    auto final_suspend() noexcept
    {
        // Custom awaiter handles coroutine end logic
        struct awaiter
        {
            TaskPromise *promises;

            bool await_ready() noexcept { return false; }

            void await_suspend(std::coroutine_handle<TaskPromise> h) noexcept
            {
                TaskPromise &promise = h.promise();
                {
                    std::unique_lock lock(promise.mtx);
                    promise.ready = true;
                }

                promise.cv.notify_all();

                if (promise.continuation)
                    promise.continuation.resume();
            }

            void await_resume() noexcept
            {
                if (promises->exception)
                    std::rethrow_exception(promises->exception);
            }
        };

        return awaiter{};
    }

    void unhandled_exception() { exception = std::current_exception(); }

    // Coroutine calls `co_return;` without value, so use return_void()
    void return_void() {}
};

// Specialization of Task<T> for return type void (no value)
template <>
struct Task<void>
{
    using promise_type = TaskPromise<void>;
    std::coroutine_handle<promise_type> handle;
    std::vector<Task<void> *> dependencies; // List of dependent coroutines

    // no coroutine needed right now
    Task() : handle(nullptr) {}

    Task(std::coroutine_handle<promise_type> h) : handle(h) {}

    Task(const Task &) = delete;

    Task(Task &&other) noexcept : handle(other.handle) { other.handle = nullptr; }

    Task &operator=(Task &&other) noexcept
    {
        if (this != &other)
        {
            if (handle)
                handle.destroy();

            handle = other.handle;
            other.handle = nullptr;
        }

        return *this;
    }

    ~Task()
    {
        if (handle)
            handle.destroy();
    }

    // Always suspend to keep coroutine running
    bool await_ready() { return false; }

    bool operator==(const Task<void> &other) const
    {
        return handle == other.handle; // Compare based on coroutine handle
    }

    void await_suspend(std::coroutine_handle<> h)
    {
        handle.promise().continuation = h;
    }

    void await_resume()
    {
        if (handle.promise().exception)
            std::rethrow_exception(handle.promise().exception);
    }

    void wait()
    {
        handle.resume();
        std::unique_lock lock(handle.promise().mtx);

        handle.promise().cv.wait(lock, [&]
                                 { return handle.promise().ready; });
    }

    void add_dependency(Task<void> &dep)
    {
        // dependencies.push_back(std::move(dep));
        dependencies.push_back(&dep);
    }

    void execute()
    {
        for (auto *d : dependencies)
        {
            if (d)
                d->wait(); // Wait for dependent coroutine to complete
        }

        handle.resume();
    }

    // ✅  Add dependency via pointer
    void dependsOn(Task<void> &dep)
    {
        dependencies.push_back(&dep);
    }
};

// Task class represents coroutine returning T
template <typename T>
struct Task
{
    // Define promise_type so that the coroutine knows to use TaskPromise<T>
    using promise_type = TaskPromise<T>;
    std::coroutine_handle<promise_type> handle; // Handle to coroutine

    //  List of dependencies
    std::vector<Task<void> *> dependencies;

    // Constructor: get coroutine handle from compiler when calling co_return
    Task(std::coroutine_handle<promise_type> h) : handle(h) {}

    Task(const Task &) = delete;

    Task(Task &&other) noexcept : handle(other.handle) { other.handle = nullptr; }

    // Move assignment operator: destroy old coroutine, get new coroutine
    Task &operator=(Task &&other) noexcept
    {
        if (this != &other)
        {
            if (handle)
                handle.destroy(); // Cancel old coroutine if any

            handle = other.handle;
            other.handle = nullptr;
        }

        return *this;
    }

    ~Task()
    {
        if (handle)
            handle.destroy();
    }

    // Always suspend to keep coroutine running
    bool await_ready() { return false; }

    // When co_await Task, save the calling coroutine (continuation)
    void await_suspend(std::coroutine_handle<> h)
    {
        handle.promise().continuation = h;
    }

    // When coroutine completes, return result
    T await_resume()
    {
        auto &promise = handle.promise();

        // If there is an exception, rethrow
        if (promise.exception)
            std::rethrow_exception(promise.exception);

        // Return the value stored in the promise
        return std::move(promise.value).value();
    }

    // Run coroutine and wait until it completes (synchronous)
    void wait()
    {
        handle.resume(); // Start coroutine execution
        std::unique_lock lock(handle.promise().mtx);

        // Wait until the coroutine marked as completed
        handle.promise().cv.wait(lock, [&]
                                 { return handle.promise().ready; });
    }

    // Run coroutine and return result after waiting
    T get()
    {
        wait();

        // Throw an error if any
        if (handle.promise().exception)
            std::rethrow_exception(handle.promise().exception);

        return *handle.promise().value;
    }

    void execute()
    {
        for (auto *dep : dependencies)
        {
            if (dep)
                dep->wait();
        }

        handle.resume();
    }

    // ✅ NEW: Add dependency
    void dependsOn(Task<void> &dep)
    {
        dependencies.push_back(&dep);
    }
};



inline Task<void> process_simd_data(Task<void> dep_task, std::span<uint8_t> data)
{
    co_await dep_task;       // Wait for previous coroutine to complete
    process_data_simd(data); // Apply SIMD
}

/*
pauses the coroutine for a duration without blocking the main thread.
*/
inline Task<void> sleepAsync(std::chrono::milliseconds duration) noexcept
{
    struct Awaiter
    {
        std::chrono::milliseconds duration;

        bool await_ready() const noexcept { return duration.count() <= 0; }

        void await_suspend(std::coroutine_handle<> h) noexcept
        {
            std::jthread([duration = this->duration, h]() mutable
                         {
        std::this_thread::sleep_for(duration);
        h.resume(); })
                .detach();
        }

        void await_resume() noexcept {}
    };

    // Directly instantiate an object of type Awaiter with duration
    co_await Awaiter{duration}; // list-initialization (uniform initialization)

    // The compiler will translate it into (equivalent)
    /*
    auto&& awaiter = Awaiter{duration}; // initialize awaiter

    if (!awaiter.await_ready())
    {
        // if not ready then...
        co_await awaiter.await_suspend(h); // suspend coroutine and do something
    }

    awaiter.await_resume(); // when resume, continue running
    */
}

/*
 * Returns a Task<T> that manages the current coroutine.
 * - Constructs a coroutine handle from the promise object (*this).
 * - Wraps the handle in a Task<T> instance, which provides access to the coroutine.
 */
template <typename T>
Task<T> TaskPromise<T>::get_return_object()
{
    // create a coroutine handle from the current TaskPromise<T>
    return Task<T>{std::coroutine_handle<TaskPromise<T>>::from_promise(*this)};
}

/*
 * Returns a Task<void> for a coroutine that doesn't return a value.
 * - Constructs a coroutine handle from this promise object.
 * - Wraps the handle in a Task<void>, which can be awaited or waited on.
 * - Called automatically at coroutine initialization.
 */
inline Task<void> TaskPromise<void>::get_return_object()
{
    // creates and returns a coroutine manager object — here Task<void>,
    // which represents a coroutine with no return value
    return Task<void>{std::coroutine_handle<TaskPromise<void>>::from_promise(*this)};
}
