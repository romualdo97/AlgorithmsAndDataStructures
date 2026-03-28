#include <algorithm>
#include <iterator>

int main()
{
    int Data[]{ 20, 33, 12, 53, 24, 65, 23, 4, 53, 1 };

    // Find the biggest element in a buffer of k elements
    for (int k = std::size(Data); k >= 2; --k)
    {
        // Find the biggest element and move it to last position of the buffer
        for (int i = 0; i < k - 1; ++i) // k - 1 to avoid null checking when comparing BeforeLastIndex and LastIndex
        {
            if (Data[i] > Data[i + 1])
            {
                std::swap(Data[i], Data[i + 1]);    
            }
        }
    }

    return -1;
}
