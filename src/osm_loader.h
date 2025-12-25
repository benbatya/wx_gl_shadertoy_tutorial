#pragma once

#include <string>

class OSMLoader {
public:
    OSMLoader() = default;

    void setFilepath(const std::string& filepath) { filepath_ = filepath; }
    int Count();

protected:
    std::string filepath_ {};
};