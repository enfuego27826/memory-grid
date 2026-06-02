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

project(memory_grid_server LANGUAGES CXX)

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
