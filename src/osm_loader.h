#pragma once

#include <string>
#include <vector>
class OSMLoader {
public:
    OSMLoader() = default;

    void setFilepath(const std::string& filepath) { filepath_ = filepath; }
    bool Count();

    using Routes = std::vector<std::vector<std::pair<double, double>>>;
    using Coordinate = std::pair<double, double>;
    using CoordinateBounds = std::pair<Coordinate, Coordinate>;
    /**
     * Get routes within the specified coordinate bounds.
     * @param bounds The coordinate bounds (min and max coordinates).
     * @return A vector of routes, where each route is represented as a vector of coordinates
     */
    Routes getRoutes(const CoordinateBounds& bounds) const;

protected:
    std::string filepath_ {};
};