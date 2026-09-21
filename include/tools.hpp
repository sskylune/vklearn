#pragma once

#include <fstream>
#include <sstream>

static inline std::string readTextFile(const std::string& path) {
    std::ifstream fs(path);
    if (fs.is_open()) {
        std::stringstream ss;
        ss << fs.rdbuf();
        const std::string data = ss.str();
        return data;
    }

    return std::string {};
}