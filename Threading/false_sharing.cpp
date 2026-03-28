#include <thread>
#include <cstdint>

#define CACHE_LINE_BYTES 0

struct alignas(CACHE_LINE_BYTES) Counter
{
    uint32_t Value{0};
};

Counter PerThreadCounters[] = { {}, {}, {}, {} };

void IncrementWorker(Counter* Counter)
{
    while (Counter->Value != pow(2,28))
    {
        ++(Counter->Value);    
    }
}

int main()
{
    std::thread ThreadA(IncrementWorker, PerThreadCounters);
    std::thread ThreadB(IncrementWorker, PerThreadCounters + 1);
    std::thread ThreadC(IncrementWorker, PerThreadCounters + 2);
    std::thread ThreadD(IncrementWorker, PerThreadCounters + 3);

    ThreadA.join();
    ThreadB.join();
    ThreadC.join();
    ThreadD.join();

    return 0;
}