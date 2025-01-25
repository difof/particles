#pragma once

#include <chrono>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>

namespace particles {

/**
 * Thread-safe logging system for DEBUG builds only
 * Zero overhead in release builds (NDEBUG defined)
 */
class Logger {
  public:
    enum Level {
        DEBUG_LEVEL = 0,
        INFO_LEVEL = 1,
        WARN_LEVEL = 2,
        ERROR_LEVEL = 3
    };

    static void log(Level level, const std::string &file, int line,
                    const std::string &message) {
#ifdef DEBUG
        static std::mutex log_mutex;
        std::lock_guard<std::mutex> lock(log_mutex);

        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                      now.time_since_epoch()) %
                  1000;

        // Extract filename from full path
        std::string filename = file;
        size_t last_slash = filename.find_last_of("/\\");
        if (last_slash != std::string::npos) {
            filename = filename.substr(last_slash + 1);
        }

        const char *level_str = level_to_string(level);

        std::cerr << "[" << level_str << "]["
                  << std::put_time(std::localtime(&time_t), "%H:%M:%S") << "."
                  << std::setfill('0') << std::setw(3) << ms.count() << "]["
                  << filename << ":" << line << "] " << message << std::endl;
#endif
    }

  private:
    static const char *level_to_string(Level level) {
        switch (level) {
        case DEBUG_LEVEL:
            return "DEBUG";
        case INFO_LEVEL:
            return "INFO ";
        case WARN_LEVEL:
            return "WARN ";
        case ERROR_LEVEL:
            return "ERROR";
        default:
            return "UNKNOWN";
        }
    }
};

} // namespace particles

// Logging macros - compile to nothing in release builds
#ifdef DEBUG
#define LOG_DEBUG(msg)                                                         \
    particles::Logger::log(particles::Logger::DEBUG_LEVEL, __FILE__, __LINE__, \
                           msg)
#define LOG_INFO(msg)                                                          \
    particles::Logger::log(particles::Logger::INFO_LEVEL, __FILE__, __LINE__,  \
                           msg)
#define LOG_WARN(msg)                                                          \
    particles::Logger::log(particles::Logger::WARN_LEVEL, __FILE__, __LINE__,  \
                           msg)
#define LOG_ERROR(msg)                                                         \
    particles::Logger::log(particles::Logger::ERROR_LEVEL, __FILE__, __LINE__, \
                           msg)
#else
#define LOG_DEBUG(msg)                                                         \
    do {                                                                       \
    } while (0)
#define LOG_INFO(msg)                                                          \
    do {                                                                       \
    } while (0)
#define LOG_WARN(msg)                                                          \
    do {                                                                       \
    } while (0)
#define LOG_ERROR(msg)                                                         \
    do {                                                                       \
    } while (0)
#endif
