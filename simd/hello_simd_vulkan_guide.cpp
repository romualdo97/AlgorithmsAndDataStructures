#include <cstdint>
#include <xmmintrin.h>
#include <immintrin.h>
#include <memory>

// Adds B to A. The arrays must have the same size
void AddArrays(float* A, float* B, size_t Count)
{
    for(size_t i = 0; i < Count; i++)
    {
        A[i] += B[i];
    }
}

// Adds B to A. The arrays must have the same size
void AddArraysUnroll4(float* A, float* B, size_t count){
    size_t i = 0;
    for(; i + 4 <= count; i+= 4){
        A[i+0] += B[i+0]; 
        A[i+1] += B[i+1]; 
        A[i+2] += B[i+2]; 
        A[i+3] += B[i+3]; 
    }

    // loop terminator. What if the loop isn't a multiple of 4?
    for(; i < count; i++){
        A[i] += B[i]; 
    }
}

void AddArraySse(float* A, float* B, size_t Count)
{
    size_t Index = 0;
    for (; Index < Count; Index += 4)
    {
        _mm_store_ps
        (
            A + Index,
            _mm_add_ps // NOLINT(*-simd-intrinsics)
            (
                _mm_load_ps(A + Index),
                _mm_load_ps(B + Index)
            )
        );
    }
}

__attribute__((target("avx")))
void AddArrayAvx(float* A, float* B, size_t Count)
{
    size_t Index = 0;
    for (; Index < Count; Index += 8)
    {
        _mm256_store_ps(
            A + Index,
            _mm256_add_ps // NOLINT(*-simd-intrinsics)
            (
                _mm256_load_ps(A + Index),
                _mm256_load_ps(B + Index)
            )
        );
    }
}

struct Vec4
{
    float x{0};
    float y{0};
    float z{0};
    float w{0};
};

struct Mat4x4
{
    Vec4 xAxis;
    Vec4 yAxis;
    Vec4 zAxis;
    Vec4 wAxis;

    Mat4x4& MultiplySlow(const Mat4x4& Left, const Mat4x4& Right)
    {
        constexpr int N = 4;
        const auto ResultPtr = reinterpret_cast<float*>(this);
        const auto LeftPtr = reinterpret_cast<const float*>(&Left);
        const auto RightPtr = reinterpret_cast<const float*>(&Right);
        for (uint32_t a = 0; a < N; ++a)
        {
            for (uint32_t b = 0; b < N; ++b)
            {
                ResultPtr[a * N + b] = 0; // m[a][b] = 0
                // Dot between row m[k][b] and column m[a][k]
                for (uint32_t k = 0; k < N; ++k)
                {
                    const float LeftScalar = LeftPtr[k * N + b];
                    const float RightScalar = RightPtr[a * N + k];
                    ResultPtr[a * N + b] += LeftScalar * RightScalar;
                }
            }
        }
        return *this;
    }

    Mat4x4& MultiplyFast(const Mat4x4& Left, const Mat4x4& Right)
    {
        constexpr int N = 4;
        const auto ResultPtr = reinterpret_cast<float*>(this);
        const auto LeftPtr = reinterpret_cast<const float*>(&Left);
        const auto RightPtr = reinterpret_cast<const float*>(&Right);

        // Load left matrix columns in xmm registers
        __m128 xAxis = _mm_load_ps(LeftPtr + 0);
        __m128 yAxis = _mm_load_ps(LeftPtr + 4);
        __m128 zAxis = _mm_load_ps(LeftPtr + 8);
        __m128 wAxis = _mm_load_ps(LeftPtr + 12);

        for (uint32_t a = 0; a < 4; ++a)
        {
            // instruction vbroadcastss, splats a scalar (with single-precision encoding) into the 4 DWORD slots of the xmm register 
            __m128 x = _mm_set1_ps(RightPtr[a * N + 0]);
            __m128 y = _mm_set1_ps(RightPtr[a * N + 1]);
            __m128 z = _mm_set1_ps(RightPtr[a * N + 2]);
            __m128 w = _mm_set1_ps(RightPtr[a * N + 3]);

            __m128 p = _mm_add_ps // 1 add op + 2 mul op
            (
                _mm_mul_ps(xAxis, x),
                _mm_mul_ps(yAxis, y)                
            );
            __m128 q = _mm_add_ps // 1 add op + 2 mul op
            (
                _mm_mul_ps(zAxis, z),
                _mm_mul_ps(wAxis, w)
            );

            _mm_store_ps(
                ResultPtr + a * N,
                _mm_add_ps(p, q) // 1 add op
            );

            // total arithmetic ops: 7
        }
        
        return *this;
    }

    Mat4x4& MultiplyUltraFast(const Mat4x4& Left, const Mat4x4& Right)
    {
        constexpr int N = 4;
        const auto ResultPtr = reinterpret_cast<float*>(this);
        const auto LeftPtr = reinterpret_cast<const float*>(&Left);
        const auto RightPtr = reinterpret_cast<const float*>(&Right);

        // Load left matrix columns in xmm registers
        __m128 xAxis = _mm_load_ps(LeftPtr + 0);
        __m128 yAxis = _mm_load_ps(LeftPtr + 4);
        __m128 zAxis = _mm_load_ps(LeftPtr + 8);
        __m128 wAxis = _mm_load_ps(LeftPtr + 12);

        for (uint32_t a = 0; a < 4; ++a)
        {
            // Previously I did: 4 Loads + 4 Splat operations.
            // The Shuffle way: 1 Load + 4 Shuffle operations.
            // The CPU can execute shuffle instructions much faster than it can fetch data from memory (even cache). Plus, by loading the whole Vec4 at once, you’re utilizing the full width of the data bus.
            __m128 Column = _mm_load_ps(RightPtr + a * N);

            // Shuffle single-precision (32-bit) floating-point elements in a using the control in imm8, and store the results in dst.
            __m128 x = _mm_shuffle_ps(Column, Column, _MM_SHUFFLE(0, 0, 0, 0));
            __m128 y = _mm_shuffle_ps(Column, Column, _MM_SHUFFLE(1, 1, 1, 1));
            __m128 z = _mm_shuffle_ps(Column, Column, _MM_SHUFFLE(2, 2, 2, 2));
            __m128 w = _mm_shuffle_ps(Column, Column, _MM_SHUFFLE(3, 3, 3, 3));

            __m128 p = _mm_add_ps // 1 add op + 2 mul op
            (
                _mm_mul_ps(xAxis, x),
                _mm_mul_ps(yAxis, y)                
            );
            __m128 q = _mm_add_ps // 1 add op + 2 mul op
            (
                _mm_mul_ps(zAxis, z),
                _mm_mul_ps(wAxis, w)
            );

            _mm_store_ps(
                ResultPtr + a * N,
                _mm_add_ps(p, q) // 1 add op
            );

            // total arithmetic ops: 7
        }
        
        return *this;
    }

    __attribute__((target("avx2,fma")))
    Mat4x4& MultiplyBlazinglyFast(const Mat4x4& Left, const Mat4x4& Right)
    {
        constexpr int N = 4;
        const auto ResultPtr = reinterpret_cast<float*>(this);
        const auto LeftPtr = reinterpret_cast<const float*>(&Left);
        const auto RightPtr = reinterpret_cast<const float*>(&Right);

        // Load left matrix columns in xmm registers
        __m128 xAxis = _mm_load_ps(LeftPtr + 0);
        __m128 yAxis = _mm_load_ps(LeftPtr + 4);
        __m128 zAxis = _mm_load_ps(LeftPtr + 8);
        __m128 wAxis = _mm_load_ps(LeftPtr + 12);

        for (uint32_t a = 0; a < 4; ++a)
        {
            // Previously I did: 4 Loads + 4 Splat operations.
            // The Shuffle way: 1 Load + 4 Shuffle operations.
            // The CPU can execute shuffle instructions much faster than it can fetch data from memory (even cache). Plus, by loading the whole Vec4 at once, you’re utilizing the full width of the data bus.
            __m128 Column = _mm_load_ps(RightPtr + a * N);

            // Shuffle single-precision (32-bit) floating-point elements in a using the control in imm8, and store the results in dst.
            __m128 x = _mm_shuffle_ps(Column, Column, _MM_SHUFFLE(0, 0, 0, 0));
            __m128 y = _mm_shuffle_ps(Column, Column, _MM_SHUFFLE(1, 1, 1, 1));
            __m128 z = _mm_shuffle_ps(Column, Column, _MM_SHUFFLE(2, 2, 2, 2));
            __m128 w = _mm_shuffle_ps(Column, Column, _MM_SHUFFLE(3, 3, 3, 3));

            // RAW dependency p <- r and q <- s, 4 cycles per instruction, for a total:
            // (4 cycles * 4 instructions) / 2 FMA units = 8 cycles 
            __m128 p = _mm_mul_ps(y, yAxis); // 1 op
            __m128 q = _mm_mul_ps(w, wAxis); // 1 op
            __m128 r = _mm_fmadd_ps(x, xAxis, p); // 1 op
            __m128 s = _mm_fmadd_ps(z, zAxis, q); // 1 op

            _mm_store_ps(
                ResultPtr + a * N,
                _mm_add_ps(r, s) // RAW dependency on r and s, +4 cycles
            );

            // total arithmetic ops: 5, expected cycles for outlined arithmetic operations: 12
        }
        
        return *this;
    }

    __attribute__((target("avx2,fma")))
    Mat4x4& MultiplyMassivelyFast(const Mat4x4& Left, const Mat4x4& Right)
    {
        constexpr int N = 4;
        const auto ResultPtr = reinterpret_cast<float*>(this);
        const auto LeftPtr = reinterpret_cast<const float*>(&Left);
        const auto RightPtr = reinterpret_cast<const float*>(&Right);

        // Load left matrix columns in xmm registers
        __m128 xAxis = _mm_load_ps(LeftPtr + 0);
        __m128 yAxis = _mm_load_ps(LeftPtr + 4);
        __m128 zAxis = _mm_load_ps(LeftPtr + 8);
        __m128 wAxis = _mm_load_ps(LeftPtr + 12);

        for (uint32_t a = 0; a < 4; ++a)
        {
            // Previously I did: 4 Loads + 4 Splat operations.
            // The Shuffle way: 1 Load + 4 Shuffle operations.
            // The CPU can execute shuffle instructions much faster than it can fetch data from memory (even cache). Plus, by loading the whole Vec4 at once, you’re utilizing the full width of the data bus.
            __m128 Column = _mm_load_ps(RightPtr + a * N);

            // Shuffle single-precision (32-bit) floating-point elements in a using the control in imm8, and store the results in dst.
            __m128 x = _mm_shuffle_ps(Column, Column, _MM_SHUFFLE(0, 0, 0, 0));
            __m128 y = _mm_shuffle_ps(Column, Column, _MM_SHUFFLE(1, 1, 1, 1));
            __m128 z = _mm_shuffle_ps(Column, Column, _MM_SHUFFLE(2, 2, 2, 2));
            __m128 w = _mm_shuffle_ps(Column, Column, _MM_SHUFFLE(3, 3, 3, 3));

            // RAW dependency between each accumulated step, 4 cycles per instruction, for a total:
            // 4 cycles * 4 instructions = 16 cycles 
            __m128 Accumulator = _mm_mul_ps(x, xAxis); // 1 op
            Accumulator = _mm_fmadd_ps(y, yAxis, Accumulator); // 1 op
            Accumulator = _mm_fmadd_ps(z, zAxis, Accumulator); // 1 op
            Accumulator = _mm_fmadd_ps(w, wAxis, Accumulator); // 1 op

            _mm_store_ps(
                ResultPtr + a * N,
                Accumulator
            );

            // total arithmetic ops: 4 ops, expected cycles for outlined arithmetic operations: 14
        }
        
        return *this;
    }

    __attribute__((target("avx2,fma")))
    Mat4x4& MultiplyFlashyFast(const Mat4x4& Left, const Mat4x4& Right)
    {
        constexpr int N = 4;
        const auto ResultPtr = reinterpret_cast<float*>(this);
        const auto LeftPtr = reinterpret_cast<const float*>(&Left);
        const auto RightPtr = reinterpret_cast<const float*>(&Right);

        // Gemini: On modern CPUs (basically anything from the last 10 years),
        // the FPU and the FMA unit are actually the same hardware.
        // In older CPUs, there were separate "Adders" and "Multipliers." Today,
        // they are merged into a single block called an FMA Pipeline. 
        // _mm_mul_ps is just an FMA instruction where the "addition" part is hardwired to zero.
        // _mm_add_ps is just an FMA instruction where the "multiplication" part is hardwired to one.
        // _mm_fmadd_ps is the hardware running at full capacity (doing both).
        
        // Assuming a CPU with 2 FMA/FPU Units, 3 Load Units and 1 Store Unit
        // 20 load operations (_mm_load_ps and _mm_broadcast_ss)
        //      3 Load Units then 20op / 3units ~= 6.66667; ceil-ed to  7 cycles  
        //      6 cycles per op
        //      7 * 6 = 42 cycles [Incorrect as it assumes no pipeline]
        // 4 store operations (_mm_store_ps)
        //      1 store unit
        //      1 cycle per op
        //      4 * 1 = 4 cycles [Incorrect as it assumes no pipeline]
        // 16 arithmetic operations (_mm_mul_ps, _mm_fmadd_ps, _mm_add_ps)
        //      2 FMA/FPU Units then 16op / 2units = 8 cycles
        //      4 cycles per op
        //      8 * 4 = 32 cycles [Incorrect as it assumes no pipeline]
        // max theoretical 78 cycles

        // Gemini: In the estimate above, you were multiplying "cycles per op" (latency) by
        // the number of ops. However, once the "pipe" is full, the CPU
        // finishes one operation every cycle (or less), even if that operation
        // took 4 cycles to travel through the pipe, gemini calculates this is in reality 10 cycles assuming a full pipeline.

        // CPI: Cycles per instruction
        // The "Cost" in Cycles (Total Ops × CPI / Ports):
        // Operation                        Count   Throughput (CPI)    Total "Cost" in Cycles
        // Arithmetic (mul, fmadd, add)	    16	    0.5	                8 cycles
        // Loads/Broadcasts	                20	    0.33                6.6 cycles
        // Stores	                        4	    0.5	                2 cycles
        
        // Load left matrix columns in xmm registers
        __m128 xAxis = _mm_load_ps(LeftPtr + 0);
        __m128 yAxis = _mm_load_ps(LeftPtr + 4);
        __m128 zAxis = _mm_load_ps(LeftPtr + 8);
        __m128 wAxis = _mm_load_ps(LeftPtr + 12);

        // Gemini: Why broadcast is better than load then shuffle
        // Think of the CPU as a factory with different specialized workstations (Ports).
        // 1. The "Shuffle" Way (2 Instructions)
        // When you use _mm_load_ps + _mm_shuffle_ps, you are using two different workstations:
        // The Load Port: Grabs the 16-byte Vec4 from memory.
        // The Shuffle Port: Rearranges the data to splat the value.
        // If you do this for x, y, z, w, you have 1 Load + 4 Shuffles.
        // The problem? Modern CPUs usually only have one shuffle unit.
        // those 4 shuffles have to wait in line, one after the other.

        // 2. The "Broadcast" Way (1 Instruction)
        // _mm_broadcast_ss is a specialized "Express Lane." It tells the Load Port: "While you are pulling this float from memory, spray it into all four slots of the register before you even hand it to me."
        // Instruction Count: It’s 1 instruction instead of 2.
        // Bypasses the Shuffle Unit: It completely skips the shuffle workstation. This leaves the shuffle unit free to do other things (or just stay cool), and it removes the "lineup" wait time.
        // Fused Execution: On modern Intel/AMD chips, a broadcast-from-memory can often be fused directly into the math instruction.
        // The CPU doesn't even "store" the splatted value in a separate step; it just pipes it straight into the Multiplier.

        // Gemini: _mm_broadcast_ss vs _mm_set1_ps?
        // 1. _mm_broadcast_ss (The Memory specialist)
        // This instruction is designed to pull a value directly from a memory address and splat it. 
        // Source: A pointer (e.g., float*).
        // Mechanism: As the data travels from the L1 Cache to the CPU register, the hardware "sprays" it into all 4 slots.
        // Efficiency: It is a single hardware operation (vbroadcastss). It's basically a "Load + Splat" in one.
        // 2. _mm_set1_ps (The Generalist)
        // This is a high-level "wrapper" that can take a value from anywhere (a variable, a constant, or a register).
        // Source: A float variable or literal.
        // Mechanism: The compiler has to figure out where that float is.
        // If the float is already in a register, it has to use a Shuffle.
        // If it's in memory, it might do a scalar load and then a shuffle. 

        // Loop was unrolled to let the CPU interleave operations from different "iterations", 
        // Two RAW dependency chains are created p <- r and q <- s, which then converge at r + s (another RAW dependency)
        // this is for ILP purposes, assuming a CPU with two FMA units, the expected theoretical latency
        // in that machine is 12 cycles per matrix by vector operation.
        // Columns from Right matrix are loaded then "linearly combined" with columns in the left matrix 

        
        // Iteration 0: a * xAxis + b * yAxis + c * zAxis + d * wAxis
        __m128 xAxisRightX = _mm_broadcast_ss(RightPtr + 0 * N + 0);
        __m128 xAxisRightY = _mm_broadcast_ss(RightPtr + 0 * N + 1);
        __m128 xAxisRightZ = _mm_broadcast_ss(RightPtr + 0 * N + 2);
        __m128 xAxisRightW = _mm_broadcast_ss(RightPtr + 0 * N + 3);

        __m128 p0 = _mm_mul_ps(xAxis, xAxisRightX);
        __m128 q0 = _mm_mul_ps(yAxis, xAxisRightY);
        __m128 r0 = _mm_fmadd_ps(zAxis, xAxisRightZ, p0);
        __m128 s0 = _mm_fmadd_ps(wAxis, xAxisRightW, q0);
        _mm_store_ps(ResultPtr + 0 * N, _mm_add_ps(r0, s0));              

        // Iteration 1: e * xAxis + f * yAxis + g * zAxis + h * wAxis
        __m128 yAxisRightX = _mm_broadcast_ss(RightPtr + 1 * N + 0);
        __m128 yAxisRightY = _mm_broadcast_ss(RightPtr + 1 * N + 1);
        __m128 yAxisRightZ = _mm_broadcast_ss(RightPtr + 1 * N + 2);
        __m128 yAxisRightW = _mm_broadcast_ss(RightPtr + 1 * N + 3);

        __m128 p1 = _mm_mul_ps(xAxis, yAxisRightX);
        __m128 q1 = _mm_mul_ps(yAxis, yAxisRightY);
        __m128 r1 = _mm_fmadd_ps(zAxis, yAxisRightZ, p1);
        __m128 s1 = _mm_fmadd_ps(wAxis, yAxisRightW, q1);
        _mm_store_ps(ResultPtr + 1 * N, _mm_add_ps(r1, s1));

        // Iteration 2: i * xAxis + j * yAxis + k * zAxis + l * wAxis
        __m128 zAxisRightX = _mm_broadcast_ss(RightPtr + 2 * N + 0);
        __m128 zAxisRightY = _mm_broadcast_ss(RightPtr + 2 * N + 1);
        __m128 zAxisRightZ = _mm_broadcast_ss(RightPtr + 2 * N + 2);
        __m128 zAxisRightW = _mm_broadcast_ss(RightPtr + 2 * N + 3);

        __m128 p2 = _mm_mul_ps(xAxis, zAxisRightX);
        __m128 q2 = _mm_mul_ps(yAxis, zAxisRightY);
        __m128 r2 = _mm_fmadd_ps(zAxis,zAxisRightZ, p2);
        __m128 s2 = _mm_fmadd_ps(wAxis,zAxisRightW, q2);
        _mm_store_ps(ResultPtr + 2 * N, _mm_add_ps(r2, s2));

        // Iteration 3: m * xAxis + n * yAxis + o * zAxis + p * wAxis
        __m128 wAxisRightX = _mm_broadcast_ss(RightPtr + 3 * N + 0);
        __m128 wAxisRightY = _mm_broadcast_ss(RightPtr + 3 * N + 1);
        __m128 wAxisRightZ = _mm_broadcast_ss(RightPtr + 3 * N + 2);
        __m128 wAxisRightW = _mm_broadcast_ss(RightPtr + 3 * N + 3);

        __m128 p3 = _mm_mul_ps(xAxis, wAxisRightX);
        __m128 q3 = _mm_mul_ps(yAxis, wAxisRightY);
        __m128 r3 = _mm_fmadd_ps(zAxis,wAxisRightZ, p3);
        __m128 s3 = _mm_fmadd_ps(wAxis,wAxisRightW, q3);
        _mm_store_ps(ResultPtr + 3 * N, _mm_add_ps(r3, s3));
                
        return *this;
    }
};

int main()
{
    //                  10  12  14  16  18  20  22  24
    float BufferA[]{    1,  2,  3,  4,  5,  6,  7,  8};
    float BufferB[]{    9,  10, 11, 12, 13, 14, 15, 16};
    AddArrayAvx(BufferA, BufferB, 8);

    Mat4x4 LeftOperand
    {
        Vec4{5, 2, 9, 1},
        Vec4{4, 1, 7, 6},
        Vec4{2, 9, 3, 8},
        Vec4{1, 5, 2, 6},
    };

    Mat4x4 RightOperand
    {
        Vec4{3, 5, 2, 8},
        Vec4{2, 8, 1, 1},
        Vec4{9, 4, 7, 3},
        Vec4{6, 1, 5, 2},
    };

    Mat4x4 Result = Mat4x4().MultiplyFlashyFast(LeftOperand, RightOperand);
    
    return 0;
}

/* it 1
struct Vec4
{
    float x{ 0 };
    float y{ 0 };
    float z{ 0 };
    float w{ 0 };
};

struct Mat4x4 // Column-major matrix
{
    Vec4 xAxis;
    Vec4 yAxis;
    Vec4 zAxis;
    Vec4 wAxis;

    void MultiplySlow(const Mat4x4& LeftOperand, const Mat4x4& RightOperand)
    {
        constexpr uint32_t N = 4;
        auto Output = reinterpret_cast<float*>(this);
        auto LeftFloatPtr = reinterpret_cast<const float*>(&LeftOperand);
        auto RightFloatPtr = reinterpret_cast<const float*>(&RightOperand);
        for (uint32_t j = 0; j < N; ++j) // j of the i,j index in linear algebra terms 
        {
            for (uint32_t i = 0; i < N; ++i) // i of the i,j index in linear algebra terms
            {
                const uint32_t OutputIndexThroughColumns = j * N + i;
                Output[OutputIndexThroughColumns] = 0; // Zeroes

                // In linear algebra terms m[i,j] = Dot(LeftMatrix_RowI, RightMatrix_ColumnJ)
                for (uint32_t k = 0; k < N; ++k)
                {
                    const uint32_t IndexThroughColumn = j * N + k; // Move through a column j of the matrix (in linear algebra terms, i changes,  j fixed)
                    const uint32_t IndexThroughRow = k * N + i; // Move through a row i of the matrix (in linear algebra terms,       i fixed,    j changes)

                    Output[OutputIndexThroughColumns] +=
                        LeftFloatPtr[IndexThroughRow] * RightFloatPtr[IndexThroughColumn]; 
                }
            }
        }
    }
};
*/