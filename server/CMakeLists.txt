cmake_minimum_required(VERSION 3.16)
project(HttpWebServer VERSION 1.0.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find required packages
find_package(Threads REQUIRED)

# Include directories for the algorithm headers
include_directories(${CMAKE_SOURCE_DIR}/..)

# Create executable
add_executable(http_server
    src/main.cpp
    src/http_server.cpp
    src/http_request.cpp
    src/http_response.cpp
    src/algorithm_service.cpp
)

# Include directories
target_include_directories(http_server PRIVATE include)

# Link libraries
target_link_libraries(http_server PRIVATE Threads::Threads)

# Compiler flags
if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(http_server PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Create build directory structure
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/www)

# Copy static files to build directory
configure_file(${CMAKE_SOURCE_DIR}/www/index.html ${CMAKE_BINARY_DIR}/www/index.html COPYONLY) 