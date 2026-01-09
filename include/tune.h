#ifndef TUNE_H
#define TUNE_H

#include <string>
#include <map>

namespace Tune {

    struct Parameter {
        int value;
        int min;
        int max;
        int step;
    };

    extern std::map<std::string, Parameter> params;

    void init();
    void set(const std::string& name, int value);
    int get(const std::string& name);
    
    // Output for SPSA
    void print_params();
}

#endif // TUNE_H
