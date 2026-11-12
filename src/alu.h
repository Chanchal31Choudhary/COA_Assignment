// =====================================================================
//  alu.h  --  RISC201 ALU class
//  OWNER : Chanchal (Member B)
//  ROLE  : All arithmetic, shift and logic operations of the CPU.
//          Two adder algorithms  : ripple-carry, carry-lookahead (Kogge-Stone)
//          Two multiplier algos  : shift-and-add, Booth radix-2
//          Shifts use a barrel shifter model (4 mux levels).
//          The ALU keeps statistics (op counts, modelled gate delay).
// =====================================================================
#pragma once
#include <cstdint>
#include <string>
#include "fp16.h"

enum class AluOp : uint8_t { NONE = 0, ADD, SUB, AND, OR, XOR, NOT, SHL, SHR, SAR, MUL, DIV, FDIV, PASSA, PASSB, COUNT };
enum class AddAlgo { Ripple, Lookahead, CarrySelect, BrentKung };
enum class MulAlgo { ShiftAdd, Booth, Wallace };

struct AluFlags { bool z = false, n = false, c = false, v = false; };
struct AluResult { uint16_t value = 0; AluFlags f; };

const char* aluOpName(AluOp op);

class ALU {
public:
    ALU();
    AluResult execute(AluOp op, uint16_t a, uint16_t b);

    void setAddAlgo(AddAlgo a) { addAlgo_ = a; }
    void setMulAlgo(MulAlgo m) { mulAlgo_ = m; }
    AddAlgo addAlgo() const { return addAlgo_; }
    MulAlgo mulAlgo() const { return mulAlgo_; }
    void resetStats();

    // ---- standalone algorithms (also used by the comparison tool) ----
    static uint16_t addRipple(uint16_t a, uint16_t b, bool cin, bool& cout, bool& c15);
    static uint16_t addLookahead(uint16_t a, uint16_t b, bool cin, bool& cout, bool& c15);
    static uint16_t addCarrySelect(uint16_t a, uint16_t b, bool cin, bool& cout, bool& c15);
    static uint16_t addBrentKung(uint16_t a, uint16_t b, bool cin, bool& cout, bool& c15);
    static uint16_t mulShiftAdd(uint16_t a, uint16_t b, int& addOps);
    static uint16_t mulBooth(uint16_t a, uint16_t b, int& addOps);
    static uint16_t mulWallace(uint16_t a, uint16_t b, int& addOps);

    // modelled gate delays (unit = one 2-input gate)
    static int addDelay(AddAlgo a) { switch (a) { case AddAlgo::Ripple: return 32; case AddAlgo::Lookahead: return 10; case AddAlgo::CarrySelect: return 12; case AddAlgo::BrentKung: return 12; } return 32; }

    // statistics
    uint64_t opCount[(int)AluOp::COUNT] = {0};
    uint64_t totalOps = 0;
    uint64_t delayUnits = 0;     // modelled gate delay summed over all operations
    uint64_t mulAddOps = 0;      // additions/subtractions used by the multiplier
    AluFlags lastFlags;

    std::string describe() const;

private:
    AddAlgo addAlgo_ = AddAlgo::Lookahead;
    MulAlgo mulAlgo_ = MulAlgo::Booth;
    uint16_t add(uint16_t a, uint16_t b, bool cin, AluFlags& f);
};
