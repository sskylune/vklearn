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

static inline std::vector<char> readBinaryFile(const std::string& path) {
    std::ifstream fs(path, std::ios::ate | std::ios::binary);
    if (fs.is_open()) {
        size_t size = static_cast<size_t>(fs.tellg());
        std::vector<char> buffer(size);
        fs.seekg(0);
        fs.read(buffer.data(), size);
        fs.close();

        return buffer;
    }

    return {};
}