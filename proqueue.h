#ifndef INCLUDE_PROQUEUE
#define INCLUDE_PROQUEUE

/* 非同期処理待ちキュー
 *
 * elh::proqueue<int> queue;
 * queue.add_callback( [] (int i) { std::cout << i << std::endl; } );
 * for (auto i=0; i < 5; i++) {
 *     queue.push(i);
 * }
 *
 * >> 0
 * >> 1
 * >> 2
 * >> 3
 * >> 4
 */

#include <algorithm>
#include <queue>
#include <functional>
#include <array>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>

namespace elh {
    template<typename T>
    class proqueue : private std::queue<T> {
            using function_type = std::function<void(T&)>;
            static constexpr int MAX_TASK_SIZE { 10 };  // max size of functions.
            std::array<function_type, MAX_TASK_SIZE> tasks; // functions to be provided variables.
            std::recursive_mutex mutex;
            std::condition_variable_any condition;  // to block the thread.
            std::thread thread;
            bool shutdown_required { false }; // if this become true, the thread must be terminated.

            // is "state variable" ready?
            bool state_is_ready() noexcept {
                return !shutdown_required && !empty();
            }

            // obtain a lock in code block.
            inline const std::unique_lock<std::recursive_mutex> get_lock() noexcept {
                return std::unique_lock<std::recursive_mutex> { mutex };
            }

            void thread_work() noexcept;    // thread looper
            void proc_one() noexcept;   // this processes one from the queue.

        public:
            template <class... Functions>
            proqueue(const function_type& func, const Functions&...);
            ~proqueue();

            int tasks_count() noexcept;
            void add_callback(const function_type& func);

            void stop() noexcept;
            void pop();
            void push(const T& t) noexcept;
            void emplace();

            // thread safe member functions. (e.g. Read Only functions)
            using std::queue<T>::size;
            using std::queue<T>::empty;
            using std::queue<T>::front;
            using std::queue<T>::back;
    };

    template <class T>
    void proqueue<T>::thread_work() noexcept {
        while (!shutdown_required) {
            auto lock = get_lock();
            // if empty the queue, thread stops at here.
            if (!state_is_ready()) condition.wait(lock, [this] { return (!empty()) || shutdown_required; } );
            proc_one();
        }

        // clean up remaining objects in queue.
        while (!empty()) {
            proc_one();
        }
    }

    template <class T>
    void proqueue<T>::proc_one() noexcept {
        auto& target = front();
        for (auto& i : tasks) {
            if (i) i(target);
        }
        pop();
    }

    template<class T> template <class... Functions>
    proqueue<T>::proqueue(const function_type& func, const Functions&... remaining) : thread { &proqueue<T>::thread_work, this } {
        tasks.at(tasks()) = func;
    }

    template <class T>
    proqueue<T>::~proqueue() {
        stop();
    }

    template <class T>
    int proqueue<T>::tasks_count() noexcept {
        return std::count_if(tasks.begin(), tasks.end(), [](const function_type& func) {
            return func;
        });
    }

    template<class T>
    void proqueue<T>::add_callback(const function_type& callback) {
        auto l = get_lock();
        try {
            tasks.at(tasks()) = callback;
        } catch (std::out_of_range& e) {
            throw std::out_of_range { "Size of tasks exceeds MAX_TASK_SIZE" };
        }
    }

    template <class T>
    void proqueue<T>::stop() noexcept {
        condition.notify_one();
        thread.join();
    }

    template <class T>
    void proqueue<T>::pop() {
        get_lock();
        std::queue<T>::pop();
    }

    template <class T>
    void proqueue<T>::push(const T &t) noexcept {
        std::queue<T>::push(t);
    }
} // namespace elh

#endif
