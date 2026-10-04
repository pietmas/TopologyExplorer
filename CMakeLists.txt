cmake_minimum_required(VERSION 3.20)
project(topoexplorer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(OpenCASCADE REQUIRED)
find_package(Python REQUIRED COMPONENTS Interpreter Development.Module)
find_package(pybind11 CONFIG REQUIRED)

add_library(topocore STATIC
    core/src/graph.cpp
    core/src/features.cpp)
target_include_directories(topocore PUBLIC core/include ${OpenCASCADE_INCLUDE_DIR})
target_link_libraries(topocore PUBLIC
    TKernel TKMath TKG2d TKG3d TKGeomBase TKBRep TKGeomAlgo TKTopAlgo
    TKPrim TKBO TKBool TKFillet TKOffset TKShHealing TKMesh)
set_target_properties(topocore PROPERTIES POSITION_INDEPENDENT_CODE ON)

pybind11_add_module(topoexplorer bindings/module.cpp)
target_link_libraries(topoexplorer PRIVATE topocore)

install(TARGETS topoexplorer LIBRARY DESTINATION .)
