#include <algorithm>
#include <iterator>
// https://www.quora.com/How-is-STLs-std-make_heap-implemented

void SiftDown(int* Arr, int Size, int ParentIndex)
{
    int LargestValueAtIndex = ParentIndex; // Assume larger is the ParentIndex
    int LeftChildIndex = ParentIndex * 2 + 1;
    int RightChildIndex = ParentIndex * 2 + 2;

    // Is left child larger than parent?
    if (LeftChildIndex < Size && Arr[LeftChildIndex] > Arr[LargestValueAtIndex])
    {
        LargestValueAtIndex = LeftChildIndex;
    }

    // Is right child larger than left?
    if (RightChildIndex < Size && Arr[RightChildIndex] > Arr[LargestValueAtIndex])
    {
        LargestValueAtIndex = RightChildIndex;
    }

    // Should parent index be updated?
    if (ParentIndex != LargestValueAtIndex)
    {
        std::swap(Arr[ParentIndex], Arr[LargestValueAtIndex]);

        // Sift down from the index where it was the node with the largest value
        // useful in cases like
        //                      index
        //      (20)            0,
        //      /  \
        //    50   (57)         1, 2,
        //   /  \    |
        // 47   50   33         3, 4, 5
        // Here when 20 and 57 get swapped, we will end with 20 -> 33 not satisfying
        // the max heap property, so we should start a new SiftDown operation from the new node located at index [2]
        SiftDown(Arr, Size, LargestValueAtIndex);
    }
}

int main()
{
    int Data[]
    {
                20,
        22,             33,
        47, 50,         57
    };

    for (int i = (std::size(Data) - 2) / 2; i >= 0; --i)
    {
        SiftDown(Data, std::size(Data), i);        
    }
    // SiftDown(Data, std::size(Data), 2);
    // SiftDown(Data, std::size(Data), 1);
    // SiftDown(Data, std::size(Data), 0);

    // std::make_heap(Data, Data + std::size(Data));
    // SiftDown(Data, std::size(Data), 2);
    // SiftDown(Data, std::size(Data), 1);
    // SiftDown(Data, std::size(Data), 0);

    return -1;
}
