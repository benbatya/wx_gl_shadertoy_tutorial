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

#include <cstdint> // for std::uint64_t
#include <exception>
#include <iostream> // for std::cout, std::cerr
#include <unordered_set>

// // Handler derive from the osmium::handler::Handler base class. Usually you
// // overwrite functions node(), way(), and relation(). Other functions are
// // available, too. Read the API documentation for details.
// struct CountHandler : public osmium::handler::Handler {

//     std::uint64_t nodes = 0;
//     std::uint64_t ways = 0;
//     std::uint64_t relations = 0;

//     // This callback is called by osmium::apply for each node in the data.
//     void node(const osmium::Node& /*node*/) noexcept
//     {
//         ++nodes;
//     }

//     // This callback is called by osmium::apply for each way in the data.
//     void way(const osmium::Way& /*way*/) noexcept
//     {
//         ++ways;
//     }

//     // This callback is called by osmium::apply for each relation in the
//     data. void relation(const osmium::Relation& /*relation*/) noexcept
//     {
//         ++relations;
//     }

// }; // struct CountHandler

// bool OSMLoader::Count()
// {
//     if (filepath_.empty()) {
//         std::cerr << "No input file specified." << std::endl;
//         return false;
//     }

//     try {
//         // The Reader is initialized here with an osmium::io::File, but could
//         // also be directly initialized with a file name.
//         const osmium::io::File input_file { filepath_ };
//         osmium::io::Reader reader { input_file };

//         std::cout << "Counting OSM objects in file: " << filepath_ <<
//         std::endl;

//         // Create an instance of our own CountHandler and push the data from
//         the
//         // input file through it.
//         CountHandler handler;
//         osmium::apply(reader, handler);

//         // You do not have to close the Reader explicitly, but because the
//         // destructor can't throw, you will not see any errors otherwise.
//         reader.close();

//         std::cout << "Nodes: " << handler.nodes << std::endl;
//         std::cout << "Ways: " << handler.ways << std::endl;
//         std::cout << "Relations: " << handler.relations << std::endl;

//         // Because of the huge amount of OSM data, some Osmium-based programs
//         // (though not this one) can use huge amounts of data. So checking
//         actual
//         // memore usage is often useful and can be done easily with this
//         class.
//         // (Currently only works on Linux, not macOS and Windows.)
//         const osmium::MemoryUsage memory;

//         std::cout << std::endl
//                   << "Memory used: " << memory.peak() << " MBytes" <<
//                   std::endl;
//     } catch (const std::exception& e) {
//         // All exceptions used by the Osmium library derive from
//         std::exception. std::cerr << e.what() << std::endl; return false;
//     }

//     return true;
// }

namespace {
using NodeID = osmium::object_id_type;
using WayIDs = std::unordered_set<osmium::object_id_type>;
using NodeWaysMap = std::unordered_map<NodeID, WayIDs>;

struct NodeWayMapper : public osmium::handler::Handler {
    // Map of Node IDs -> Way IDs to be retrieved later
    NodeWaysMap requestedNodes_{};
    // TODO: save other way info if needed

    NodeWayMapper() = default;

    void way(const osmium::Way &way) noexcept {
        for (const auto &node_ref : way.nodes()) {
            // Assume that we only get po
            assert(node_ref.ref() > 0);
            auto &nodeList = requestedNodes_[node_ref.ref()];
            nodeList.insert(way.id());
        }
    }
};

struct NodeReducer : public osmium::handler::Handler {

    const osmium::Box &bounds_;
    const NodeWaysMap &nodeWaysMap_;
    OSMLoader::Ways &routes_;

    NodeReducer(const osmium::Box &bounds, const NodeWaysMap &nodeWaysMap,
                OSMLoader::Ways &routes)
        : bounds_(bounds), nodeWaysMap_(nodeWaysMap), routes_(routes) {}

    void node(const osmium::Node &node) noexcept {
        if (!node.location().valid()) {
            return;
        }

        // NOTE: maybe bounds needs to be expanded to include more nodes?
        if (!bounds_.contains(node.location())) {
            return;
        }

        auto it = nodeWaysMap_.find(node.id());
        if (it == nodeWaysMap_.end()) {
            return;
        }
        // This node is part of one or more requested ways
        for (const auto &wayID : it->second) {
            // Find or create the route for this wayID
            routes_[wayID].push_back(node.location());
        }
    }
};

} // namespace

OSMLoader::Ways OSMLoader::getWays(const CoordinateBounds &bounds) const {
    Ways routes;

    if (filepath_.empty()) {
        std::cerr << "No input file specified." << std::endl;
        return routes;
    }

    try {
        const osmium::io::File input_file{filepath_};
        // 1) generate a mapping of node to ways
        osmium::io::Reader reader{input_file, osmium::osm_entity_bits::way};

        NodeWayMapper wayHandler;
        osmium::apply(reader, wayHandler);
        reader.close();

        // 2) find the nodes which were requested in (1) and are within bounds
        // and build a buffer to hold them
        osmium::io::Reader nodeReader{input_file,
                                      osmium::osm_entity_bits::node};
        NodeReducer reducer(bounds, wayHandler.requestedNodes_, routes);
        osmium::apply(nodeReader, reducer);
        nodeReader.close();

    } catch (const std::exception &e) {
        std::cerr << e.what() << std::endl;
    }

    return routes;
}
