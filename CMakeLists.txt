cmake_minimum_required (VERSION 3.1)

project (AcceleratedFlowCutter)
option (USE_KAHIP "Link KaHiP and at kahip related commands to console." ON)

set (CMAKE_CXX_STANDARD 14)
set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic -Werror=return-type -Wno-stringop-truncation")
set(CMAKE_EXE_LINKER_FLAGS  "${CMAKE_EXE_LINKER_FLAGS} -lm")

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
find_package(TBB REQUIRED)
include_directories(SYSTEM ${TBB_INCLUDE_DIRS})


find_package (OpenMP REQUIRED)
find_library (READLINE NAMES libreadline.a libreadline readline)
if (NOT READLINE)
    message (FATAL_ERROR "readline library not found.")
endif()

if (USE_KAHIP)
    add_subdirectory (extern/KaHIP EXCLUDE_FROM_ALL)
endif()

set (SOURCE_FILES 
    src/console.cpp 
    src/fancy_input.cpp 
    src/permutation.cpp
    src/list_graph.cpp
    src/file_utility.cpp
    src/geo_pos.cpp)

#add_custom_target (routingkit)
#add_custom_target (routingkit DEPENDS ${CMAKE_SOURCE_DIR}/extern/RoutingKit/lib/libroutingkit.a )

add_executable (console ${SOURCE_FILES})
add_executable (customize customize.cpp)
add_executable (query query.cpp)

target_include_directories (customize PRIVATE extern/RoutingKit/include)
target_link_libraries (customize ${CMAKE_SOURCE_DIR}/extern/RoutingKit/lib/libroutingkit.a OpenMP::OpenMP_CXX)

target_include_directories (query PRIVATE extern/RoutingKit/include)
target_link_libraries (query ${CMAKE_SOURCE_DIR}/extern/RoutingKit/lib/libroutingkit.a OpenMP::OpenMP_CXX)

if (USE_KAHIP)
    target_compile_definitions(console PUBLIC USE_KAHIP)
    target_link_libraries (console PUBLIC interface_static)
endif()
target_link_libraries(console PUBLIC ${READLINE})
target_link_libraries(console PUBLIC ${TBB_LIBRARIES})
target_link_libraries(console PUBLIC tinfo)


