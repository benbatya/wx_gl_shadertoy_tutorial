cmake_minimum_required(VERSION 3.20 FATAL_ERROR)

project(wx_gl_shadertoy_tutorial LANGUAGES CXX)

include(FetchContent)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

message(STATUS "Fetching GLEW...")

if (UNIX AND NOT APPLE) # For Linux
    set(GLEW_EGL TRUE) 
    set(OpenGL_GL_PREFERENCE GLVND)
endif()

FetchContent_Declare(
   glew
    URL https://github.com/nigels-com/glew/releases/download/glew-2.2.0/glew-2.2.0.zip
    SOURCE_SUBDIR build/cmake
    DOWNLOAD_EXTRACT_TIMESTAMP ON
)

FetchContent_MakeAvailable(glew)

set(wxBUILD_SHARED OFF)

message(STATUS "Fetching wxWidgets...")

FetchContent_Declare(
   wxWidgets
   GIT_REPOSITORY https://github.com/wxWidgets/wxWidgets.git
   GIT_TAG v3.2.9
   GIT_SHALLOW ON
)
FetchContent_MakeAvailable(wxWidgets)

message(STATUS "Fetching required Boost modules...")

fetchcontent_declare(
    BoostIterator GIT_REPOSITORY https://github.com/boostorg/iterator.git
    GIT_TAG boost-1.82.0
    GIT_SHALLOW ON
)
fetchcontent_declare(
    BoostVariant GIT_REPOSITORY https://github.com/boostorg/variant.git
    GIT_TAG boost-1.82.0
    GIT_SHALLOW ON
)
fetchcontent_makeavailable(BoostIterator BoostVariant)

message(STATUS "Fetching libexpat...")

FetchContent_Declare(
  libexpat
  GIT_REPOSITORY https://github.com/libexpat/libexpat.git
  GIT_TAG R_2_7_3
  GIT_SHALLOW ON
)
FetchContent_MakeAvailable(libexpat)

message(STATUS "Fetching bzip2...")

FetchContent_Declare(
  bzip2
  GIT_REPOSITORY https://gitlab.com/bzip2/bzip2.git
  GIT_TAG        bzip2-1.0.8 # For production, use a specific tag or commit
)

FetchContent_MakeAvailable(bzip2)

message(STATUS "Fetching protozero...")
FetchContent_Declare(
  protozero
  GIT_REPOSITORY https://github.com/mapbox/protozero.git
  GIT_TAG v1.8.1
  GIT_SHALLOW ON
)
## Disable building protozero tools/tests/examples - they require headers
## to be available in a different layout and can cause include errors
## when using FetchContent in this project.
set(PROTOZERO_BUILD_TESTS OFF CACHE BOOL "Disable protozero tests" FORCE)
set(PROTOZERO_BUILD_TOOLS OFF CACHE BOOL "Disable protozero tools" FORCE)
set(PROTOZERO_BUILD_EXAMPLES OFF CACHE BOOL "Disable protozero examples" FORCE)
FetchContent_MakeAvailable(protozero)

message(STATUS "Fetching libosmium...")

FetchContent_Declare(
  libosmium
  GIT_REPOSITORY https://github.com/osmcode/libosmium.git
  GIT_TAG v2.22.0
  GIT_SHALLOW ON
)
## Ensure libosmium's own CMake module files (e.g. FindOsmium.cmake)
## are available via CMAKE_MODULE_PATH before configuring the subproject.
FetchContent_Populate(libosmium)
list(APPEND CMAKE_MODULE_PATH "${libosmium_SOURCE_DIR}/cmake")
FetchContent_MakeAvailable(libosmium)


set(SRCS src/main.cpp src/openglcanvas.cpp src/osm_loader.cpp)

if(APPLE)
    # create bundle on apple compiles
    add_executable(main MACOSX_BUNDLE ${SRCS})

    # Set a custom plist file for the app bundle - needed for Mac OS Retina display
    set_target_properties(main PROPERTIES MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/src/Info.plist)
else()
    # the WIN32 is needed for Windows in order for it to look for WinMain
    # instead of the main function. This is ignored on other systems,
    # so it works on all platforms
    add_executable(main WIN32 ${SRCS} src/main.exe.manifest)
endif()

find_package(expat CONFIG REQUIRED)

target_include_directories(main PRIVATE ${glew_SOURCE_DIR}/include)
target_include_directories(main PRIVATE ${libosmium_SOURCE_DIR}/include)
target_include_directories(main PRIVATE ${protozero_SOURCE_DIR}/include)

target_link_libraries(main PRIVATE wxcore wxstc wxgl glew_s expat::expat bz2)
