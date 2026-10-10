#pragma once
#include <vector>
#include <cmath>
#include <algorithm>
namespace zs {
// Offline manual edit. The original file is never modified. Boundaries are
// selected by the listener, with a short crossfade to suppress hard jumps.
inline bool cutRange(const std::vector<float>& in,double sr,double from,double to,std::vector<float>& out) {
    if(!std::isfinite(sr)||sr<=0||!std::isfinite(from)||!std::isfinite(to)||from<0||to<=from||to>in.size()/sr) return false;
    const size_t a=std::min(in.size(),(size_t)std::llround(from*sr));
    const size_t b=std::min(in.size(),(size_t)std::llround(to*sr));
    if(b<=a || b-a==in.size())return false;
    const size_t fade=std::min({(size_t)std::llround(sr*.01),a,in.size()-b});
    std::vector<float> result;result.reserve(in.size()-(b-a)-fade);
    result.insert(result.end(),in.begin(),in.begin()+a-fade);
    for(size_t i=0;i<fade;++i) {
        const float t=float(i+1)/float(fade+1);
        result.push_back(in[a-fade+i]*(1-t)+in[b+i]*t);
    }
    result.insert(result.end(),in.begin()+b+fade,in.end());
    out=std::move(result);return true;
}
}
