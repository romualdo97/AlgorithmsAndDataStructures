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

            __m128 p = _mm_add_ps
            (
                _mm_mul_ps(xAxis, x),
                _mm_mul_ps(yAxis, y)                
            );
            __m128 q = _mm_add_ps
            (
                _mm_mul_ps(zAxis, z),
                _mm_mul_ps(wAxis, w)
            );

            _mm_store_ps(ResultPtr + a * N, _mm_add_ps(p, q));
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
            // instruction vbroadcastss, splats a scalar (with single-precision encoding) into the 4 DWORD slots of the xmm register
            // Previously I did: 4 Loads + 4 Splat operations.
            // The Shuffle way: 1 Load + 4 Shuffle operations.
            // The CPU can execute shuffle instructions much faster than it can fetch data from memory (even cache). Plus, by loading the whole Vec4 at once, you’re utilizing the full width of the data bus.
            __m128 Column = _mm_load_ps(RightPtr + a * N);

            // Shuffle single-precision (32-bit) floating-point elements in a using the control in imm8, and store the results in dst.
            __m128 x = _mm_shuffle_ps(Column, Column, _MM_SHUFFLE(0, 0, 0, 0));
            __m128 y = _mm_shuffle_ps(Column, Column, _MM_SHUFFLE(1, 1, 1, 1));
            __m128 z = _mm_shuffle_ps(Column, Column, _MM_SHUFFLE(2, 2, 2, 2));
            __m128 w = _mm_shuffle_ps(Column, Column, _MM_SHUFFLE(3, 3, 3, 3));

            __m128 p = _mm_add_ps
            (
                _mm_mul_ps(xAxis, x),
                _mm_mul_ps(yAxis, y)                
            );
            __m128 q = _mm_add_ps
            (
                _mm_mul_ps(zAxis, z),
                _mm_mul_ps(wAxis, w)
            );

            _mm_store_ps(ResultPtr + a * N, _mm_add_ps(p, q));
        }
        
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

    Mat4x4 Result = Mat4x4().MultiplyUltraFast(LeftOperand, RightOperand);
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