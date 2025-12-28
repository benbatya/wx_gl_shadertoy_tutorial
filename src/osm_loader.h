#pragma once

#include <osmium/osm/box.hpp>
#include <osmium/osm/location.hpp>
#include <osmium/osm/types.hpp>

#include <string>
#include <vector>
class OSMLoader {
  public:
    OSMLoader() = default;

    void setFilepath(const std::string &filepath) { filepath_ = filepath; }
    bool Count();

    // Using definition of Location:
    // https://osmcode.org/libosmium/manual.html#locations
    using Coordinate = osmium::Location;
    using Route = std::vector<Coordinate>;
    using Routes = std::unordered_map<osmium::object_id_type, Route>;
    using CoordinateBounds = osmium::Box;
    /**
     * Get routes within the specified coordinate bounds.
     * @param bounds The coordinate bounds (min and max coordinates).
     * @return A vector of routes, where each route is represented as a vector
     * of coordinates
     */
    Routes getRoutes(const CoordinateBounds &bounds) const;

  protected:
    std::string filepath_{};
};