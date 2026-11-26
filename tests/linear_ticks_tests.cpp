#include "charts/line/linear_ticks_p.h"
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>

#define CHECK(x) do { if (!(x)) { std::cerr << __LINE__ << ": " << #x << '\n'; std::exit(1); } } while(false)
int main() {
    const auto measure=[](const QString& s) { return double(s.size()*8); };
    auto ticks=ProGraphics::detail::linearTicks(-1.56,20.29,480,true,measure);
    CHECK(ticks.values.size()==5);
    const QStringList whole{"0","5","10","15","20"};
    for(qsizetype i=0;i<ticks.values.size();++i) {
        CHECK(ticks.values[i].text==whole[i]); CHECK(ticks.values[i].value==i*5);
    }
    ticks=ProGraphics::detail::linearTicks(0,1,800,false,measure);
    CHECK(ticks.values.size()==11);
    CHECK(ticks.values[0].text=="0.0" && ticks.values[5].text=="0.5" && ticks.values[10].text=="1.0");
    ticks=ProGraphics::detail::linearTicks(0.03,1.03,800,false,measure);
    CHECK(ticks.values.size()==10); CHECK(ticks.values[0].text=="0.1"); CHECK(ticks.values.last().text=="1.0");
    ticks=ProGraphics::detail::linearTicks(0.001,0.006,800,false,measure);
    CHECK(!ticks.values.isEmpty());
    int places=-1;
    for(const auto& tick:ticks.values) {
        int p=tick.text.size()-tick.text.indexOf('.')-1;
        if(places<0) places=p; CHECK(p==places);
    }
    ticks=ProGraphics::detail::linearTicks(1e12,1e12+0.002,800,false,measure);
    CHECK(ticks.values.size()>=2);
    for(qsizetype i=1;i<ticks.values.size();++i) {
        CHECK(ticks.values[i].value>ticks.values[i-1].value);
        CHECK(ticks.values[i].text!=ticks.values[i-1].text);
    }
    CHECK(ProGraphics::detail::linearTicks(1,1,800,false,measure).values.isEmpty());
    ticks=ProGraphics::detail::linearTicks(-8.5e307,8.5e307,800,false,measure);
    CHECK(!ticks.values.isEmpty() && ticks.values.size()<1000);
    for(const auto& tick:ticks.values) CHECK(std::isfinite(tick.value));
    std::cout<<"Linear axis tick spacing, uniform decimals, pan alignment and extreme-range checks passed\n";
}
