#include "../Source/DSP/CutEdit.h"
#include <iostream>
int main() {
 int bad=0;auto check=[&](bool ok,const char* label){std::cout<<(ok?"OK ":"FAIL ")<<label<<"\n";bad+=!ok;};
 std::vector<float> in(1000,.25f),out;
 check(zs::cutRange(in,1000,.2,.4,out),"middle cut accepted");
 check(out.size()==790,"length includes 10 ms crossfade");
 check(std::all_of(out.begin(),out.end(),[](float x){return std::abs(x-.25f)<1e-6;}),"constant signal continuous across cut");
 check(in.size()==1000,"source preserved");
 check(zs::cutRange(in,1000,0,.2,out)&&out.size()==800,"trim beginning");
 check(zs::cutRange(in,1000,.8,1,out)&&out.size()==800,"trim end");
 check(!zs::cutRange(in,1000,0,1,out),"do not delete entire take");
 check(!zs::cutRange(in,1000,.5,.2,out),"reject reversed range");
 check(!zs::cutRange(in,1000,-1,.2,out),"reject negative range");
 return bad?1:0;
}
