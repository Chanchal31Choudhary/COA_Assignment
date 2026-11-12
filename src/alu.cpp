// alu.cpp -- OWNER: Chanchal (Member B)
#include "alu.h"
#include "exceptions.h"
#include <cstdint>

const char* aluOpName(AluOp op) {
    static const char* n[] = {"-", "ADD", "SUB", "AND", "OR", "XOR", "NOT", "SHL", "SHR", "SAR", "MUL", "DIV", "FDIV", "PASSA", "PASSB"};
    return n[(int)op];
}

ALU::ALU() {}

void ALU::resetStats() {
    for (auto& c : opCount) c = 0;
    totalOps = delayUnits = mulAddOps = 0;
    lastFlags = AluFlags();
}

// ---------------- adders ---------------------------------------------------
// Ripple carry: carry travels bit by bit (16 stages, 2 gate delays each)
uint16_t ALU::addRipple(uint16_t a, uint16_t b, bool cin, bool& cout, bool& c15) {
    uint16_t s = 0;
    bool c = cin;
    for (int i = 0; i < 16; i++) {
        bool ai = (a >> i) & 1, bi = (b >> i) & 1;
        if (i == 15) c15 = c;                       // carry INTO the sign bit
        bool sum = ai ^ bi ^ c;
        c = (ai & bi) | (c & (ai ^ bi));
        s |= (uint16_t)(sum << i);
    }
    cout = c;
    return s;
}

// Carry lookahead using a parallel-prefix (Kogge-Stone) network: log2(16)=4 levels
uint16_t ALU::addLookahead(uint16_t a, uint16_t b, bool cin, bool& cout, bool& c15) {
    bool p[16], g[16], G[16], P[16];
    for (int i = 0; i < 16; i++) {
        bool ai = (a >> i) & 1, bi = (b >> i) & 1;
        p[i] = ai ^ bi;
        g[i] = ai & bi;
        G[i] = g[i];
        P[i] = p[i];
    }
    for (int d = 1; d < 16; d <<= 1) {
        bool G2[16], P2[16];
        for (int i = 0; i < 16; i++) { G2[i] = G[i]; P2[i] = P[i]; }
        for (int i = d; i < 16; i++) {
            G2[i] = G[i] | (P[i] & G[i - d]);
            P2[i] = P[i] & P[i - d];
        }
        for (int i = 0; i < 16; i++) { G[i] = G2[i]; P[i] = P2[i]; }
    }
    bool c[17];
    c[0] = cin;
    for (int i = 0; i < 16; i++) c[i + 1] = G[i] | (P[i] & cin);
    uint16_t s = 0;
    for (int i = 0; i < 16; i++) s |= (uint16_t)((p[i] ^ c[i]) << i);
    cout = c[16];
    c15 = c[15];
    return s;
}

// Carry-select: lower 8 bits ripple; upper half is computed for carry-in 0 and 1.
uint16_t ALU::addCarrySelect(uint16_t a, uint16_t b, bool cin, bool& cout, bool& c15) {
    bool c = cin; uint16_t out = 0;
    for (int i=0;i<8;i++) { bool x=(a>>i)&1, y=(b>>i)&1; bool z=x^y^c; c=(x&y)|(c&(x^y)); out|=(uint16_t)z<<i; }
    auto upper = [&](bool carry, bool& co) { uint16_t v=0; bool cc=carry; for(int i=8;i<16;i++){bool x=(a>>i)&1,y=(b>>i)&1; bool z=x^y^cc; cc=(x&y)|(cc&(x^y)); v|=(uint16_t)z<<i;} co=cc; return v; };
    bool c0=false,c1=false; uint16_t s0=upper(false,c0), s1=upper(true,c1); uint16_t hi=c?s1:s0; cout=c?c1:c0; bool cc=(c); for(int i=8;i<15;i++){bool x=(a>>i)&1,y=(b>>i)&1;cc=(x&y)|(cc&(x^y));} c15=cc; return out|hi;
}

// Brent-Kung parallel-prefix network (up-sweep and down-sweep).
uint16_t ALU::addBrentKung(uint16_t a, uint16_t b, bool cin, bool& cout, bool& c15) {
    bool p[16], g[16];
    for (int i=0;i<16;i++) { bool x=(a>>i)&1, y=(b>>i)&1; p[i]=x^y; g[i]=x&y; }
    // Up-sweep: compute group generate/propagate at tree roots.
    for (int d=1; d<16; d*=2)
        for (int i=2*d-1; i<16; i+=2*d) { g[i]=g[i] | (p[i]&g[i-d]); p[i]=p[i]&p[i-d]; }
    // Down-sweep: fill the missing prefixes between the tree roots.
    for (int d=8; d>=1; d/=2)
        for (int i=3*d-1; i<16; i+=2*d) { g[i]=g[i] | (p[i]&g[i-d]); p[i]=p[i]&p[i-d]; }
    uint16_t sum=0; bool carries[17]{}; carries[0]=cin;
    for (int i=0;i<16;i++) carries[i+1]=g[i] | (p[i]&cin);
    c15=carries[15]; cout=carries[16];
    for (int i=0;i<16;i++) sum |= (uint16_t)( ((a^b)>>i & 1) ^ carries[i] ) << i;
    return sum;
}

// Wallace-style carry-save reduction of 16x16 partial-product columns.
uint16_t ALU::mulWallace(uint16_t a, uint16_t b, int& addOps) {
    bool cols[32][32]{}; int n[32]{};
    for(int i=0;i<16;i++) if((b>>i)&1) for(int j=0;j<16;j++) if((a>>j)&1) cols[i+j][n[i+j]++]=true;
    addOps=0;
    bool changed=true;
    while(changed){ changed=false; for(int k=0;k<31;k++) while(n[k]>=3){ bool x=cols[k][--n[k]], y=cols[k][--n[k]], z=cols[k][--n[k]]; bool sum=x^y^z, carry=(x&y)|(x&z)|(y&z); cols[k][n[k]++]=sum; cols[k+1][n[k+1]++]=carry; addOps++; changed=true; } }
    uint32_t row0=0,row1=0; for(int k=0;k<16;k++){if(n[k]>0&&cols[k][0])row0|=1u<<k;if(n[k]>1&&cols[k][1])row1|=1u<<k;}
    return (uint16_t)(row0+row1);
}

uint16_t ALU::add(uint16_t a, uint16_t b, bool cin, AluFlags& f) {
    bool cout = false, c15 = false;
    uint16_t s;
    switch(addAlgo_) { case AddAlgo::Ripple: s=addRipple(a,b,cin,cout,c15); break; case AddAlgo::Lookahead: s=addLookahead(a,b,cin,cout,c15); break; case AddAlgo::CarrySelect: s=addCarrySelect(a,b,cin,cout,c15); break; default: s=addBrentKung(a,b,cin,cout,c15); break; }
    f.c = cout;
    f.v = cout ^ c15;                 // signed overflow = carry into MSB xor carry out
    delayUnits += addDelay(addAlgo_);
    return s;
}

// ---------------- multipliers ---------------------------------------------
uint16_t ALU::mulShiftAdd(uint16_t a, uint16_t b, int& addOps) {
    uint16_t acc = 0;
    addOps = 0;
    for (int i = 0; i < 16; i++)
        if ((b >> i) & 1) { acc = (uint16_t)(acc + (a << i)); addOps++; }
    return acc;
}

// Booth radix-2: adds/subtracts only where the multiplier bit pattern changes
uint16_t ALU::mulBooth(uint16_t a, uint16_t b, int& addOps) {
    int32_t A = 0;
    int32_t M = (int16_t)a;
    uint32_t Q = b;
    int q1 = 0;
    addOps = 0;
    for (int i = 0; i < 16; i++) {
        int q0 = Q & 1;
        if (q0 == 0 && q1 == 1) { A += M; addOps++; }
        else if (q0 == 1 && q1 == 0) { A -= M; addOps++; }
        q1 = Q & 1;
        Q = ((Q >> 1) | ((uint32_t)(A & 1) << 15)) & 0xFFFF;
        A >>= 1;                      // arithmetic shift right
    }
    return (uint16_t)(Q & 0xFFFF);
}

// ---------------- main entry ------------------------------------------------
AluResult ALU::execute(AluOp op, uint16_t a, uint16_t b) {
    AluResult r;
    AluFlags f;
    uint16_t v = 0;
    switch (op) {
        case AluOp::ADD:  v = add(a, b, false, f); break;
        case AluOp::SUB:  v = add(a, (uint16_t)~b, true, f); break;   // a + ~b + 1
        case AluOp::AND:  v = a & b; delayUnits += 1; break;
        case AluOp::OR:   v = a | b; delayUnits += 1; break;
        case AluOp::XOR:  v = a ^ b; delayUnits += 1; break;
        case AluOp::NOT:  v = (uint16_t)~a; delayUnits += 1; break;
        case AluOp::SHL:  v = (uint16_t)(a << (b & 15)); delayUnits += 4; break;   // barrel shifter
        case AluOp::SHR:  v = (uint16_t)(a >> (b & 15)); delayUnits += 4; break;
        case AluOp::SAR:  v = (uint16_t)(((int16_t)a) >> (b & 15)); delayUnits += 4; break;
        case AluOp::DIV: {
            const int32_t dividend = static_cast<int16_t>(a);
            const int32_t divisor = static_cast<int16_t>(b);
            if (divisor == 0)
                throw SimException(ExKind::DivideByZero, "integer division by zero");
            // Signed two's-complement division, truncating toward zero. The
            // 16-bit architectural result wraps for -32768 / -1.
            v = static_cast<uint16_t>(dividend / divisor);
            delayUnits += 16; // simple modelled restoring-divider cost
            break;
        }
        case AluOp::FDIV:
            // Registers contain raw IEEE-754 binary16 encodings.
            v = FP16::div(a, b);
            delayUnits += 16; // modelled FP divider cost; not a timing claim
            break;
        case AluOp::MUL: {
            int adds = 0;
            v = (mulAlgo_ == MulAlgo::Booth) ? mulBooth(a, b, adds) : (mulAlgo_ == MulAlgo::Wallace ? mulWallace(a,b,adds) : mulShiftAdd(a, b, adds));
            mulAddOps += adds;
            delayUnits += 16 + adds * addDelay(addAlgo_);
            break;
        }
        case AluOp::PASSA: v = a; break;
        case AluOp::PASSB: v = b; break;
        default: break;
    }
    f.z = (v == 0);
    f.n = (v & 0x8000) != 0;
    r.value = v;
    r.f = f;
    lastFlags = f;
    opCount[(int)op]++;
    totalOps++;
    return r;
}

std::string ALU::describe() const {
    std::string s = "adder = ";
    switch(addAlgo_) { case AddAlgo::Ripple: s += "ripple-carry"; break; case AddAlgo::Lookahead: s += "Kogge-Stone"; break; case AddAlgo::CarrySelect: s += "carry-select"; break; case AddAlgo::BrentKung: s += "Brent-Kung"; break; }
    s += ", multiplier = ";
    s += (mulAlgo_ == MulAlgo::Booth) ? "Booth radix-2" : (mulAlgo_ == MulAlgo::Wallace ? "Wallace tree" : "shift-and-add");
    s += ", shifter = barrel (4 levels)";
    return s;
}
