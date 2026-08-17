#include <atomic>
#include <thread>

namespace
{
    std::atomic<bool> g_start{false};
    int g_sharedValue = 0;
}

int main()
{
    std::thread worker([]() {
        while (!g_start.load(std::memory_order_acquire))
        {
            std::this_thread::yield();
        }
        g_sharedValue = 1;
    });

    g_start.store(true, std::memory_order_release);
    g_sharedValue = 2;
    worker.join();
    return g_sharedValue == 0 ? 1 : 0;
}
