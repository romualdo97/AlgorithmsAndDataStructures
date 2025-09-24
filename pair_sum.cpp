#include <cstdint>
#include <cstdio>

int main()
{
    int nums[] = { 1, 2, 3, 4, 5, 6 };
    int expected = 7;

    size_t size = sizeof(nums) / sizeof(int);
    if (expected == 0 ||
        expected > (nums[size - 1] + nums[size - 2]))
    {
        return -1;
    }

    for (int i = 0; i < size; ++i)
    {
        int first = nums[i];
        for (int j = i + 1; j < size; ++j)
        {
            int second = nums[j];
            int pair = first + second;
            printf("%d+%d=%d\n", first, second, pair);
            if (pair == expected)
            {
                return pair;
            }
        }
    }

    return -1;
}