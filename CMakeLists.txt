cmake_minimum_required(VERSION 3.10)

if(POLICY CMP0167)
    cmake_policy(SET CMP0167 NEW)
endif()

project(competition_system)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Boost REQUIRED COMPONENTS system thread)
include_directories(${Boost_INCLUDE_DIRS} ${CMAKE_SOURCE_DIR}/src)

add_executable(server 
    src/server_main.cpp
    src/server.cpp)

add_executable(client
    src/client_main.cpp
    src/client.cpp)

target_link_libraries(server ${Boost_LIBRARIES} pthread)
target_link_libraries(client ${Boost_LIBRARIES} pthread)