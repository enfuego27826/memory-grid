# Memory Grid — server build.
#
# THIS SESSION builds only the pure functional core + its unit tests (no
# transport / networking). The transport (uWebSockets) and its deps are added in
# a later milestone; see /docs and the build brief §7. Pin cmake 3.31 when
# bootstrapping via pip (`pip install "cmake==3.31.10"`) to avoid the cmake 4.x
# breakage of old transitive `cmake_minimum_required` declarations.
cmake_minimum_required(VERSION 3.20)

# Some fetched deps (doctest v2.4.11) still declare a pre-3.5 minimum, which
# cmake 4.x rejects. Allow them to configure. (With a pinned cmake 3.31 this is a
# no-op.) Set before any project()/FetchContent so it applies to subprojects.
if(NOT DEFINED CMAKE_POLICY_VERSION_MINIMUM)
  set(CMAKE_POLICY_VERSION_MINIMUM 3.5 CACHE STRING "" FORCE)
endif()

project(memory_grid_server LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug)
endif()
add_compile_options(-Wall -Wextra)

# ---- The functional core (zero transport dependencies) --------------------
add_library(mg_core
  core/path_gen.cpp
  core/scoring.cpp
  core/hint_parser.cpp
  core/game_room.cpp
)
# Include root is /server so headers resolve as "core/..." and "protocol.hpp".
target_include_directories(mg_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# ---- Tests (doctest, fetched) ---------------------------------------------
include(FetchContent)
FetchContent_Declare(
  doctest
  GIT_REPOSITORY https://github.com/doctest/doctest.git
  GIT_TAG v2.4.11
  GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(doctest)

enable_testing()
add_executable(mg_tests
  tests/test_main.cpp
  tests/test_path_gen.cpp
  tests/test_scoring.cpp
  tests/test_hint_parser.cpp
  tests/test_game_room.cpp
)
target_link_libraries(mg_tests PRIVATE mg_core doctest::doctest)
add_test(NAME mg_tests COMMAND mg_tests)

# ---- Transport dependency: nlohmann/json (header-only, fetched) -----------
FetchContent_Declare(
  json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG v3.11.3
  GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(json)

# ---- Transport logic library (NO uWebSockets dependency) ------------------
# wire/rooms/dispatch talk only to ITransport, so they build + test without any
# WebSocket library. uWS lives solely in uws_transport.cpp (added with mg_server).
add_library(mg_transport
  transport/wire.cpp
  transport/rooms.cpp
  transport/dispatch.cpp
)
target_link_libraries(mg_transport PUBLIC mg_core nlohmann_json::nlohmann_json)
target_include_directories(mg_transport PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# ---- Transport tests (doctest, FakeRoom + FakeTransport, no real sockets) -
add_executable(mg_transport_tests
  tests/test_main.cpp
  tests/test_transport_wire.cpp
  tests/test_transport_dispatch.cpp
)
target_link_libraries(mg_transport_tests PRIVATE mg_transport doctest::doctest)
add_test(NAME mg_transport_tests COMMAND mg_transport_tests)

# ---- uWebSockets + uSockets (ws:// only) ----------------------------------
# Building the server is opt-in so the (slow, single-core) uWS template build and
# its network fetch don't run for core/transport-logic test cycles.
option(MG_BUILD_SERVER "Build the uWebSockets server executable" ON)
if(MG_BUILD_SERVER)
  # uWS pulls uSockets as a git submodule; FetchContent does NOT recurse by
  # default, hence GIT_SUBMODULES_RECURSE. uWS has no usable CMake, so we only
  # populate and author the uSockets target ourselves.
  FetchContent_Declare(
    uwebsockets
    GIT_REPOSITORY https://github.com/uNetworking/uWebSockets.git
    GIT_TAG v20.62.0
    GIT_SUBMODULES_RECURSE TRUE
    GIT_SHALLOW TRUE
  )
  FetchContent_MakeAvailable(uwebsockets)

  set(USOCKETS_DIR ${uwebsockets_SOURCE_DIR}/uSockets)
  if(NOT EXISTS ${USOCKETS_DIR}/src/libusockets.h)
    message(FATAL_ERROR "uSockets sources missing at ${USOCKETS_DIR}/src — submodule not fetched")
  endif()

  # Compile uSockets directly (NOT its Makefile). Exclude crypto/* for ws://-only.
  file(GLOB USOCKETS_SRC ${USOCKETS_DIR}/src/*.c ${USOCKETS_DIR}/src/eventing/*.c)
  add_library(usockets STATIC ${USOCKETS_SRC})
  target_include_directories(usockets PUBLIC ${USOCKETS_DIR}/src)
  target_compile_definitions(usockets PUBLIC LIBUS_NO_SSL)
  target_compile_options(usockets PRIVATE -w)  # third-party C: silence warnings
  find_package(ZLIB REQUIRED)
  target_link_libraries(usockets PUBLIC ZLIB::ZLIB)

  add_library(uwebsockets_hdr INTERFACE)
  target_include_directories(uwebsockets_hdr INTERFACE ${uwebsockets_SOURCE_DIR}/src)
  target_link_libraries(uwebsockets_hdr INTERFACE usockets)

  add_executable(mg_server
    transport/uws_transport.cpp
    transport/main.cpp
  )
  # uWS headers are warning-heavy; this is the only TU paying the template cost.
  # -O0 + aggressive GC params keep the uWS template compile's peak RAM low enough
  # to build on this 1.8GB box without swap (runtime perf is irrelevant — ~3MB RSS).
  target_compile_options(mg_server PRIVATE -Wno-unused-parameter -O0
    "SHELL:--param ggc-min-expand=10" "SHELL:--param ggc-min-heapsize=32768")
  target_link_libraries(mg_server PRIVATE mg_transport uwebsockets_hdr)
  target_include_directories(mg_server PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
endif()
