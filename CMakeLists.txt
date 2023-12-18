cmake_minimum_required(VERSION 3.0)
project(simget)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)

# Find packages
find_package(imgui CONFIG REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(argparse CONFIG REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
link_directories(/Users/charlie/code/simavr/simavr/obj-arm64-apple-darwin23.1.0)

# Add your source files here
add_executable(simget simget.cpp)

# Include directories for simavr
include_directories(~/code/simavr/)
include_directories(~/code/simavr/simavr/cores)
include_directories(~/code/simavr/simavr/sim)

# Link libraries
target_link_libraries(simget PRIVATE imgui::imgui glfw)
target_link_libraries(simget PRIVATE libsimavr.a) 
target_link_libraries(simget PRIVATE libelf.a) 
