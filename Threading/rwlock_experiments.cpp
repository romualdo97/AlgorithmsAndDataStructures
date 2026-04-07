// GO RWMutex study
// In this experiment I'll study multiple threads massively spamming the R-Lock/R-Unlock operation
// W-lock/W-Unlock is assumed to be called very infrequently (or never), the expectation is that modifying rw.readerCount
// in every R-Lock/R-Unlock will generate a lot of contention caused by constant invalidation of caches,
// code uses g++ intrinsics just for fun, but a subsequent iteration of the experiment should consider std::atomic
// Note: A bunch of important pieces of this code might get optimized out by the compiler, for now the experiment is assumed to be in debug builds
// https://cs.opensource.google/go/go/+/refs/tags/go1.26.1:src/sync/rwmutex.go;l=39

#include <cstdio>
#include <vector>
#include <thread>

void AtomicIncrement(int* Dest, int Delta)
{
    int Expected = *Dest;
    // The function returns true if *Dest == *Expected
    while (!__atomic_compare_exchange_n(
            /*type *ptr*/ Dest,
            /*type *expected*/ &Expected, // If *Dest != *Expected  then *Expected contains the new value at *Dest
            /*type desired*/ Expected + Delta,
            /*bool weak*/ true, // Allows spurious failures that can happen on architectures that support something like LL/CS
            /*int success_memorder*/ __ATOMIC_RELEASE, // Allow subsequent instructions to be hoisted to before this operation
            /*int failure_memorder*/ __ATOMIC_RELAXED)); // Allow hoisting or sinking of other instructions respect to this operation
}

void AtomicSet(int* Dest, int NewValue)
{
    int Expected = *Dest;
    // The function returns true if *Dest == *Expected
    while (!__atomic_compare_exchange_n(
            /*type *ptr*/ Dest,
            /*type *expected*/ &Expected, // If *Dest != *Expected  then *Expected contains the new value at *Dest
            /*type desired*/ NewValue,
            /*bool weak*/ true, // Allows spurious failures that can happen on architectures that support something like LL/CS
            /*int success_memorder*/ __ATOMIC_RELEASE, // Allow subsequent instructions to be hoisted to before this operation
            /*int failure_memorder*/ __ATOMIC_RELAXED)); // Allow hoisting or sinking of other instructions respect to this operation
}

struct RWMutex
{
    int ReaderCount = 0;
    
    void ReadLock()
    {
        AtomicIncrement(&ReaderCount, 1);
    }

    void ReadUnlock()
    {
        AtomicIncrement(&ReaderCount, -1);
    }
};
// ==================================================================

// Reading status for each thread should have their own exclusive cache line
struct alignas(64) ReadStatus
{
    bool IsReading = false;
};

template<int TotalPossibleReaders>
struct FatRWMutex
{
    enum Status
    {
        Writing = 0, // Exclusive write access, 100% certainty
        PossiblyReading = 1 // Someone has read before, not sure if they are still reading, you should check
    };

    Status MutexStatus = Status::Writing;

    // As ReadStatus has alignas(64), the compiler automatically calls: 
    // operator new(sizeof(MyCacheLine), std::align_val_t(64))
    ReadStatus* Readers = new ReadStatus[TotalPossibleReaders];

    void ReadLock(const int ReaderId) const
    {
        Readers[ReaderId].IsReading = true;
    }

    void ReadUnlock(const int ReaderId) const
    {
        Readers[ReaderId].IsReading = false;
    }
};

namespace
{
    RWMutex ReaderWriterMutex;
    FatRWMutex<24> FatReadWriterMutex;
}

int main()
{
    std::vector<std::thread> Threads;
    for (int i = 0; i < 24; ++i)
    {
        // This will generate a lot of contention and traffic
        Threads.emplace_back
            ([i]()
            {
                // std::this_thread::sleep_for(std::chrono::seconds(1));
                for (int Index = 0; Index < 1000000000; ++Index)
                {
                    ReaderWriterMutex.ReadLock();
                    // Protected read
                    ReaderWriterMutex.ReadUnlock();
                    
                    /*FatReadWriterMutex.ReadLock(i);
                    // Protected read
                    FatReadWriterMutex.ReadUnlock(i);*/
                }
            }) ;        
    }

    for (auto& Thread : Threads)
    {
        Thread.join();
    }

    // std::printf("%d\n", Counter);
    return 0;
}