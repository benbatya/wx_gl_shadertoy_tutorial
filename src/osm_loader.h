#pragma once

#include <string>

class OSMLoader {
public:
    OSMLoader() = default;

    void setFilepath(const std::string& filepath) { filepath_ = filepath; }
    bool Count();

protected:
    std::string filepath_ {};
};