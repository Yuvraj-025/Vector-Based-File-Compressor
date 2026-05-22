cmake_minimum_required(VERSION 3.16)
project(YVCompressor VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# --- Fetch stb (header-only image I/O) ---
include(FetchContent)
FetchContent_Declare(
    stb
    GIT_REPOSITORY https://github.com/nothings/stb.git
    GIT_TAG master
    CONFIGURE_COMMAND ""
    BUILD_COMMAND ""
)
FetchContent_MakeAvailable(stb)

# --- Source files ---
set(YVC_SOURCES
    src/crypto.cpp
    src/parser.cpp
    src/chunker.cpp
    src/vectorizer.cpp
    src/compressor.cpp
    src/format.cpp
    src/reconstructor.cpp
    src/stb_impl.cpp
)

# --- Library ---
add_library(yvc_lib STATIC ${YVC_SOURCES})
target_include_directories(yvc_lib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${stb_SOURCE_DIR}
)

# --- Executable ---
add_executable(yvc src/main.cpp)
target_link_libraries(yvc PRIVATE yvc_lib)

# --- Platform-specific ---
if(MSVC)
    target_compile_options(yvc_lib PRIVATE /W3 /O2)
    target_compile_options(yvc PRIVATE /W3 /O2)
else()
    target_compile_options(yvc_lib PRIVATE -Wall -Wextra -O2)
    target_compile_options(yvc PRIVATE -Wall -Wextra -O2)
endif()

message(STATUS "YVCompressor v${PROJECT_VERSION} configured")
message(STATUS "  stb headers: ${stb_SOURCE_DIR}")
