// - Tested in godbolt using gcc 15.2 intrinsics (YR 2025), https://godbolt.org/z/rP5jvf6dP
// - Tested in MinGW using g++ 13.1.0 (YR 2023) which is packed by default in my CLion 2025.1.4 

#include <thread>
#include <vector>

void AtomicIncrement(int* Dest)
{
    int Expected = *Dest;
    // The function returns true if *Dest == *Expected
    while (!__atomic_compare_exchange_n(
            /*type *ptr*/ Dest,
            /*type *expected*/ &Expected, // If *Dest != *Expected  then *Expected contains the new value at *Dest
            /*type desired*/ Expected + 1,
            /*bool weak*/ true, // Allows spurious failures that can happen on architectures that support something like LL/CS
            /*int success_memorder*/ __ATOMIC_RELEASE, // Allow subsequent instructions to be hoisted to before this operation
            /*int failure_memorder*/ __ATOMIC_RELAXED)); // Allow hoisting or sinking of other instructions respect to this operation
}

void AcquireLock(int* Lock) // Test-And-Set
{
    int Expected = 0;
    // The function returns true if *Dest == *Expected
    while (!__atomic_compare_exchange_n(
            /*type *ptr*/ Lock,
            /*type *expected*/ &Expected, // If *Dest != *Expected  then *Expected contains the new value at *Dest
            /*type desired*/ 1,
            /*bool weak*/ true, // Allows spurious failures that can happen on architectures that support something like LL/CS
            /*int success_memorder*/ __ATOMIC_RELEASE, // Allow subsequent instructions to be hoisted to before this operation
            /*int failure_memorder*/ __ATOMIC_RELAXED)) // Allow hoisting or sinking of other instructions respect to this operation
    {
        Expected = 0;   
    }
}

void AcquireLock2(volatile int* Lock) // Test-And----Test-And-Set
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

static int Counter = 0;

int main()
{
    std::vector<std::thread> Threads;
    for (int i = 0; i < 24; i++)
    {
        // This will generate a lot of contention and traffic
        Threads.emplace_back
            ([]()
            {
                // std::this_thread::sleep_for(std::chrono::seconds(1));
                for (int Index = 0; Index < 10000000; ++Index)
                {
                    //++Counter;
                    AtomicIncrement(&Counter);
                }
            }) ;        
    }

    for (auto& Thread : Threads)
    {
        Thread.join();
    }

    std::printf("%d\n", Counter);
    return 0;
}