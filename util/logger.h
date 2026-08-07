#include "../stdx.h"
#include <format>



class LOG {
public:
    enum class LOG_LEVEL { DEF, LOW, MID, HIGH, SYS };

private:
    std::string message;

    std::string_view level_to_string(LOG_LEVEL level) {
        switch (level) {
            case LOG_LEVEL::DEF:  return "DEF";
            case LOG_LEVEL::LOW:  return "LOW";
            case LOG_LEVEL::MID:  return "MID";
            case LOG_LEVEL::HIGH: return "HIGH";
            case LOG_LEVEL::SYS:  return "SYS";
        }
        return "UNKNOWN";
    }

public:
    LOG() {
        std::cout << "LOGGER: Initialized.\n";
    }

    template <typename... Args>
    void log(LOG_LEVEL level, std::format_string<Args...> fmt, Args&&... args) {
        message = std::format(fmt, std::forward<Args>(args)...);
        std::cout << std::format("[{}] {}\n", level_to_string(level), message);
    }
};


// todo:
// 1 -> time formating
// 2 -> CPU USAGE
