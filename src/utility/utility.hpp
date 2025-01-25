#ifndef UTILITY_HPP
#define UTILITY_HPP

#include <string>
#include <fstream>
#include <sstream>
#include <vector>

std::string readFileIntoBuffer(const std::string &filename)
{
    std::ifstream file(filename, std::ios::binary);
    std::stringstream buffer;
    buffer << file.rdbuf();
    file.close();

    return buffer.str();
}

void writeFile(const std::string &filename, const std::string &buffer)
{
    std::ofstream file(filename, std::ios::binary);
    file.write(buffer.c_str(), buffer.size());
    file.close();
}

std::vector<std::string> split(const std::string &s, const char delimiter)
{
    std::vector<std::string> tokens;
    std::string token;
    std::istringstream tokenStream(s);
    while (std::getline(tokenStream, token, delimiter))
    {
        tokens.push_back(token);
    }
    return tokens;
}

bool ends_with(std::string const &value, std::string const &ending)
{
    if (ending.size() > value.size())
        return false;
    return std::equal(ending.rbegin(), ending.rend(), value.rbegin());
}

#include <string>

std::string replace_all(std::string str, const std::string &from, const std::string &to)
{
    size_t start_pos = 0;
    while ((start_pos = str.find(from, start_pos)) != std::string::npos)
    {
        str.replace(start_pos, from.length(), to);
        start_pos += to.length(); // Handles case where 'to' is a substring of 'from'
    }
    return str;
}

#endif