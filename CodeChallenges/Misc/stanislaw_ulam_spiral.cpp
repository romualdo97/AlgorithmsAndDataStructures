#include <cassert>
#include <cmath>
#include <cstdint>
#include <format>
#include <iostream>
#include <sstream>
#include <vector>

/**
 * A N*N square lattice filled with integer numbers of the Stanislaw Ulam Spiral  
 */
class UlamLattice
{
public:
    explicit UlamLattice(const size_t InSquareSide)
        : SquareSide(InSquareSide)
        , Grid(SquareSide * SquareSide)
   {
        using Vec2 = std::pair<int32_t, int32_t>;

        // Iterate the grid in a clockwise spiral starting from the center
        Vec2 Direction{1, 0};
        Vec2 CurrentPosition{SquareSide / 2, SquareSide / 2};
        for (size_t Count = 1; Count <= Grid.size(); ++Count)
        {
            Grid[CurrentPosition.second * SquareSide + CurrentPosition.first] = Count;

            // Update pos for next frame
            CurrentPosition.first += Direction.first;
            CurrentPosition.second += Direction.second;

            // Rotate direction vector 90 deg clockwise
            const Vec2 NewDirection{ -Direction.second, Direction.first };

            // From position at next frame check if direction should rotate 90 deg clockwise
            if (const Vec2 NewPosition{ CurrentPosition.first + NewDirection.first, CurrentPosition.second + NewDirection.second };
                Grid[NewPosition.second * SquareSide + NewPosition.first] == 0) // Direction can only change new pos won't collide with previously walked steps
            {
                Direction = NewDirection;
            }
        }
    }

    [[nodiscard]] std::string ToString() const
    {
        std::stringstream StringStream;
        for (size_t Index = 0; Index < SquareSide * SquareSide; ++Index)
        {
            if (Index > 0 &&
                Index % SquareSide == 0)
            {
                StringStream << std::endl;
            }

            // StringStream << std::format("{:04}", Grid[Index]) << " "; // Print the numbers in the spiral
            StringStream << (IsPrime(Grid[Index]) ? "*" : ".") << " "; // Print as bitmap
        }
        return StringStream.str();
    }

private:
    static bool IsPrime(const uint8_t Num) // Check if prime using trial division method as we expect small numbers
    {
        if (Num <= 1)
        {
            return false;
        }

        if (Num <= 3)
        {
            return true;
        }

        if (Num % 2 == 0 || Num % 3 == 0)
        {
            return false;
        }

        // Check if the square root is divisible by an odd number
        auto FlooredSquareRoot = static_cast<uint32_t>(std::sqrt(Num));

        // Increments by 6 to check both i (like 5) and i + 2 (like 7) in one pass.
        for (uint32_t i = 5; i <= FlooredSquareRoot; i += 6)
        {
            if (Num % i == 0 || Num % (i + 2) == 0)
            {
                return false;
            }
        }

        return true;
    }
    
private:
    size_t SquareSide;
    std::vector<uint32_t> Grid;
};

std::ostream& operator<<(std::ostream& Out, const UlamLattice& Obj)
{
    return Out << Obj.ToString();
}

int main()
{
    size_t SquareSide = 0;
    std::cin >> SquareSide;
    assert(
        SquareSide >= 1 &&
        SquareSide <= 50 &&
        "Side of square grid must be in range [0, 50]");
    assert(
        (SquareSide & 1) == 1 &&
        "Side of square grid must be an odd number (because I'm a lazy guy)");

    const UlamLattice UlamSpiral(SquareSide);
    std::stringstream StringStream;
    std::cout << UlamSpiral << std::endl;
    
    return 0;
}
