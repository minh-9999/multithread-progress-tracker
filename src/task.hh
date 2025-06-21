#pragma once

#include <coroutine>
#include <mutex>
#include <optional>
#include <utility>

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
    std::optional<T> value;
    std::exception_ptr exception;

    Task<T> get_return_object();

    std::suspend_never initial_suspend() { return {}; }

    auto final_suspend() noexcept
    {
        struct awaiter
        {
            std::coroutine_handle<TaskPromise> handle;

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
                if (handle.promise().exception)
                    std::rethrow_exception(handle.promise().exception);
            }
        };

        // return awaiter{};
        return awaiter{std::coroutine_handle<TaskPromise>::from_promise(*this)};
    }

    void unhandled_exception() { exception = std::current_exception(); }

    void return_value(T v)
    {
        assert(!value.has_value());
        value = std::move(v);
    }
};

template <>
struct TaskPromise<void> : public TaskPromiseBase
{
    std::exception_ptr exception;

    Task<void> get_return_object();

    std::suspend_never initial_suspend() { return {}; }

    auto final_suspend() noexcept
    {
        struct awaiter
        {
            TaskPromise *promise;

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
                if (promise->exception)
                    std::rethrow_exception(promise->exception);
            }
        };

        return awaiter{};
    }

    void unhandled_exception() { exception = std::current_exception(); }

    void return_void() {}
};

template <typename T>
struct Task
{
    using promise_type = TaskPromise<T>;
    std::coroutine_handle<promise_type> handle;

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

    bool await_ready() { return false; }

    void await_suspend(std::coroutine_handle<> h)
    {
        handle.promise().continuation = h;
    }

    T await_resume()
    {
        auto &promise = handle.promise();
        if (promise.exception)
            std::rethrow_exception(promise.exception);

        return std::move(promise.value).value();
    }

    void wait()
    {
        handle.resume();
        std::unique_lock lock(handle.promise().mtx);
        handle.promise().cv.wait(lock, [&]
                                 { return handle.promise().ready; });
    }

    T get()
    {
        wait();
        if (handle.promise().exception)
            std::rethrow_exception(handle.promise().exception);

        return *handle.promise().value;
    }
};

template <>
struct Task<void>
{
    using promise_type = TaskPromise<void>;
    std::coroutine_handle<promise_type> handle;

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

    bool await_ready() { return false; }

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
};

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

    co_await Awaiter{duration};
}

template <typename T>
Task<T> TaskPromise<T>::get_return_object() { return Task<T>{std::coroutine_handle<TaskPromise<T>>::from_promise(*this)}; }

inline Task<void> TaskPromise<void>::get_return_object() { return Task<void>{std::coroutine_handle<TaskPromise<void>>::from_promise(*this)}; }
