#ifndef TIMEMAN_H
#define TIMEMAN_H

#include "search.h"
#include "types.h"
#include <chrono>

namespace ClercX {

class TimeManager {
public:
    void init(const Search::Limits& limits, Color us, int ply);
    bool should_stop(const Search::SearchInfo& info);
    int64_t optimum_time() const;
    int64_t maximum_time() const;

private:
    int64_t startTime;
    int64_t optTime;
    int64_t maxTime;
    
    // Limits
    bool infinite;
    int64_t moveTime;
    int movesToGo;
    int64_t wTime, bTime;
    int64_t wInc, bInc;
    int depthLimit;
    int64_t nodesLimit;
    
    // Logic
    bool stability_detected;
    double stability_factor;

    int64_t elapsed() const;
};

extern TimeManager Time;

} // namespace ClercX

#endif // TIMEMAN_H
