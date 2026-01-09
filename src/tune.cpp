#include "tune.h"
#include <iostream>
#include <sstream>

namespace Tune {

std::map<std::string, Parameter> params;

void add(const std::string& name, int value, int min = 0, int max = 1000, int step = 1) {
    params[name] = {value, min, max, step};
}

void init() {
    // --- Search Parameters ---
    add("LMR_Base", 4, 1, 10);
    add("LMR_Factor", 4, 1, 10);
    add("Futility_Margin", 100, 50, 500);
    add("RFP_Margin", 75, 25, 200);
    add("ASP_Window", 25, 10, 100);
    
    // --- Evaluation: Material ---
    add("Pawn_MG", 82, 50, 150);   add("Pawn_EG", 94, 50, 150);
    add("Knight_MG", 337, 250, 450); add("Knight_EG", 281, 200, 400);
    add("Bishop_MG", 365, 250, 450); add("Bishop_EG", 297, 200, 400);
    add("Rook_MG", 477, 400, 600);   add("Rook_EG", 512, 400, 600);
    add("Queen_MG", 1025, 900, 1200); add("Queen_EG", 968, 900, 1200);

    // --- Evaluation: Mobility ---
    add("Mobility_N_MG", 4, 0, 20); add("Mobility_N_EG", 4, 0, 20);
    add("Mobility_B_MG", 3, 0, 20); add("Mobility_B_EG", 3, 0, 20);
    add("Mobility_R_MG", 2, 0, 20); add("Mobility_R_EG", 4, 0, 20);
    add("Mobility_Q_MG", 1, 0, 20); add("Mobility_Q_EG", 2, 0, 20);

    // --- Evaluation: Pawn Structure ---
    add("Pawn_Passed_MG", 10, 0, 100); add("Pawn_Passed_EG", 20, 0, 100);
    add("Pawn_Iso_MG", -10, -50, 0);   add("Pawn_Iso_EG", -15, -50, 0);
    add("Pawn_Double_MG", -10, -50, 0);add("Pawn_Double_EG", -15, -50, 0);
    
    // --- Evaluation: King Safety ---
    add("Safety_Weight", 100, 50, 200); // Percentage
    
    // --- Threads ---
    add("Threads", 1, 1, 128);
}

void set(const std::string& name, int value) {
    if (params.find(name) != params.end()) {
        params[name].value = value;
    }
}

int get(const std::string& name) {
    if (params.find(name) != params.end()) {
        return params[name].value;
    }
    return 0;
}

void print_params() {
    for (const auto& [name, p] : params) {
        std::cout << "option name " << name << " type spin default " << p.value 
                  << " min " << p.min << " max " << p.max << std::endl;
    }
}

} // namespace Tune