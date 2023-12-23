cmake_minimum_required(VERSION 3.5)
project(simget)

set(PROJECT_LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type (default Release)" FORCE)
endif()

if(APPLE)
message("APPLE Build")
set(CMAKE_C_FLAGS "-funsigned-char")
set(CMAKE_C_FLAGS_RELEASE "-mtune=generic -O2 -Wall -pedantic -mstackrealign -flto -fomit-frame-pointer -DNDEBUG -Wno-unknown-pragmas -fexceptions -fstack-clash-protection")
set(CMAKE_CXX_FLAGS_RELEASE "-mtune=generic -O2 -Wall -pedantic -mstackrealign -flto -fomit-frame-pointer -DNDEBUG -Wno-unknown-pragmas -fexceptions -fstack-clash-protection")

set(CMAKE_CXX_FLAGS "-funsigned-char")

else()

set(CMAKE_C_FLAGS "-funsigned-char -static-libgcc")
set(CMAKE_C_FLAGS_RELEASE "-mtune=native -O2 -ftree-loop-vectorize -Wall -pedantic -mstackrealign -flto -fomit-frame-pointer -DNDEBUG -fcf-protection -Wno-unknown-pragmas -fexceptions -fstack-clash-protection -fpie -Wl,-pie -Wl,-z,relro -Wl,-z,defs -Wl,-z,now")
set(CMAKE_CXX_FLAGS_RELEASE "-mtune=native -O2 -ftree-loop-vectorize -Wall -pedantic -mstackrealign -flto -fomit-frame-pointer -DNDEBUG -fcf-protection -Wno-unknown-pragmas -fexceptions -fstack-clash-protection -fpie -Wl,-z,relro -Wl,-z,defs -Wl,-z,now")

set(CMAKE_CXX_FLAGS "-funsigned-char -static-libgcc -static-libstdc++")

endif()

set(CMAKE_C_FLAGS_DEBUG "-O0 -ggdb -fasynchronous-unwind-tables")

set(CMAKE_CXX_FLAGS_DEBUG "-O0 -ggdb -fasynchronous-unwind-tables")

message(CMAKE_BUILD_TYPE="${CMAKE_BUILD_TYPE}")

# Find packages
find_package(imgui CONFIG REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(argparse CONFIG REQUIRED)
find_package(OpenGL REQUIRED)
#find_package(LibElf REQUIRED)
# Find the GLAD package
find_package(glad CONFIG REQUIRED)

# have to change this to match how simavr builds
link_directories(simavr/simavr/build)
link_directories(avrdisas/build)

#mac libelf homebrew. 
link_directories(/System/Volumes/Data/opt/homebrew/lib/)

# Add your source files here
add_executable(simget simget.cpp simgetavr.cpp framebuffer.cpp)

# Include directories for simavr
include_directories(simavr/)
include_directories(simavr/simavr/cores)
include_directories(simavr/simavr/sim)

#  Include directories for avrdisas
include_directories(avrdisas/includes)

# Link libraries
target_link_libraries(simget PRIVATE imgui::imgui glfw)
target_link_libraries(simget PRIVATE OpenGL::GL)
# Link GLAD
target_link_libraries(${PROJECT_NAME} PRIVATE glad::glad)

target_link_libraries(simget PRIVATE libsimavr.a) 
target_link_libraries(simget PRIVATE libelf.a) 
target_link_libraries(simget PRIVATE libavrdisas_static.a) 
