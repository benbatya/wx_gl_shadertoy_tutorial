#pragma once

#include <osmium/osm/box.hpp>
#include <osmium/osm/location.hpp>
#include <osmium/osm/types.hpp>

#include <string>
#include <vector>

constexpr auto NAME_TAG = "name";
constexpr auto HIGHWAY_TAG = "highway";
constexpr auto TYPE_TAG = "type";
constexpr auto BOUNDARY_VALUE = "boundary";

class OSMLoader {
  public:
    OSMLoader() = default;

    void setFilepath(const std::string &filepath) { filepath_ = filepath; }
    bool Count();

    // Using definition of Location:
    // https://osmcode.org/libosmium/manual.html#locations
    using Coordinate = osmium::Location;
    using Coordinates = std::vector<Coordinate>;
    using Tags = std::unordered_map<std::string, std::string>;
    using Id2Tags = std::unordered_map<osmium::object_id_type, Tags>;

    // Represents both Areas (closed=true) and Ways (closed=false)
    // Areas will have the first and last nodes match to ensure that it is closed
    struct Way_t {
        osmium::object_id_type id{0};
        Coordinates nodes;
        Tags tags;
    };
    using Id2Way = std::unordered_map<osmium::object_id_type, Way_t>;

    struct Relationship_t {
        osmium::object_id_type id{0};
        Coordinates outerRing;
        // std::vector<Coordinates> innerRings;
        Tags tags;
    };
    using Id2Relationship = std::unordered_map<osmium::object_id_type, Relationship_t>;

    using CoordinateBounds = osmium::Box;
    /**
     * Get ways within the specified coordinate bounds.
     * @param bounds The coordinate bounds (min and max coordinates).
     * @return A vector of routes, where each route is represented as a vector
     * of coordinates
     */
    using OSMData = std::pair<Id2Way, Id2Relationship>;
    OSMData getWays(const CoordinateBounds &bounds) const;

  protected:
    std::string filepath_{};
};