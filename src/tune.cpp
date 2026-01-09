#include "tune.h"
#include <iostream>

namespace Tune {
    std::map<std::string, Parameter> params;

    void init() {
        // Define tunable parameters here
        // Evaluation weights
        params["PawnVal"] = {100, 50, 150, 5};
        params["KnightVal"] = {320, 200, 400, 5};
        params["BishopVal"] = {330, 200, 400, 5};
        params["RookVal"] = {500, 300, 700, 5};
        params["QueenVal"] = {900, 700, 1100, 5};
        
        // Search parameters
        params["LMR_Base"] = {50, 10, 100, 5}; // multiplied by 100
        params["LMR_Div"] = {200, 100, 400, 10}; // multiplied by 100
    }

    void set(const std::string& name, int value) {
        if (params.count(name)) {
            params[name].value = value;
        }
    }

    int get(const std::string& name) {
        if (params.count(name)) {
            return params[name].value;
        }
        return 0;
    }

    void print_params() {
        for (const auto& p : params) {
            std::cout << "option name " << p.first << " type spin default " 
                      << p.second.value << " min " << p.second.min 
                      << " max " << p.second.max << "\n";
        }
    }
}
