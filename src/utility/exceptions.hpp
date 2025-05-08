#pragma once

#include <stdexcept>
#include <string>

namespace particles {

/**
 * Base exception class for all particles-related errors
 */
class ParticlesException : public std::runtime_error {
  public:
    explicit ParticlesException(const std::string &message)
        : std::runtime_error(message) {}
};

/**
 * Exception for simulation-related errors
 */
class SimulationError : public ParticlesException {
  public:
    explicit SimulationError(const std::string &message)
        : ParticlesException("Simulation error: " + message) {}
};

/**
 * Exception for rendering-related errors
 */
class RenderError : public ParticlesException {
  public:
    explicit RenderError(const std::string &message)
        : ParticlesException("Render error: " + message) {}
};

/**
 * Exception for I/O operations (file read/write, JSON parsing)
 */
class IOError : public ParticlesException {
  public:
    explicit IOError(const std::string &message)
        : ParticlesException("I/O error: " + message) {}
};

/**
 * Exception for configuration validation errors
 */
class ConfigError : public ParticlesException {
  public:
    explicit ConfigError(const std::string &message)
        : ParticlesException("Configuration error: " + message) {}
};

/**
 * Exception for UI-related errors
 */
class UIError : public ParticlesException {
  public:
    explicit UIError(const std::string &message)
        : ParticlesException("UI error: " + message) {}
};

} // namespace particles
