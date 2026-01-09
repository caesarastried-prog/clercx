#include "../include/ucioption.h"
#include <algorithm>
#include <sstream>

namespace ClercX {

OptionsMap Options;

Option::Option(const char* v, OnChange f) : defaultValue(std::string(v)), currentValue(std::string(v)), min(0), max(0), onChange(f), type("string") {}

Option::Option(bool v, OnChange f) : defaultValue(v), currentValue(v), min(0), max(0), onChange(f), type("check") {}

Option::Option(OnChange f) : defaultValue(nullptr), currentValue(nullptr), min(0), max(0), onChange(f), type("button") {}

Option::Option(int v, int min, int max, OnChange f) : defaultValue(v), currentValue(v), min(min), max(max), onChange(f), type("spin") {}

Option& Option::operator=(const std::string& v) {
    if (type == "button") {
        if (onChange) onChange(*this);
        return *this;
    }

    if (type == "check") {
        currentValue = (v == "true");
    } else if (type == "spin") {
        currentValue = std::clamp(std::stoi(v), min, max);
    } else {
        currentValue = v;
    }

    if (onChange) onChange(*this);
    return *this;
}

Option::operator int() const {
    return std::get<int>(currentValue);
}

Option::operator bool() const {
    return std::get<bool>(currentValue);
}

Option::operator std::string() const {
    return std::get<std::string>(currentValue);
}

std::string Option::get_type() const {
    return type;
}

std::string Option::get_default() const {
    if (type == "string") return std::get<std::string>(defaultValue);
    if (type == "check") return std::get<bool>(defaultValue) ? "true" : "false";
    if (type == "spin") return std::to_string(std::get<int>(defaultValue));
    return "";
}

std::string Option::get_current() const {
    if (type == "string") return std::get<std::string>(currentValue);
    if (type == "check") return std::get<bool>(currentValue) ? "true" : "false";
    if (type == "spin") return std::to_string(std::get<int>(currentValue));
    return "";
}

int Option::get_min() const {
    return min;
}

int Option::get_max() const {
    return max;
}

OptionsMap::OptionsMap() {}

void OptionsMap::init() {
    // Hash (1-65536 MB)
    options["Hash"] = Option(16, 1, 65536, [](const Option& o) {
        std::cout << "info string Hash set to " << int(o) << " MB" << std::endl;
        // Logic to resize TT would go here: TT.resize(int(o));
    });

    // Threads (1-128)
    options["Threads"] = Option(1, 1, 128);

    // Ponder (bool)
    options["Ponder"] = Option(false);

    // Clear Hash (button)
    options["Clear Hash"] = Option(Option::OnChange([](const Option&) {
        std::cout << "info string Hash cleared" << std::endl;
        // Logic to clear TT: TT.clear();
    }));

    // Contempt (-100 to 100)
    options["Contempt"] = Option(0, -100, 100);

    // Move Overhead (0-5000ms)
    options["Move Overhead"] = Option(10, 0, 5000);

    // MultiPV (1-500)
    options["MultiPV"] = Option(1, 1, 500);

    // Skill Level (0-20)
    options["Skill Level"] = Option(20, 0, 20);
}

Option& OptionsMap::operator[](const std::string& name) {
    return options[name];
}

const Option& OptionsMap::operator[](const std::string& name) const {
    return options.at(name);
}

bool OptionsMap::count(const std::string& name) const {
    return options.count(name);
}

} // namespace ClercX
