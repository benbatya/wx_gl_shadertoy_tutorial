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

#include <cstdint> // for std::uint64_t
#include <exception>
#include <iostream> // for std::cout, std::cerr

// Allow any format of input files (XML, PBF, ...)
#include <osmium/io/any_input.hpp>

// We want to use the handler interface
#include <osmium/handler.hpp>

// Utility class gives us access to memory usage information
#include <osmium/util/memory.hpp>

// For osmium::apply()
#include <osmium/visitor.hpp>

// Handler derive from the osmium::handler::Handler base class. Usually you
// overwrite functions node(), way(), and relation(). Other functions are
// available, too. Read the API documentation for details.
struct CountHandler : public osmium::handler::Handler {

    std::uint64_t nodes = 0;
    std::uint64_t ways = 0;
    std::uint64_t relations = 0;

    // This callback is called by osmium::apply for each node in the data.
    void node(const osmium::Node& /*node*/) noexcept
    {
        ++nodes;
    }

    // This callback is called by osmium::apply for each way in the data.
    void way(const osmium::Way& /*way*/) noexcept
    {
        ++ways;
    }

    // This callback is called by osmium::apply for each relation in the data.
    void relation(const osmium::Relation& /*relation*/) noexcept
    {
        ++relations;
    }

}; // struct CountHandler

bool OSMLoader::Count()
{
    if (filepath_.empty()) {
        std::cerr << "No input file specified." << std::endl;
        return false;
    }

    try {
        // The Reader is initialized here with an osmium::io::File, but could
        // also be directly initialized with a file name.
        const osmium::io::File input_file { filepath_ };
        osmium::io::Reader reader { input_file };

        std::cout << "Counting OSM objects in file: " << filepath_ << std::endl;

        // Create an instance of our own CountHandler and push the data from the
        // input file through it.
        CountHandler handler;
        osmium::apply(reader, handler);

        // You do not have to close the Reader explicitly, but because the
        // destructor can't throw, you will not see any errors otherwise.
        reader.close();

        std::cout << "Nodes: " << handler.nodes << std::endl;
        std::cout << "Ways: " << handler.ways << std::endl;
        std::cout << "Relations: " << handler.relations << std::endl;

        // Because of the huge amount of OSM data, some Osmium-based programs
        // (though not this one) can use huge amounts of data. So checking actual
        // memore usage is often useful and can be done easily with this class.
        // (Currently only works on Linux, not macOS and Windows.)
        const osmium::MemoryUsage memory;

        std::cout << std::endl
                  << "Memory used: " << memory.peak() << " MBytes" << std::endl;
    } catch (const std::exception& e) {
        // All exceptions used by the Osmium library derive from std::exception.
        std::cerr << e.what() << std::endl;
        return false;
    }

    return true;
}

OSMLoader::Routes OSMLoader::getRoutes(const CoordinateBounds& bounds) const
{
    Routes routes;

    if (filepath_.empty()) {
        std::cerr << "No input file specified." << std::endl;
        return routes;
    }

    try {
        const osmium::io::File input_file { filepath_ };
        osmium::io::Reader reader { input_file };

        struct RouteHandler : public osmium::handler::Handler {
            const CoordinateBounds& bounds;
            Routes& routes_ref;

            RouteHandler(const CoordinateBounds& b, Routes& r)
                : bounds(b)
                , routes_ref(r)
            {
            }

            static bool contains(const CoordinateBounds& b, const Coordinate& c)
            {
                return b.first.first <= c.first && c.first <= b.second.first && b.first.second <= c.second && c.second <= b.second.second;
            }

            void way(const osmium::Way& way) noexcept
            {
                std::vector<Coordinate> coords;
                for (const auto& node_ref : way.nodes()) {
                    if (node_ref.location()) {
                        Coordinate coord { node_ref.location().lat(),
                            node_ref.location().lon() };
                        if (contains(bounds, coord)) {
                            coords.push_back(coord);
                        }
                    }
                }
                if (!coords.empty()) {
                    routes_ref.push_back(coords);
                }
            }
        };

        RouteHandler handler(bounds, routes);
        osmium::apply(reader, handler);
        reader.close();

    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
    }

    return routes;
}
