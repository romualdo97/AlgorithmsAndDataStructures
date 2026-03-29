#include <chrono>
#include <cstdint>
#include <cstdio>
#include <ctime>
#include <iostream>

int main()
{
    // Experiment from: https://en.algorithmica.org/hpc/cpu-cache/associativity/
    // My processor data
    // Arrow Lake
    // Cache Level      Size        Associativity       Latency
    // L0 Data	        48 KB	    12-way          	4 cycles
    // L0 Instruction	64 KB	    16-way	            -
    // L1 Data	        192 KB	    12-way	            9 cycles
    // L2 (Mid-Level)	3 MB*	    10-way	            17 cycles
    // L3 (Shared)	    3 MB/slice	12-way	            51–84 cycles
    
    // Create a big buffer
    constexpr int N = 1 << 20;
    constexpr int STEP_SIZE = 1024; // ~18ms and ~8ms if 1025 
    int* Buffer = new int[N * STEP_SIZE];

    auto Start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < N * STEP_SIZE; i += STEP_SIZE)
    {
        Buffer[i]++;
    }

    auto End = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> Duration = End - Start;
    std::cout << Duration.count() << "ms" << std::endl;

    delete[] Buffer;
    return 0;
}
