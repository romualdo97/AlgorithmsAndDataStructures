#include <array>
#include <cstdlib>
#include <cstdio>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <unordered_map>
#include <cassert>
#include <chrono>
#include <fstream>
#include <functional>
#include <vector>

// MOV REG, SRC — Assign: REG = SRC (SRC can be register or immediate).
// ADD REG, SRC — Add: REG = REG + SRC.
// SUB REG, SRC — Subtract: REG = REG - SRC.
// JMP LABEL — Unconditional jump to a label.
// JZ REG, LABEL — Jump if zero: if REG == 0, jump to LABEL.
// JNZ REG, LABEL — Jump if not zero: if REG != 0, jump to LABEL.
// PRINT REG — Output the integer value in REG (print one line).
// HALT — Stop execution immediately.
namespace
{
    /**
     * Identifies each general purpose register
     */
    enum class RegisterId : unsigned int
    {
        R0 = 0,
        R1,
        R2,
        R3,
        Total
    };

    /**
     * Converts an opcode in IvSembly to representation in RomuByteCode
     */
    std::unordered_map<std::string, unsigned char> OpCodeToBinary
    {
        { "MOV", 0 }, { "ADD", 1 }, { "SUB", 2 },
        { "JMP", 3 },  { "JZ", 4 }, { "JNZ", 5 },
        { "PRINT", 6 },
        { "HALT", 7 },
        { "LOAD", 8 },
        { "STORE", 9 }
    };

    /**
     * Converts a register name in IvSembly to representation in RomuByteCode
     */
    std::unordered_map<std::string, RegisterId> RegisterNameToId
    {
        { "R0", RegisterId::R0 }, { "R1", RegisterId::R1 }, { "R2", RegisterId::R2 }, { "R3", RegisterId::R3 }
    };

    constexpr bool IsArchitecturalRegister(const std::string& RegisterName) { return !RegisterName.empty() && RegisterNameToId.contains(RegisterName); }
    constexpr unsigned int AsUint(RegisterId Id) { return static_cast<unsigned int>(Id); }
    constexpr RegisterId AsRegister(unsigned int Id) { return static_cast<RegisterId>(Id); }
}

///////////////////////////////////////////
///////////////////////////////////////////
/// Assembling phase
///////////////////////////////////////////
///////////////////////////////////////////

/**
 * Represents an instruction in binary form for the RomuRuntime
 */
struct InstructionVector
{
    InstructionVector() = default;
    InstructionVector(const unsigned char InOp, const bool InMod, const int InOperand1, const int InOperand2)
        : Op(InOp), Mod(InMod), Operand1(InOperand1), Operand2(InOperand2) {}
    unsigned char Op;
    bool Mod; // 0 if 2nd operand is imm, if reg then 1
    int Operand1;
    int Operand2;
};

/**
 * IvSembly stands for ([Iv]an As[sembly]), utility class that
 * assembles from the assembly representation to a bytecode understandable by RomuRuntime
 */
class IvSemblyToRomuByteCode
{
public:
    explicit IvSemblyToRomuByteCode(std::stringstream& SourceCode)
    {
        // Remove comments and generate label table
        std::string Line;
        unsigned int InstructionAddress = 0;
        std::stringstream CleanSourceCode;
        while (std::getline(SourceCode, Line))
        {
            Line = TrimLineComment(Line);

            // Comment or empty line
            if (CanIgnoreLine(Line))
            {
                continue;
            }
            
            // Get the label name and the address that represents
            if (std::optional<std::string> LabelName = GetLabelName(Line))
            {
                LabelTranslationTable[LabelName.value()] = InstructionAddress;
                continue;
            }

            CleanSourceCode << Line << "\n";
            ++InstructionAddress;            
        }

        #define TRY_TRANSLATE_INSTRUCTION(Name, OpCode, Op1, Op2) \
            if (std::optional<InstructionVector> Name = Get##Name##Instruction(OpCode.value(), Op1, Op2)) \
            { \
                ByteCode.push_back(Name.value()); \
                ++InstructionAddress; \
                continue; \
            } \

        // Traverse again, now without labels and comments
        while (std::getline(CleanSourceCode, Line))
        {
            std::stringstream LineSs(Line);
            std::string Token, Op1, Op2;
            LineSs >> Token;

            std::optional<std::string> OpCode = GetOpCode(Token);
            if (!OpCode.has_value())
            {
                std::cerr << "Error: Unknown OpCode " << Token << std::endl;
                assert(false);
            }

            std::getline(LineSs >> std::ws, Op1, ',');
            std::getline(LineSs >> std::ws, Op2);

            // Translate instructions to byte code
            TRY_TRANSLATE_INSTRUCTION(Jump, OpCode, Op1, Op2)
            TRY_TRANSLATE_INSTRUCTION(ConditionalJump, OpCode, Op1, Op2)
            TRY_TRANSLATE_INSTRUCTION(Binary, OpCode, Op1, Op2)
            TRY_TRANSLATE_INSTRUCTION(Load, OpCode, Op1, Op2)
            TRY_TRANSLATE_INSTRUCTION(Store, OpCode, Op1, Op2)
            TRY_TRANSLATE_INSTRUCTION(Print, OpCode, Op1, Op2)
            TRY_TRANSLATE_INSTRUCTION(Halt, OpCode, Op1, Op2)

            std::cerr << "Error: Invalid instruction " << OpCode.value() << std::endl;
            assert(false);            
        }

        #undef TRY_TRANSLATE_INSTRUCTION
    }

    [[nodiscard]] std::vector<InstructionVector> ToByteCode() const { return ByteCode; }
    
private:
    void CheckValidLabel(const std::string& Operand) const
    {
        if (!LabelTranslationTable.contains(Operand))
        {
            std::cerr << "Error: Unknown Label " << Operand << std::endl;
            assert(false);
        }
    }

    static bool CanIgnoreLine(const std::string& Line)
    {
        const size_t First = Line.find_first_not_of(" \t\r\n");
        if (First == std::string::npos) return true;
        return false;
    }

    static std::optional<std::string> GetLabelName(const std::string& Token)
    {
        if (!Token.empty() && Token.find_first_of(':') != std::string::npos)
        {
            return Token.substr(0, Token.size() - 1);
        }

        return std::nullopt;
    }

    static std::optional<std::string> GetOpCode(const std::string& Token)
    {
        if (OpCodeToBinary.contains(Token))
        {
            return Token;
        }

        return std::nullopt;
    }

    static std::string TrimCopy(const std::string& In)
    {
        const size_t Start = In.find_first_not_of(" \t\r\n");
        if (Start == std::string::npos) return {};
        const size_t End = In.find_last_not_of(" \t\r\n");
        return In.substr(Start, End - Start + 1);
    }

    static std::string TrimLineComment(const std::string& In)
    {
        const size_t CommentStart = In.find_first_of(';');
        const std::string WithoutComment = (CommentStart == std::string::npos) ? In : In.substr(0, CommentStart);
        return TrimCopy(WithoutComment);
    }

    static bool ParseAddressOperand(const std::string& Operand, unsigned int& OutAddress, bool& OutIsIndirectAddressing)
    {
        const std::string Clean = TrimCopy(Operand);
        if (Clean.size() < 3 || Clean.front() != '[' || Clean.back() != ']')
        {
            return false;
        }

        const std::string Inner = Clean.substr(1, Clean.size() - 2);
        if (IsArchitecturalRegister(Inner))
        {
            OutAddress = static_cast<unsigned int>(RegisterNameToId[Inner]);
            OutIsIndirectAddressing = true;
            return true;
        }

        char* EndPtr = nullptr;
        const long Value = std::strtol(Inner.c_str(), &EndPtr, 10);
        if (EndPtr == Inner.c_str() || *EndPtr != '\0' || Value < 0)
        {
            return false;
        }

        OutAddress = static_cast<unsigned int>(Value);
        OutIsIndirectAddressing = false;
        return true;
    }

    std::optional<InstructionVector> GetJumpInstruction(
        const std::string& OpCode,
        const std::string& Op1,
        const std::string& Op2)
    {
        if (OpCode != "JMP")
        {
            return std::nullopt;
        }

        CheckValidLabel(Op1);
        return InstructionVector{
            OpCodeToBinary[OpCode],
            false,
            static_cast<int>(LabelTranslationTable[Op1]),
            0
        };
    }

    std::optional<InstructionVector> GetConditionalJumpInstruction(
        const std::string& OpCode,
        const std::string& Op1,
        const std::string& Op2)
    {
        if (OpCode != "JN" && OpCode != "JNZ")
        {
            return std::nullopt;
        }
        
        CheckValidLabel(Op2);
        return InstructionVector{
            OpCodeToBinary[OpCode],
            false,
            static_cast<int>(RegisterNameToId[Op1]),
            static_cast<int>(LabelTranslationTable[Op2])
        };
    }

    std::optional<InstructionVector> GetBinaryInstruction(
        const std::string& OpCode,
        const std::string& Op1,
        const std::string& Op2)
    {
        if (OpCode != "MOV" && OpCode != "ADD" && OpCode != "SUB")
        {
            return std::nullopt;
        }

        bool bIsArchitecturalRegister = IsArchitecturalRegister(Op2);
        return InstructionVector{
            OpCodeToBinary[OpCode],
            bIsArchitecturalRegister,
            static_cast<int>(RegisterNameToId[Op1]),
            bIsArchitecturalRegister ? static_cast<int>(RegisterNameToId[Op2]) : std::atoi(Op2.c_str()) 
        };
    }

    std::optional<InstructionVector> GetLoadInstruction(
        const std::string& OpCode,
        const std::string& Op1,
        const std::string& Op2)
    {
        if (OpCode != "LOAD")
        {
            return std::nullopt;
        }

        unsigned int Address = 0;
        bool bIsIndirectAddressing = false;
        if (!IsArchitecturalRegister(Op1) || !ParseAddressOperand(Op2, Address, bIsIndirectAddressing))
        {
            return std::nullopt;
        }

        return InstructionVector{
            OpCodeToBinary[OpCode],
            bIsIndirectAddressing,
            static_cast<int>(RegisterNameToId[Op1]),
            static_cast<int>(Address)
        };
    }

    std::optional<InstructionVector> GetStoreInstruction(
        const std::string& OpCode,
        const std::string& Op1,
        const std::string& Op2)
    {
        if (OpCode != "STORE")
        {
            return std::nullopt;
        }

        unsigned int Address = 0;
        bool bIsIndirectAddressing = false;
        if (!ParseAddressOperand(Op1, Address, bIsIndirectAddressing) || !IsArchitecturalRegister(Op2))
        {
            return std::nullopt;
        }

        return InstructionVector{
            OpCodeToBinary[OpCode],
            bIsIndirectAddressing,
            static_cast<int>(Address),
            static_cast<int>(RegisterNameToId[Op2])
        };
    }
        
    std::optional<InstructionVector> GetPrintInstruction(
        const std::string& OpCode,
        const std::string& Op1,
        const std::string& Op2)
    {
        return OpCode == "PRINT" ? std::optional(InstructionVector{ OpCodeToBinary[OpCode], false, static_cast<int>(RegisterNameToId[Op1]), 0}) : std::nullopt;
    }
    
    std::optional<InstructionVector> GetHaltInstruction(
        const std::string& OpCode,
        const std::string& Op1,
        const std::string& Op2)
    {
        return OpCode == "HALT" ? std::optional(InstructionVector{OpCodeToBinary[OpCode], false, 0, 0}) : std::nullopt;
    }

private:
    std::vector<InstructionVector> ByteCode {};
    std::unordered_map<std::string, unsigned int> LabelTranslationTable;
};

///////////////////////////////////////////
///////////////////////////////////////////
/// The execution environment
///////////////////////////////////////////
///////////////////////////////////////////

/**
 * Simulates main memory using a vector.
 * Provides load and store operations for memory access.
 */
class MainMemory
{
public:
    explicit MainMemory(size_t Size = 1024) : Memory(Size, 0) {}

    /**
     * Load a value from the specified memory address.
     * @param Address The memory address to read from.
     * @return The value stored at the address.
     */
    unsigned int Load(size_t Address) const
    {
        assert(Address < Memory.size() && "Memory address out of bounds");
        return Memory[Address];
    }

    /**
     * Store a value at the specified memory address.
     * @param Address The memory address to write to.
     * @param Value The value to store.
     */
    void Store(size_t Address, int Value)
    {
        assert(Address < Memory.size() && "Memory address out of bounds");
        Memory[Address] = Value;
    }

    [[nodiscard]] size_t Size() const { return Memory.size(); }

private:
    std::vector<int> Memory;
};

/*
 * Represents the runtime execution environment available for IvSembly instructions.
 */
struct ExecutionEnvironment
{
    std::array<int, AsUint(RegisterId::Total)>                                              Registers { }; // General purpose registers
    std::vector<InstructionVector>                                                          InstructionsROM { }; // Read-Only-Memory for instructions
    unsigned int                                                                            Instruction { 0 }; // Instruction pointer register
    MainMemory                                                                               Memory { };
    InstructionVector FetchInstruction()                                                    { const InstructionVector Out = InstructionsROM[Instruction]; ++Instruction; return Out; }
    const std::function<bool(int, int)>& DecodeInstruction(const InstructionVector& In)     { return OperationsTable[In.Op].OperationHandlers[In.Mod]; }
    
private:
    struct ExecutionEntry
    {
        std::array<std::function<bool(int, int)>, 2> OperationHandlers;
    };

    #define REGISTER_HANDLERS(Predicate1, Predicate2) \
        ExecutionEntry{ [this](int InOperand1, int InOperand2) -> bool { Predicate1; }, [this](int InOperand1, int InOperand2) -> bool { Predicate2; } }

    std::array<ExecutionEntry, 10> OperationsTable
    {
        /*MOV*/     REGISTER_HANDLERS(
                        Registers[InOperand1] = InOperand2; return true,
                        Registers[InOperand1] = Registers[InOperand2]; return true
                    ),
        /*ADD*/     REGISTER_HANDLERS(
                        Registers[InOperand1] += InOperand2; return true,
                        Registers[InOperand1] += Registers[InOperand2]; return true
                    ),
        /*SUB*/     REGISTER_HANDLERS(
                        Registers[InOperand1] -= InOperand2; return true,
                        Registers[InOperand1] -= Registers[InOperand2]; return true
                    ),
                    REGISTER_HANDLERS(
        /*JMP*/         Instruction = InOperand1; return true,
                        return false // Invalid operation
                    ),
        /*JZ*/      REGISTER_HANDLERS(
                        if (Registers[InOperand1] == 0) Instruction = InOperand2; return true,
                        return false // Invalid operation
                    ),    
        /*JNZ*/     REGISTER_HANDLERS(
                        if (Registers[InOperand1] != 0) Instruction = InOperand2; return true,
                        return false // Invalid operation
                    ),
                    REGISTER_HANDLERS(
        /*PRINT*/       std::printf("%d\n", Registers[InOperand1]); return true,
                        return false // Invalid operation
                    ),
                    REGISTER_HANDLERS(
        /*HALT*/        return false,
                        return false // Invalid operation
                    ),
        /*LOAD*/    REGISTER_HANDLERS(
                        Registers[InOperand1] = Memory.Load(static_cast<size_t>(InOperand2)); return true,
                        Registers[InOperand1] = Memory.Load(static_cast<size_t>(Registers[InOperand2])); return true
                    ),
        /*STORE*/   REGISTER_HANDLERS(
                        Memory.Store(static_cast<size_t>(InOperand1), Registers[InOperand2]); return true,
                        Memory.Store(static_cast<size_t>(Registers[InOperand1]), Registers[InOperand2]); return true
                    )
    };

    #undef REGISTER_HANDLERS
};

///////////////////////////////////////////
///////////////////////////////////////////
/// Execution phase utils
///////////////////////////////////////////
///////////////////////////////////////////

class ClockWatch
{
public:
    explicit ClockWatch(std::string&& InLabel)
        : Label(InLabel)
        , StartTime(std::chrono::high_resolution_clock::now())
    {
        
    }

    ~ClockWatch()
    {
        const auto EndTime = std::chrono::high_resolution_clock::now();
        const auto Duration = std::chrono::duration_cast<std::chrono::microseconds>(EndTime - StartTime);
        std::cout << "" << Label << " took "  << Duration.count() << " microseconds (" 
                  << static_cast<double>(Duration.count()) / 1000 << " ms)" << std::endl;
    }

private:
    std::string Label;
    std::chrono::time_point<std::chrono::steady_clock> StartTime;
};

class RomuRuntime
{
public:
    explicit RomuRuntime(std::vector<InstructionVector>&& Code)
    {
        Environment.InstructionsROM = std::move(Code);
    }
    
    [[nodiscard]] bool Next()
    {
        const InstructionVector Instruction = Environment.FetchInstruction();
        const std::function<bool(int, int)>& Operation = Environment.DecodeInstruction(Instruction);
        return Operation(Instruction.Operand1, Instruction.Operand2);
    }
    
private:
    
    /*
     * Execution environment state
     */
    ExecutionEnvironment Environment;
};

void Run(std::stringstream&& SourceCode)
{
    // Compile into bytecode then unload the assembler buffers
    std::vector<InstructionVector> ByteCode;
    {
        ClockWatch Watch{"IvSembly compilation"};
        const IvSemblyToRomuByteCode Assembler{ SourceCode };
        ByteCode = Assembler.ToByteCode();
    }

    // Load the runtime and start execution
    {
        ClockWatch Watch{"ByteCode execution"};
        RomuRuntime Runtime{ std::move(ByteCode) };
        while (Runtime.Next()) {}
    }
}

int main(int ArgCount, char* ArgValues[])
{
    if (ArgCount < 2)
    {
        std::cerr << "Usage: " << ArgValues[0] << " <filename.txt>" << std::endl;
        return 1;
    }
    
    std::stringstream SourceCode;
    {
        std::ifstream SourceCodeFile(ArgValues[1]);
        if (!SourceCodeFile.is_open()) {
            std::cerr << "Error: Could not open file " << ArgValues[1] << std::endl;
            return 1;
        }

        SourceCode << SourceCodeFile.rdbuf(); // Efficiently read the file buffer into the stream
        SourceCodeFile.close();
    }

    Run(std::move(SourceCode));
    return 0;
}