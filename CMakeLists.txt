cmake_minimum_required(VERSION 3.10)
project(jardeleter)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_BUILD_TYPE Release)

set(CMAKE_CXX_FLAGS_RELEASE "-O2 -DNDEBUG")
set(CMAKE_EXE_LINKER_FLAGS_RELEASE "-s")

add_executable(jardeleter
        main.cpp
        handle_finder.cpp
        utils/handle_finder.h
        utils/nt_types.h
        utils/privileges.cpp
        utils/privileges.h)

target_link_libraries(jardeleter ntdll psapi)
target_link_options(jardeleter PRIVATE -static -static-libgcc -static-libstdc++)
