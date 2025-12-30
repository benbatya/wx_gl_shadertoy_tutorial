# wx_gl_shadertoy_tutorial

Renders OpenStreetMap (OSM) ways tagged as highways using OpenGL geometry shaders.

![Example](/Sausalito_Ways.png)

This small demo parses an OSM XML file and renders the ways that are tagged with `highway` using Geometry Shaders. The
implementation is focused on demonstrating how to render large vector map datasets on the GPU efficiently using
OpenGL (the same infrastructure available on many mobile GPUs). A Vulkan port could be added later if desired.

**Quick summary:**
- **Input:** an OSM XML file exported from OpenStreetMap
- **Output:** an OpenGL window rendering the highway ways from the OSM file
- **Tested on:** Ubuntu (Linux). Should work on Windows and macOS with the appropriate development packages.

**Table of contents**
- [Requirements](#requirements)
- [Dependencies](#dependencies)
- [Build](#build)
- [Run](#run)
- [Notes](#notes)

## Requirements

- `cmake` (3.x)
- A C++ compiler (`clang`, `gcc`, or MSVC)
- OpenGL development headers

On Debian/Ubuntu you can install the system packages used for development with:

```bash
sudo apt update
sudo apt install build-essential cmake libgtk-3-dev libglu1-mesa-dev
```

On Fedora:

```bash
sudo dnf install cmake gcc-c++ gtk3-devel mesa-libGLU-devel
```

macOS users: install dependencies with Homebrew (for example `brew install cmake gtk+3`), then use the macOS toolchain.

## Dependencies

This project uses a number of C++ libraries (see `CMakeLists.txt` and the `build/` subproject folders). If you run into
missing dependency errors, follow the libosmium dependency guide:

https://osmcode.org/libosmium/manual.html#dependencies

The repo already includes CPM-based dependency provisioning in the CMake configuration used during configure/build.

## Build

Create an out-of-source build directory and build the `main` target:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j8 --target main
```

This produces the executable `build/main`.

## Run

Run the program with a path to an OSM XML file. Example:

```bash
./build/main ~/Downloads/map.osm
```

You should see an OpenGL window rendering the map ways similar to the screenshot above.

## Notes

- The demo currently renders OSM ways tagged with `highway` (roads). It is intended as an educational example of
	geometry shader usage with large vector data.
- The code has been built and run on Ubuntu; platform differences (X11/Wayland/Windows/macOS) may require different
	development packages or small build tweaks.

## License

See the `LICENSE` file in the repository for license details.

## Contact

If you have suggestions, issues, or questions, open an issue in the repository.

