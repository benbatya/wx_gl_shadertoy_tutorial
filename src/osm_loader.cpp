/*

  EXAMPLE osmium_count

  Counts the number of nodes, ways, and relations in the input file.

  DEMONSTRATES USE OF:
  * OSM file input
  * your own handler
  * the memory usage utility class

  SIMPLER EXAMPLES you might want to understand first:
  * osmium_read

  LICENSE
  The code in this example file is released into the Public Domain.

*/

#include "osm_loader.h"

// Only work with XML input files here
#include <osmium/io/xml_input.hpp>

// We want to use the handler interface
#include <osmium/handler.hpp>
#include <osmium/osm/node.hpp>
#include <osmium/osm/way.hpp>

// Utility class gives us access to memory usage information
#include <osmium/util/memory.hpp>

// For osmium::apply()
#include <osmium/visitor.hpp>

// efficient node location storage for ways
#include <osmium/index/map/sparse_mem_array.hpp>

// location handler for ways
#include <osmium/handler/node_locations_for_ways.hpp>

#include <algorithm>
#include <cstdint> // for std::uint64_t
#include <exception>
#include <iostream> // for std::cout, std::cerr
#include <unordered_set>
namespace {
struct WayNodePair {
    osmium::object_id_type wayID;
    size_t nodeIndex;

    WayNodePair(osmium::object_id_type wID, size_t nIndex) : wayID(wID), nodeIndex(nIndex) {}

    bool operator==(const WayNodePair &other) const { return wayID == other.wayID && nodeIndex == other.nodeIndex; }
};

struct WayNodePairHash {
    std::size_t operator()(const WayNodePair &p) const {
        return std::hash<osmium::object_id_type>()(p.wayID) ^ (std::hash<size_t>()(p.nodeIndex) << 1);
    }
};

using Id2Ways = std::unordered_map<osmium::object_id_type, std::unordered_set<WayNodePair, WayNodePairHash>>;
using Id2String = std::unordered_map<osmium::object_id_type, std::string>;
struct MappedWayData {
    Id2Ways node2Ways;
    OSMLoader::Id2Tags id2Tags;
};

// Map of Way -> Relationships
using Id2Ids = std::unordered_map<osmium::object_id_type, std::unordered_set<osmium::object_id_type>>;
struct RelationshipData {
    Id2Ids way2Relationships;  // all of the ways that are referenced by relationships
    Id2Ids node2Relationships; // all of the nodes that are referenced by relationships
    OSMLoader::Id2Tags id2Tags;
};

struct RelationshipHandler : public osmium::handler::Handler {
    RelationshipData relationshipData;

    void relation(const osmium::Relation &relation) noexcept {
        auto tag_value = relation.tags().get_value_by_key(::TYPE_TAG);

        if (std::strcmp(tag_value, ::BOUNDARY_VALUE) != 0) {
            return;
        }
        for (const auto &member : relation.members()) {
            if (member.type() == osmium::item_type::way) {
                // TODO: handle inner ways
                if (member.role() == "outer") {
                    relationshipData.way2Relationships[member.ref()].insert(relation.id());
                }
            } else if (member.type() == osmium::item_type::node) {
                // TODO: handle other node types
                if (member.role() == "label") {
                    relationshipData.node2Relationships[member.ref()].insert(relation.id());
                }
            }
        }

        if (auto tag_value = relation.tags().get_value_by_key(NAME_TAG); tag_value) {
            relationshipData.id2Tags[relation.id()][NAME_TAG] = tag_value;
        }
        if (auto tag_value = relation.tags().get_value_by_key(TYPE_TAG); tag_value) {
            relationshipData.id2Tags[relation.id()][TYPE_TAG] = tag_value;
        }
    };
};

struct WayHandler : public osmium::handler::Handler {
    // Map of Node IDs -> Way IDs to be retrieved later
    const RelationshipData &inputRelationships;

    MappedWayData wayData;

    WayHandler(const RelationshipData &relationshipData) : inputRelationships(relationshipData) {}

    bool isWayInRelationship(const osmium::Way &way) const {
        return inputRelationships.way2Relationships.count(way.id()) > 0;
    }
    bool isWayAValidRoute(const osmium::Way &way) const { return way.tags().get_value_by_key(HIGHWAY_TAG) != nullptr; }

    void way(const osmium::Way &way) noexcept {
        if (!(isWayInRelationship(way) || isWayAValidRoute(way))) {
            return;
        }

        auto &wayData = this->wayData;

        if (isWayAValidRoute(way)) {
            auto tag_value = way.tags().get_value_by_key(HIGHWAY_TAG);
            wayData.id2Tags[way.id()][HIGHWAY_TAG] = tag_value;

            tag_value = way.tags().get_value_by_key(NAME_TAG);
            if (tag_value) {
                wayData.id2Tags[way.id()][NAME_TAG] = tag_value;
            }
        }
        for (size_t ii = 0; ii < way.nodes().size(); ++ii) {
            const auto &node_ref = way.nodes()[ii];
            // Assume that we only get po
            assert(node_ref.ref() > 0);
            auto &nodeMap = wayData.node2Ways[node_ref.ref()];
            nodeMap.emplace(way.id(), ii);
        }
    }
};

struct NodeHandler : public osmium::handler::Handler {

    const osmium::Box &bounds_;
    const MappedWayData &wayData_;
    const RelationshipData &relationshipData_;

    OSMLoader::Id2Way routes_;
    OSMLoader::Id2Relationship areas_;

    NodeHandler(const osmium::Box &bounds, const MappedWayData &wayData, const RelationshipData &relationshipData)
        : bounds_(bounds), wayData_(wayData), relationshipData_(relationshipData) {}

    void node(const osmium::Node &node) noexcept {
        if (!node.location().valid()) {
            return;
        }

        // NOTE: maybe bounds needs to be expanded to include more nodes?
        if (!bounds_.contains(node.location())) {
            return;
        }

        // TODO: check if node is in relationship

        // check if node is in way
        auto it = wayData_.node2Ways.find(node.id());
        if (it == wayData_.node2Ways.end()) {
            return;
        }
        // This node is part of one or more requested ways
        for (const auto &way : it->second) {
            // Find or create the route for this wayID
            auto &route = routes_[way.wayID];
            if (route.nodes.size() <= way.nodeIndex) {
                route.nodes.resize(way.nodeIndex + 1);
            }
            route.nodes[way.nodeIndex] = node.location();
            route.id = way.wayID;
            // TODO: fix this
            route.tags[NAME_TAG] =
                wayData_.id2tags[NAME_TAG].count(way.wayID) > 0 ? wayData_.way2Name.at(way.wayID) : "";
        }
        if (route.type.empty()) {
            route.type = wayData_.highway2Type.count(way.wayID) > 0 ? wayData_.highway2Type.at(way.wayID) : "";
        }
    }
}
};

} // namespace

OSMLoader::Id2Way OSMLoader::getWays(const CoordinateBounds &bounds) const {
    Id2Way routes;
    Id2Relationship relationships;

    RelationshipData relationshipData;

    if (filepath_.empty()) {
        std::cerr << "No input file specified." << std::endl;
        return routes;
    }

    try {
        const osmium::io::File input_file{filepath_};

        // 1) Generate a mapping of ways&nodes to relationships
        osmium::io::Reader relationshipReader{input_file, osmium::osm_entity_bits::relation};
        RelationshipHandler relationshipHandler;
        osmium::apply(relationshipReader, relationshipHandler);
        relationshipReader.close();
        const auto &relationshipData = relationshipHandler.relationshipData;

        // 2) generate a mapping of node to ways
        osmium::io::Reader wayReader{input_file, osmium::osm_entity_bits::way};
        WayHandler wayHandler(relationshipData);
        osmium::apply(wayReader, wayHandler);
        wayReader.close();
        const auto &wayData = wayHandler.wayData;
        const auto &finalRelationships = wayHandler.finalRelationships;

        //
        // 2) find the nodes which were requested in (1) and are within bounds
        // and build a buffer to hold them
        osmium::io::Reader nodeReader{input_file, osmium::osm_entity_bits::node};
        NodeHandler nodeHandler(bounds, wayHandler.wayData, relationshipData, routes);
        osmium::apply(nodeReader, nodeHandler);
        nodeReader.close();

    } catch (const std::exception &e) {
        std::cerr << e.what() << std::endl;
    }

    // clean up routes to remove any incomplete ways
    for (auto it = routes.begin(); it != routes.end();) {
        auto &way = it->second;
        auto new_end =
            std::remove_if(way.nodes.begin(), way.nodes.end(), [](const Coordinate &loc) { return !loc.valid(); });
        way.nodes.erase(new_end, way.nodes.end());

        if (way.nodes.empty()) {
            it = routes.erase(it);
        } else {
            ++it;
        }
    }

    // Uncomment for data analysis
    // std::unordered_map<std::string, uint32_t> types;
    // for (const auto &entry : routes) {
    //     types[entry.second.type] += 1;
    // }
    // std::cout << "Highway types:" << std::endl;
    // for (const auto &type : types) {
    //     std::cout << type.first << ": " << type.second << std::endl;
    // }

    return routes;
}
