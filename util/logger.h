#pragma once
#include <chrono>
#include <format>
#include <fstream>
#include <print>
#include <string>
#include <string_view>
#include <utility>

class LOG {
public:
  enum class LOG_LEVEL : std::uint8_t { DEF, LOW, MID, HIGH, SYS };

private:
  std::string m_buffer;
  std::ofstream m_logFile;

  static constexpr std::string_view level_to_string(LOG_LEVEL level) noexcept {
    switch (level) {
    case LOG_LEVEL::DEF:
      return "DEF";
    case LOG_LEVEL::LOW:
      return "LOW";
    case LOG_LEVEL::MID:
      return "MID";
    case LOG_LEVEL::HIGH:
      return "HIGH";
    case LOG_LEVEL::SYS:
      return "SYS";
    }
    return "UNKNOWN";
  }

  LOG(const LOG &) = delete;
  LOG &operator=(const LOG &) = delete;
  LOG(LOG &&) = delete;
  LOG &operator=(LOG &&) = delete;

  static LOG &get() {
    static LOG instance;
    return instance;
  }

  ~LOG() {
    if (m_logFile.is_open())
      std::println(m_logFile, "|==========> Logger Closed <===========|");
  }

private:
  LOG() : m_logFile("log.txt", std::ios::app) {
    m_buffer.reserve(256);
    std::println("LOGGER: Initialized.");
    if (m_logFile.is_open()) {
      std::println(m_logFile, "\n|==========> Logger Started <==========|");
      m_logFile.flush();
    }
  }

public:
  template <typename... Args>
  void log(LOG_LEVEL level, std::format_string<Args...> fmt, Args &&...args) {
    auto ts = std::chrono::floor<std::chrono::milliseconds>(
        std::chrono::system_clock::now());

    m_buffer.clear();
    std::format_to(std::back_inserter(m_buffer), "[{:%Y-%m-%d %H:%M:%S}] [{}] ",
                   ts, level_to_string(level));
    std::format_to(std::back_inserter(m_buffer), fmt,
                   std::forward<Args>(args)...);

    std::println("{}", m_buffer);

    if (m_logFile.is_open()) {
      std::println(m_logFile, "{}", m_buffer);
      m_logFile.flush();
    }
  }
};

/// usage

/*
LOG::get().log(LOG::LOG_LEVEL::SYS, "Hello, {}!", "World");
*/
