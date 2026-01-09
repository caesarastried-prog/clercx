#ifndef UCIOPTION_H
#define UCIOPTION_H

#include <string>
#include <map>
#include <functional>
#include <variant>
#include <vector>
#include <iostream>

namespace ClercX {

class Option {
public:
    using OnChange = std::function<void(const Option&)>;

    // Constructors for different option types
    Option(const char* v, OnChange f = nullptr); // String
    Option(bool v, OnChange f = nullptr);        // Check
    Option(OnChange f = nullptr);                // Button
    Option(int v, int min, int max, OnChange f = nullptr); // Spin

    Option& operator=(const std::string& v);
    void operator<<(const Option& o); // Assignment from another option (copy value)

    // Conversions
    operator int() const;
    operator bool() const;
    operator std::string() const;

    // Getters
    std::string get_type() const;
    std::string get_default() const;
    std::string get_current() const;
    int get_min() const;
    int get_max() const;

private:
    friend class OptionsMap;
    friend std::ostream& operator<<(std::ostream& os, const Option& o);

    std::variant<std::string, bool, int, std::nullptr_t> defaultValue, currentValue;
    int min, max;
    OnChange onChange;
    std::string type;
};

class OptionsMap {
public:
    OptionsMap();
    
    void init();
    
    Option& operator[](const std::string& name);
    const Option& operator[](const std::string& name) const;
    
    bool count(const std::string& name) const;
    
    std::map<std::string, Option> options;
};

extern OptionsMap Options;

} // namespace ClercX

#endif // UCIOPTION_H