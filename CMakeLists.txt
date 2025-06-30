cmake_minimum_required(VERSION 3.10)
project(eda_proyecto)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Include directories for header files
include_directories(${PROJECT_SOURCE_DIR} ${PROJECT_SOURCE_DIR}/Data)

# Source files for the main executable
set(SOURCES
    main.cpp
)

# Add the executable
add_executable(eda_proyecto_app ${SOURCES}) 