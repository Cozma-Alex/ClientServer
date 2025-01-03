cmake_minimum_required(VERSION 3.10)

if(POLICY CMP0167)
    cmake_policy(SET CMP0167 NEW)
endif()

project(competition_system)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")

find_package(Boost REQUIRED COMPONENTS system thread)
include_directories(${Boost_INCLUDE_DIRS} ${CMAKE_CURRENT_SOURCE_DIR}/include)

add_library(competition_lib
    src/server.cpp)

add_executable(server 
    src/server_main.cpp)

add_executable(client
    src/client_main.cpp
    src/client.cpp)

target_link_libraries(competition_lib PUBLIC ${Boost_LIBRARIES} pthread)
target_link_libraries(server PRIVATE competition_lib)
target_link_libraries(client PRIVATE ${Boost_LIBRARIES} pthread)