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
    using Coordinates = std::vector<Coordinate>;

    // Represents both Areas (closed=true) and Ways (closed=false)
    // Areas will have the first and last nodes match to ensure that it is closed
    struct Way_t {
        osmium::object_id_type id{0};
        std::string name{};
        std::string type{};
        Coordinates nodes;
    };
    using Id2Way = std::unordered_map<osmium::object_id_type, Way_t>;

    struct Relationship_t {
        osmium::object_id_type id{0};
        std::string name{};
        std::string type{};
        Coordinates outerRing;
        std::vector<Coordinates> innerRings;
    };
    using Id2Relationship = std::unordered_map<osmium::object_id_type, Relationship_t>;

    using CoordinateBounds = osmium::Box;
    /**
     * Get ways within the specified coordinate bounds.
     * @param bounds The coordinate bounds (min and max coordinates).
     * @return A vector of routes, where each route is represented as a vector
     * of coordinates
     */
    Id2Way getWays(const CoordinateBounds &bounds) const;

  protected:
    std::string filepath_{};
};