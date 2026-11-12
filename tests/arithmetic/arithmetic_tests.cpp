#include "alu.h"
#include "fp16.h"
#include "exceptions.h"
#include <iostream>
#include <random>
#include <cmath>
#include <limits>
#include <cstdint>

int main(){ std::mt19937 rng(0xC0A2026); int checks=0, fails=0;
 auto check=[&](bool ok,const char* what){++checks;if(!ok){++fails;if(fails<12)std::cerr<<"FAIL: "<<what<<"\n";}};
 for(int i=0;i<250000;i++){
   uint16_t a=(uint16_t)rng(),b=(uint16_t)rng(); bool c0,c1,c2,c3,d0,d1,d2,d3;
   uint16_t ref=ALU::addRipple(a,b,false,c0,d0);
   check(ALU::addLookahead(a,b,false,c1,d1)==ref && c1==c0 && d1==d0,"Kogge-Stone adder");
   check(ALU::addCarrySelect(a,b,false,c2,d2)==ref && c2==c0 && d2==d0,"Carry Select adder");
   check(ALU::addBrentKung(a,b,false,c3,d3)==ref && c3==c0 && d3==d0,"Brent-Kung adder");
   bool cin=(rng()&1)!=0; bool rco,rc15,aco,ac15,bco,bc15,cco,cc15;
   uint16_t cinRef=ALU::addRipple(a,b,cin,rco,rc15);
   check(ALU::addLookahead(a,b,cin,aco,ac15)==cinRef && aco==rco && ac15==rc15,"Kogge-Stone carry-in");
   check(ALU::addCarrySelect(a,b,cin,bco,bc15)==cinRef && bco==rco && bc15==rc15,"Carry Select carry-in");
   check(ALU::addBrentKung(a,b,cin,cco,cc15)==cinRef && cco==rco && cc15==rc15,"Brent-Kung carry-in");
   int ops=0; uint16_t w=ALU::mulWallace(a,b,ops);
   check(w==(uint16_t)((uint32_t)a*b),"Wallace multiplier");
 }
 // FP16 round-trip all finite encodings (NaNs are canonicalized on conversion).
 for(uint32_t h=0;h<65536;h++){
   uint16_t x=(uint16_t)h; float f=FP16::toFloat(x); uint16_t y=FP16::fromFloat(f);
   if((x&0x7c00)==0x7c00 && (x&0x03ff)) check((y&0x7c00)==0x7c00 && (y&0x03ff),"FP16 NaN round-trip");
   else check(y==x,"FP16 representation round-trip");
 }
 for(int i=0;i<150000;i++){
   uint16_t a=(uint16_t)rng(),b=(uint16_t)rng(); float x=FP16::toFloat(a),y=FP16::toFloat(b);
   auto equivalent=[](uint16_t p,uint16_t q){ bool pn=(p&0x7c00)==0x7c00&&(p&0x03ff), qn=(q&0x7c00)==0x7c00&&(q&0x03ff); return (pn&&qn)||p==q; };
   check(equivalent(FP16::add(a,b),FP16::fromFloat(x+y)),"FP16 add");
   check(equivalent(FP16::sub(a,b),FP16::fromFloat(x-y)),"FP16 subtract");
   check(equivalent(FP16::mul(a,b),FP16::fromFloat(x*y)),"FP16 multiply");
   if (y != 0.0f) {
     check(equivalent(FP16::div(a,b),FP16::fromFloat(x/y)),"FP16 divide");
   }
 }
 // ALU-integrated integer DIV and FP16 FDIV instruction semantics.
 ALU integrated;
 for (int i=0;i<100000;i++) {
   int16_t a=static_cast<int16_t>(rng());
   int16_t b=static_cast<int16_t>(rng());
   if (b==0) b=1;
   const uint16_t got=integrated.execute(AluOp::DIV,static_cast<uint16_t>(a),static_cast<uint16_t>(b)).value;
   const uint16_t expected=static_cast<uint16_t>(static_cast<int32_t>(a)/static_cast<int32_t>(b));
   check(got==expected,"ALU signed integer DIV");
 }
 check(integrated.execute(AluOp::DIV,static_cast<uint16_t>(-21),4).value==static_cast<uint16_t>(-5),"integer DIV truncates toward zero");
 check(integrated.execute(AluOp::DIV,0x8000u,0xffffu).value==0x8000u,"integer DIV INT16_MIN / -1 wraps to 16 bits");
 bool integerZeroCaught=false;
 try { (void)integrated.execute(AluOp::DIV,7,0); }
 catch (const SimException& e) { integerZeroCaught=e.kind==ExKind::DivideByZero; }
 check(integerZeroCaught,"integer DIV by zero raises DivideByZero");
 for (int i=0;i<20000;i++) {
   uint16_t a=static_cast<uint16_t>(rng()), b=static_cast<uint16_t>(rng());
   check(integrated.execute(AluOp::FDIV,a,b).value==FP16::div(a,b),"ALU FP16 FDIV integration");
 }
 check(integrated.execute(AluOp::FDIV,FP16::fromFloat(6.0f),FP16::fromFloat(2.0f)).value==FP16::fromFloat(3.0f),"ALU FP16 FDIV exact result");
 check(integrated.execute(AluOp::FDIV,FP16::fromFloat(1.0f),FP16::fromFloat(3.0f)).value==FP16::fromFloat(1.0f/3.0f),"ALU FP16 FDIV rounding");
 check(integrated.execute(AluOp::FDIV,FP16::fromFloat(1.0f),0x0000u).value==0x7c00u,"ALU FP16 FDIV by zero gives infinity");
 check((integrated.execute(AluOp::FDIV,0x0000u,0x0000u).value&0x7fffu)==0x7e00u,"ALU FP16 FDIV 0/0 gives NaN");
 // Explicit FP16 division corner cases and representative rounding checks.
 auto h=[](float v){ return FP16::fromFloat(v); };
 auto isNaN=[](uint16_t v){ return (v & 0x7c00u)==0x7c00u && (v & 0x03ffu)!=0; };
 check(FP16::div(h(6.0f),h(2.0f))==h(3.0f),"FP16 divide exact result");
 check(FP16::div(h(1.0f),h(3.0f))==h(1.0f/3.0f),"FP16 divide rounding");
 check(FP16::div(h(1.0f),h(0.0f))==0x7c00u,"FP16 divide by +zero");
 check(FP16::div(h(-1.0f),h(0.0f))==0xfc00u,"FP16 divide by signed zero");
 check(isNaN(FP16::div(h(0.0f),h(0.0f))),"FP16 0/0 produces NaN");
 check(isNaN(FP16::div(0x7c00u,0x7c00u)),"FP16 infinity/infinity produces NaN");
 check(FP16::div(h(0.0f),h(-2.0f))==0x8000u,"FP16 signed zero quotient");
 check(FP16::div(h(2.0f),0x7c00u)==0x0000u,"FP16 finite/infinity produces zero");
 check(FP16::div(0x7c00u,h(-2.0f))==0xfc00u,"FP16 infinity/finite produces signed infinity");
 check(FP16::div(h(1.0f),h(65504.0f))==h(1.0f/65504.0f),"FP16 division subnormal result");
 check(FP16::div(h(65504.0f),h(0.5f))==0x7c00u,"FP16 division overflow");
 check(isNaN(FP16::div(0x7e00u,h(1.0f))),"FP16 NaN propagation");
 std::cout<<"Arithmetic verification: "<<checks-fails<<" passed, "<<fails<<" failed ("<<checks<<" checks)\n";
 return fails?1:0;
}
