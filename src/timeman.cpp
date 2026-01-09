#include "../include/timeman.h"
#include "../include/ucioption.h"
#include <algorithm>
#include <iostream>

namespace ClercX {

TimeManager Time;

int64_t now() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}

void TimeManager::init(const Search::Limits& limits, Color, int) {
    startTime = now();
    optTime = 0;
    maxTime = 0;
    
    infinite = false; // limits.infinite; // Assuming Limits has this
    moveTime = 0; // limits.movetime;
    movesToGo = limits.movestogo > 0 ? limits.movestogo : 35; // Default to 35 if not specified or 0 (sudden deathish)
    wTime = limits.time; // Assuming limits has wtime/btime distinction or handled before
    bTime = limits.time; // This logic needs alignment with how Limits is passed. 
    // In uci.cpp, limits.time is set based on side to move. 
    // Let's assume limits.time is the time for 'us'.
    
    int64_t myTime = limits.time;
    int64_t myInc = limits.inc;
    
    depthLimit = limits.depth;
    // nodesLimit = limits.nodes; // Assuming Search::Limits has nodes
    
    if (limits.use_time) {
        // Basic allocation
        // Formula: optimum = (time + inc*40)/moves_remaining * stability_factor
        // Actually, typically (time / moves_remaining) + inc
        
        // Let's use the user's requested formula structure if possible, but adjusted for standard chess engines.
        // User asked: optimum = (time + inc*40)/moves_remaining * stability_factor
        
        // Standard moves_remaining guess if unknown
        int mtg = limits.movestogo > 0 ? limits.movestogo : 40;
        
        // Reserve some time
        int64_t overhead = static_cast<int>(Options["Move Overhead"]);
        myTime = std::max<int64_t>(0, myTime - overhead);

        optTime = (myTime + myInc * (mtg - 1)) / mtg; // Simplified
        // Or user formula: 
        // optTime = (myTime + myInc * 40) / mtg; // If we assume 40 moves horizon? 
        
        // Let's stick to a robust standard first:
        double base = (myTime / static_cast<double>(mtg)) + myInc;
        optTime = static_cast<int64_t>(base);

        maxTime = std::min<int64_t>(myTime, optTime * 5);
    } else {
        // Fixed time or infinite
        if (moveTime > 0) {
            optTime = maxTime = moveTime;
        } else {
            // Infinite or depth/nodes based
            optTime = maxTime = 24*60*60*1000; // Very long
        }
    }
}

bool TimeManager::should_stop(const Search::SearchInfo& info) {
    if (infinite) return false;
    
    // Check limits
    if (depthLimit > 0 && info.depth >= depthLimit) return true;
    // if (nodesLimit > 0 && info.nodes >= nodesLimit) return true;
    
    if (maxTime == 0) return false; // Infinite
    
    int64_t t = elapsed();
    if (t >= maxTime) return true;
    
    // Flexible stop: if we used optimum time and result is stable, stop.
    // This requires feedback from search (e.g., best move stability).
    // For now, hard stop at maxTime, soft stop logic usually in search loop.
    
    return false;
}

int64_t TimeManager::optimum_time() const {
    return optTime;
}

int64_t TimeManager::maximum_time() const {
    return maxTime;
}

int64_t TimeManager::elapsed() const {
    return now() - startTime;
}

} // namespace ClercX
