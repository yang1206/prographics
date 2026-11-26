#pragma once
#include <QVector>
#include <QString>
#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>

// Adapted from ImPlot v1.1 NiceNum / Locator_Default (MIT).
// https://github.com/epezent/implot/blob/v1.1/implot.cpp#L415-L438
// https://github.com/epezent/implot/blob/v1.1/implot.cpp#L713-L756
// Copyright (c) 2020-2024 Evan Pezent
// Copyright (c) 2025-2026 Breno Cunha Queiroz
// Full permission notice: third_party/implot/LICENSE.
namespace ProGraphics::detail {
struct LinearTick { double value; QString text; };
struct LinearTicks { double interval=0; QVector<LinearTick> values; };

inline double niceNumber(double x,bool round) {
    if (!(x>0) || !std::isfinite(x)) return x;
    const int exponent=int(std::floor(std::log10(x)));
    const double power=std::pow(10.0,exponent);
    if (!(power>0)) return x;
    const double f=x/power;
    const double nf=round ? (f<1.5?1:f<3?2:f<7?5:10) : (f<=1?1:f<=2?2:f<=5?5:10);
    const double result=nf*power;
    return std::isfinite(result) && result>0 ? result : x;
}

inline LinearTicks linearTicks(double min,double max,double pixels,bool vertical,
                               const std::function<double(const QString&)>& measure) {
    LinearTicks result;
    const double span=max-min;
    if (!std::isfinite(min) || !std::isfinite(max) || !std::isfinite(span) || span<=0 || pixels<=0) return result;
    // ImPlot v1.1: pixel-based major count, nice 1/2/5 interval, ten subdivisions.
    const int majorCount=std::max(2,int(std::round(std::min(pixels,100000.0)/(vertical?300.0:400.0))));
    double interval=niceNumber(niceNumber(span*0.99,false)/(majorCount-1),true);
    const double magnitude=std::max(std::abs(min),std::abs(max));
    double ulp=std::nextafter(magnitude,std::numeric_limits<double>::infinity())-magnitude;
    if (!std::isfinite(ulp)) ulp=magnitude-std::nextafter(magnitude,0.0);
    interval=std::max(interval,ulp);
    if (!(interval>0) || !std::isfinite(interval)) return result;
    const double spacing=std::max(interval/10,ulp);
    const double start=std::floor(min/interval)*interval;
    if (!std::isfinite(start)) return result;
    QVector<double> candidates;
    int anchor=-1;
    const int count=std::min(1000,int(std::ceil(span/interval))+3);
    for(int major=0;major<count;++major) {
        const double base=std::fma(double(major),interval,start);
        if(!std::isfinite(base) || base>max) break;
        for(int minor=0;minor<10;++minor) {
            double v=std::fma(double(minor)/10,interval,base);
            if(std::abs(v)<spacing*1e-10) v=0;
            if(v<min || v>max || (!candidates.isEmpty() && v<=candidates.last())) continue;
            if(minor==0 && anchor<0) anchor=int(candidates.size());
            candidates.append(v);
        }
    }
    if(anchor<0) anchor=0;
    // ImPlot removes alternating labels when occupied size exceeds the axis.
    // Repeat its pruning for Qt fonts; one shared precision follows the resulting spacing.
    for(int stride=1;stride<=1024;stride*=2) {
        result.values.clear();
        result.interval=spacing*stride;
        const bool scientific=(magnitude>=1e7 || (magnitude>0 && magnitude<1e-6));
        const int precision=scientific ? int(std::clamp(std::ceil(std::log10(std::max(magnitude,spacing)))-std::floor(std::log10(result.interval)),0.0,16.0))
            : int(std::clamp(-std::floor(std::log10(result.interval)+1e-10),0.0,15.0));
        double occupied=0;
        for(qsizetype i=0;i<candidates.size();++i) if((int(i)-anchor)%stride==0) {
            QString text=QString::number(candidates[i],scientific?'e':'f',precision);
            occupied+=measure(text);
            result.values.append({candidates[i],std::move(text)});
        }
        if(occupied<=pixels*(vertical?1.0:0.8) || result.values.size()<=2) break;
    }
    return result;
}
} // namespace ProGraphics::detail
