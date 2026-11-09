#include "VideoCommon/TurbulencePresentTimes.h"
#include <cassert>
#include <thread>
#include <vector>
#include <set>
int main() {
 std::vector<std::thread> threads;
 for(int t=0;t<4;++t)threads.emplace_back([t]{for(int i=0;i<10000;++i)TurbulencePresentTimes::Push(1+t*10000+i);});
 for(auto& thread:threads)thread.join();
 auto& ring=TurbulencePresentTimes::Get();
 assert(ring.head.load()==40000);
 std::set<double> samples;
 for(auto& slot:ring.seconds){double value=slot.load();assert(value>=1&&value<=40000);samples.insert(value);}
 assert(samples.size()==ring.seconds.size());
}
