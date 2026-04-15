cmake_minimum_required(VERSION 3.15)
project(RaylibEnttGame)

# Set C++ standard (EnTT requires at least C++17)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 1. Add Raylib
add_subdirectory(external/raylib)

# 2. Create Executable
add_executable(${PROJECT_NAME} main.cpp)

# 3. Add EnTT single-header include path
target_include_directories(${PROJECT_NAME} PRIVATE external)

# 4. Link Libraries
target_link_libraries(${PROJECT_NAME} PRIVATE raylib)