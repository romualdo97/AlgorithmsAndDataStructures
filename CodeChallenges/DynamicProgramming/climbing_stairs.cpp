// https://leetcode.com/problems/climbing-stairs/description/
// You are climbing a staircase. It takes n steps to reach the top.
// Each time you can either climb 1 or 2 steps. In how many distinct ways can you climb to the top?

#include <chrono>
#include <stack>
#include <cstdio>
#include <unordered_map>

class ScopedWallStopwatch
{
public:
    ScopedWallStopwatch()
    {
        Start = std::chrono::high_resolution_clock::now();
    }

    ~ScopedWallStopwatch()
    {
        const auto End = std::chrono::high_resolution_clock::now();
        printf("%lld us\n", std::chrono::duration_cast<std::chrono::microseconds>(End - Start).count());
    }

private:
    std::chrono::time_point<std::chrono::system_clock> Start;
};

template<int N>
struct Fibonacci
{
    static constexpr int Value = Fibonacci<N-1>::Value + Fibonacci<N-2>::Value;
};

template<>
struct Fibonacci<0>
{
    static constexpr int Value = 0; 
};

template<>
struct Fibonacci<1>
{
    static constexpr int Value = 1;
};

int NoRecursiveBruteForce(int InitialStair)
{
    int OutPaths = 0;

    std::stack<int> Stack;
    Stack.push(InitialStair);

    // Depth-First-Search
    while (!Stack.empty())
    {
        int Current = Stack.top();
        Stack.pop();

        int RightChild = Current - 2;
        if (RightChild > 1)
        {
            Stack.push(RightChild);    
        }
        else if (RightChild == 1)
        {
            ++OutPaths;
        }

        int LeftChild = Current - 1;
        if (LeftChild > 1)
        {
            Stack.push(LeftChild);    
        }
        else if (LeftChild == 1)
        {
            ++OutPaths;
        }
    }

    return OutPaths;
}

static std::unordered_map<
        int /* StairId */,
        int /* PossiblePathsFromStairId */> Table
    {
        { 2, Fibonacci<2>::Value },
        { 3, Fibonacci<3>::Value },
        { 4, Fibonacci<4>::Value },
        { 5, Fibonacci<5>::Value },
        { 6, Fibonacci<6>::Value },
        { 7, Fibonacci<7>::Value },
        { 8, Fibonacci<8>::Value },
        { 9, Fibonacci<9>::Value },
        { 10, Fibonacci<10>::Value },
    };

int NoRecursiveWithStaticTable(int InitialStair)
{
    int OutPaths = 0;
    
    std::stack<int> Stack;
    Stack.push(InitialStair);

    // Depth-First-Search
    while (!Stack.empty())
    {
        int Current = Stack.top();
        Stack.pop();

        if (Table.contains(Current))
        {
            OutPaths += Table[Current];
            continue;
        }

        int RightChild = Current - 2;
        if (RightChild > 1)
        {
            Stack.push(RightChild);    
        }
        else if (RightChild == 1)
        {
            ++OutPaths;
        }
        
        int LeftChild = Current - 1;
        if (LeftChild > 1)
        {
            Stack.push(LeftChild);    
        }
        else if (LeftChild == 1)
        {
            ++OutPaths;
        }
    }

    return OutPaths;
}

int NoRecursiveWithMemoization(int InitialStair)
{
    using Pair = std::pair<
        int /* StairId */,
        int /* PossiblePathsFromStairId */>;

    // We just need to remember last two answers
    int MemoizationIt = 1;
    static constexpr int MemoizationSize = 2;
    Pair Memoization[MemoizationSize]
    {
        { 0, 0 },
        { 1, 1 },
    };

    int OutPaths = 0;

    std::stack<int> Stack;
    Stack.push(InitialStair);

    // Depth-First-Search
    while (!Stack.empty())
    {
        int Current = Stack.top();
        Stack.pop();

        const int LeftChild = Current - 1;
        const int RightChild = Current - 2;

        const Pair& CurrMemoization = Memoization[MemoizationIt % MemoizationSize];
        const Pair& PrevMemoization = Memoization[(MemoizationIt - 1) % MemoizationSize];

        // Check if "num of paths" for left and right parent are already known
        if (LeftChild == CurrMemoization.first &&
           RightChild == PrevMemoization.first)
        {
            // Calculate total paths of children node
            const int TotalPaths = CurrMemoization.second + PrevMemoization.second;
            OutPaths += TotalPaths;

            // Update next memoization
            Memoization[++MemoizationIt % MemoizationSize] = { Current, TotalPaths };
            continue;
        }

        // For the right branch of the binary tree, check if Current was already computed
        bool bAlreadyCalculated = false;
        for (const Pair& PairValue : Memoization)
        {
            if (Current == PairValue.first)
            {
                bAlreadyCalculated = true;
                OutPaths += PairValue.second;
                break;
            }
        }
        if (bAlreadyCalculated)
        {
            continue;
        }

        // Insert new nodes
        Stack.push(RightChild);
        Stack.push(LeftChild);
    }

    return OutPaths;
}

int CalculateFibonacci(int InitialStair)
{
    static constexpr int MemoizationSize = 2;
    int Memoization[MemoizationSize]
    {
        0, 1
    };

    int i = 2;
    while (i <= InitialStair)
    {
        Memoization[i % MemoizationSize] =
            Memoization[(i - 1) % MemoizationSize] +
            Memoization[(i - 2) % MemoizationSize];
        ++i;
    }

    return Memoization[(i - 1) % MemoizationSize];
}

int Walk(int m, int n)
{
    static int TotalPaths = 0;
    // printf("Walk (%d, %d)\n", m, n);

    if (m == 1 && n == 1)
    {
        ++TotalPaths;
    }

    if (m > 1)
    {
        Walk(m - 1, n);
    }

    if (n > 1)
    {
        Walk(m, n - 1);        
    }

    return TotalPaths;
}

int WalkNotTraversingPathFully(int m, int n)
{
    static int TotalPaths = 0;
    // printf("Walk (%d, %d)\n", m, n);

    if (m == 1 || n == 1)
    {
        ++TotalPaths;
        return -1;
    }

    if (m > 1)
    {
        WalkNotTraversingPathFully(m - 1, n);
    }

    if (n > 1)
    {
        WalkNotTraversingPathFully(m, n - 1);        
    }

    return TotalPaths;
}

int main()
{
    // ReSharper disable once CppTooWideScope
    int NumStairs = 45;
    printf("%d stairs\n", NumStairs);
    
    // {
    //     ScopedWallStopwatch Stopwatch; // ~35'000'000us
    //     printf(">> NoRecursiveBruteForce\n");
    //     int Paths = NoRecursiveBruteForce(NumStairs);
    //     printf("%d paths\n", Paths);
    // }
    //
    // {
    //     ScopedWallStopwatch Stopwatch; // ~2'000'000u
    //     printf(">> NoRecursiveWithStaticTable\n");
    //     int Paths = NoRecursiveWithStaticTable(NumStairs);
    //     printf("%d paths\n", Paths);
    // }
    //
    // {
    //     ScopedWallStopwatch Stopwatch; // ~5us
    //     printf(">> NoRecursiveWithMemoization\n");
    //     int Paths = NoRecursiveWithMemoization(NumStairs);
    //     printf("%d paths\n", Paths);
    // }
    //
    // {
    //     ScopedWallStopwatch Stopwatch; // ~0us
    //     printf(">> CalculateFibonacci\n");
    //     int Paths = CalculateFibonacci(NumStairs);
    //     printf("%d paths\n", Paths);
    // }

    {
        ScopedWallStopwatch Stopwatch;
        printf(">> WalkingRobot\n");
        int Paths = Walk(17, 18);
        printf("%d paths\n", Paths);
    }
    
    {
        ScopedWallStopwatch Stopwatch;
        printf(">> WalkNotTraversingPathFully\n");
        int Paths = WalkNotTraversingPathFully(17, 18);
        printf("%d paths\n", Paths);
    }
}
