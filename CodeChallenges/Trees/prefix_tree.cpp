// The Trie Data Structure (Prefix Tree)
// https://www.youtube.com/watch?v=3CbFFVHQrk4&t=435s
#include <iostream>
#include <memory>
#include <string>

#define NUM_CHILDREN 256

// a.k.a trie node or prefix tree node
struct Node
{
public:
    Node()
        : Children()
        , bIsTerminal(false)
    {
        
    }
    ~Node() = default;

    bool Insert(const std::string& Text)
    {
        Node* NodePtr = this;
        for (const char& Character : Text)
        {
            const unsigned char UnsignedChar = static_cast<unsigned char>(Character);
            if (NodePtr->Children[UnsignedChar] == nullptr)
            {
                NodePtr->Children[UnsignedChar] = std::make_unique<Node>();
            }

            // Update the root to be the next level of the tree
            NodePtr = &(*NodePtr->Children[UnsignedChar]);
        }
        
        if (NodePtr->bIsTerminal)
        {
            return false;
        }

        // Mark node as a terminal node
        NodePtr->bIsTerminal = true;
        return true;
    }

    void Print() const
    {
        PrintRecursive(this, "");
    }
    
private:
    static void PrintRecursive(const Node* InNode, const std::string& Prefix)
    {
        if (InNode->bIsTerminal)
        {
            std::cout << Prefix << std::endl;
        }

        for (int i = 0; i < NUM_CHILDREN; ++i)
        {
            if (InNode->Children[i] != nullptr)
            {
                std::string NewPrefix = Prefix; // Copy
                NewPrefix += i; // Append new char
                PrintRecursive(&(*InNode->Children[i]), NewPrefix);
            }
        }
    }

private:
    std::unique_ptr<Node> Children[NUM_CHILDREN];
    bool bIsTerminal;
};

int main()
{
    Node PrefixTree;
    PrefixTree.Insert("Hello");
    PrefixTree.Insert("Hello World");
    PrefixTree.Insert("World");
    PrefixTree.Insert("Cat");
    PrefixTree.Insert("Kat");
    PrefixTree.Insert("Albedo");
    PrefixTree.Print();

    std::integer_sequence<unsigned, 1, 2, 3, 4>;
}