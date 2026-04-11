// GO RWMutex study
// In this experiment I'll study multiple threads massively spamming the R-Lock/R-Unlock operation
// W-lock/W-Unlock is assumed to be called very infrequently (or never), the expectation is that modifying rw.readerCount
// in every R-Lock/R-Unlock will generate a lot of contention caused by constant invalidation of caches (confirmed via Vtune),
// code uses g++ intrinsics just for fun, but a subsequent iteration of the experiment should consider std::atomic
// Note: A bunch of important pieces of this code might get optimized out by the compiler, for now the experiment is assumed to be in debug builds
// https://cs.opensource.google/go/go/+/refs/tags/go1.26.1:src/sync/rwmutex.go;l=39

#include <cstdio>
#include <vector>
#include <thread>
#include <chrono>

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

void AcquireLock(volatile int* Lock) // Test-And----Test-And-Set
{
    while (true)
    {
        // Wait for the lock to appear "released", avoids to submit
        // BusRdx transactions in the interconnect which would invalidate other core's cache
        while (*Lock != 0) {} // Lock is volatile to avoid compiler being to smart and removing this

        // The function returns true if *Dest == *Expected
        int Expected = 0;
        if (__atomic_compare_exchange_n(
                /*type *ptr*/ Lock,
                /*type *expected*/ &Expected, // If *Dest != *Expected  then *Expected contains the new value at *Dest
                /*type desired*/ 1,
                /*bool weak*/ true, // Allows spurious failures that can happen on architectures that support something like LL/CS
                /*int success_memorder*/ __ATOMIC_RELEASE, // Allow subsequent instructions to be hoisted to before this operation
                /*int failure_memorder*/ __ATOMIC_RELAXED)) // Allow hoisting or sinking of other instructions respect to this operation
        {
            return;
        }
    }
}

void ReleaseLock(int* Lock)
{
    *Lock = 0; // Assumes lock is already acquired, UB if that's not true
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

// Each observer should have a local copy of the global truth to avoid cache invalidations on read
// We interpret each thread to be an observer (inspiring the definition from ARM manuals where an
// observer is anything connected to the memory bus that can perform reads or writes)
struct alignas(64) ObserverStatus 
{
    int ReadLock = 0;
};

/**
 * Not rigorously tested cache friendly RW Mutex, the "fat" prefix alludes to the fact
 * that this mutex has a big spatial cost, each observer requiring at least 64 bytes to avoid one Observer
 * invalidating caches for other Observers in scenarios of high read traffic, this is key to make ReadLock operations "blazingly" fast,
 *
 * Implementation inspires from the concept of fine-grained locking, the state is distributed (duplicated) through the different
 * physical Core's caches, read locking/unlocking will only touch the local lock, write locking/unlocking on the other hand
 * requires that the writer takes all the locks which is obviously expensive, but acceptable as this data structure assumes
 * infrequent writes and ideally a single writer
 * 
 * @tparam TotalObservers Num of threads that the Fat RW Mutex will track
 */
template<int TotalObservers>
struct FatRWMutex
{
    FatRWMutex()
    {
        Observers = new ObserverStatus[TotalObservers];
    }

    ~FatRWMutex()
    {
        delete[] Observers;
        Observers = nullptr;
    }

    void ReadLock(const int ObserverId) const
    {
        // Spin wait for the lock, a read operation can only load/store data from their local cache
        AcquireLock(&Observers[ObserverId].ReadLock);
    }

    void ReadUnlock(const int ObserverId) const
    {
        ReleaseLock(&Observers[ObserverId].ReadLock);
    }

    void WriteLock(const int ObserverId) const
    {
        // A write lock operation pays a high price,
        // as it needs to acquire the lock for all observers,
        // this is a naive approach, multiple things can go wrong here
        // for example eager readers can cause starvation on writers,
        // this is deadlock-free as ReadLock's are acquired by all threads in the same order 
        for (int i = 0; i < TotalObservers; ++i)
        {
            AcquireLock(&Observers[i].ReadLock);
        }
    }

    void WriteUnlock(const int ObserverId) const
    {
        for (int i = 0; i < TotalObservers; ++i)
        {
            ReleaseLock(&Observers[i].ReadLock);
        }
    }

private:
    // As ReadStatus has alignas(64), the compiler automatically calls: 
    // operator new(sizeof(MyCacheLine), std::align_val_t(64))
    ObserverStatus* Observers = nullptr;
};

class FScopedTimer // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
    explicit FScopedTimer(
        const char* InFunctionName,
        const char* InDebugName = nullptr) :
        FunctionName(InFunctionName),
        DebugTagName(InDebugName)
    {
        if (DebugTagName)
        {
            std::printf(
                "[%s] (tag:%s) started\n",
                FunctionName,
                DebugTagName);
        }
        else
        {
            std::printf(
                "[%s] started\n",
                FunctionName);
        }

        StartTime = std::chrono::high_resolution_clock::now();
    }

    ~FScopedTimer()
    {
        const std::chrono::time_point<std::chrono::high_resolution_clock> EndTime = std::chrono::high_resolution_clock::now();
        const std::chrono::microseconds us = std::chrono::duration_cast<std::chrono::microseconds>(EndTime - StartTime);
        const std::chrono::seconds s = std::chrono::duration_cast<std::chrono::seconds>(EndTime - StartTime);
        

        if (DebugTagName)
        {
            std::printf(
                "[%s] (tag:%s) ended after %llu seconds (%llu microseconds)\n",
                FunctionName,
                DebugTagName,
                s.count(),
                us.count());        
        }
        else
        {
            std::printf(
                "[%s] ended after %llu seconds (%llu microseconds)\n",
                FunctionName,
                s.count(),
                us.count()); 
        }
        
        FunctionName = nullptr;
        DebugTagName = nullptr;
    }
private:    
    const char* FunctionName;
    const char* DebugTagName;
    std::chrono::time_point<std::chrono::high_resolution_clock> StartTime;
};

namespace
{
    RWMutex ReaderWriterMutex;
    FatRWMutex<24> FatReadWriterMutex;
}

int main()
{
    FScopedTimer ProgramExecution("RWMutexExperiment");
    
    std::vector<std::thread> Threads;
    for (int i = 0; i < 24; ++i)
    {
        Threads.emplace_back
            ([i]()
            {
                // std::this_thread::sleep_for(std::chrono::seconds(1));
                for (int Index = 0; Index < 1'000'000'000; ++Index)
                {
                    // This will generate a lot of contention and traffic                    
                    ReaderWriterMutex.ReadLock();
                    // Protected read
                    ReaderWriterMutex.ReadUnlock();

                    // Ideally each physical core interacts with their local cache without invalidating other caches
                    //FatReadWriterMutex.ReadLock(i);
                    // Protected read
                    //FatReadWriterMutex.ReadUnlock(i);
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
