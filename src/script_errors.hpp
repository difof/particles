#ifndef SCRIPT_ERRORS_HPP
#define SCRIPT_ERRORS_HPP

#include <string>
#include <sstream>
#include <regex>
#include <stdexcept>

struct ScriptErrorBase
{
    ScriptErrorBase() : line(0), source(""), message("") {}
    ScriptErrorBase(const std::string &_source) : line(0), source(_source), message("") {}

    virtual std::string toString() const = 0;

    int line;
    std::string source;
    std::string message;
};

struct MoonCompileError : public ScriptErrorBase, public std::runtime_error
{
    MoonCompileError(const std::string &errorMessage, const std::string &filename)
        : ScriptErrorBase(filename),
          std::runtime_error(_constructErrorMessage(errorMessage))
    {
    }

    std::string toString() const
    {
        std::ostringstream oss;
        oss << "moonscript compile error at " << source << ":" << line << ": " << message;
        return oss.str();
    }

private:
    std::string _constructErrorMessage(const std::string &errorMessage)
    {
        std::regex re(R"(\[string \"moonc\"\]:(\d+): .*\n \[\d+\] >>    (.*))");
        std::smatch match;

        if (std::regex_search(errorMessage, match, re) && match.size() > 1)
        {
            this->line = std::stoi(match.str(1));
            this->message = "failed to parse " + match.str(2);
        }

        return toString();
    }
};

struct LuaRuntimeError : public ScriptErrorBase, public std::runtime_error
{
    LuaRuntimeError(const std::string &errorMessage)
        : ScriptErrorBase(),
          std::runtime_error(_constructErrorMessage("lua runtime error", errorMessage))
    {
    }

    LuaRuntimeError(const std::string &_prefix, const std::string &errorMessage)
        : ScriptErrorBase(),
          std::runtime_error(_constructErrorMessage(_prefix, errorMessage))
    {
    }

    std::string toString() const
    {
        std::ostringstream oss;
        if (line == 0)
        {
            oss << prefix << ": " << message;
        }
        else
        {
            oss << prefix << " at " << source << ":" << line << ": " << message;
        }
        return oss.str();
    }

private:
    std::string _constructErrorMessage(const std::string &_prefix, const std::string &errorMessage)
    {
        std::regex re(R"(\[(\S*) \"(.*)\"\]:(\d+): (.*))");
        std::smatch match;

        this->prefix = _prefix;

        if (std::regex_search(errorMessage, match, re) && match.size() > 1)
        {
            this->line = std::stoi(match.str(3));
            this->source = match.str(2);
            this->message = match.str(4);
        }
        else
        {
            this->message = errorMessage;
        }

        return toString();
    }

public:
    std::string prefix;
};

struct ScriptContainerError : public std::runtime_error
{
    ScriptContainerError(const std::string &errorMessage)
        : std::runtime_error(errorMessage)
    {
    }

    std::string toString() const
    {
        return what();
    }
};

#endif