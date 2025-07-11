cmake_minimum_required(VERSION 3.16.0)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(PoolControlMain)

# Main component CMakeLists.txt for Pool Controller project

# Get compile definitions to check which environment we're building for
get_directory_property(COMPILE_DEFS COMPILE_DEFINITIONS)

# Debug: Print all compile definitions
message(STATUS "All compile definitions: ${COMPILE_DEFS}")

# Check for HMI_CONTROLLER first (more specific check)
if("HMI_CONTROLLER" IN_LIST COMPILE_DEFS)
    # HMI Controller build - use HMI files
    FILE(GLOB_RECURSE COMPONENT_SRCS 
        ${CMAKE_SOURCE_DIR}/src/hmi/*.cpp
        ${CMAKE_SOURCE_DIR}/src/hmi/*.c
    )
    message(STATUS "Building for HMI Controller")
    message(STATUS "HMI source files: ${COMPONENT_SRCS}")
    
    # Set include directories for HMI
    set(COMPONENT_ADD_INCLUDEDIRS 
        ${CMAKE_SOURCE_DIR}/src/hmi
    )
    
elseif("MAIN_CONTROLLER" IN_LIST COMPILE_DEFS)
    # Main Controller build - use main_controller files
    FILE(GLOB_RECURSE COMPONENT_SRCS 
        ${CMAKE_SOURCE_DIR}/src/main_controller/*.cpp
        ${CMAKE_SOURCE_DIR}/src/main_controller/*.c
    )
    message(STATUS "Building for Main Controller")
    
    # Set include directories for main controller
    set(COMPONENT_ADD_INCLUDEDIRS 
        ${CMAKE_SOURCE_DIR}/src/main_controller
    )
    
else()
    # Default to main controller if no specific controller is defined
    FILE(GLOB_RECURSE COMPONENT_SRCS 
        ${CMAKE_SOURCE_DIR}/src/main_controller/*.cpp
        ${CMAKE_SOURCE_DIR}/src/main_controller/*.c
    )
    message(STATUS "Building for Main Controller (default)")
    
    # Set include directories for main controller
    set(COMPONENT_ADD_INCLUDEDIRS 
        ${CMAKE_SOURCE_DIR}/src/main_controller
    )
endif()

# Register component
idf_component_register(SRCS ${COMPONENT_SRCS}
                      INCLUDE_DIRS ${COMPONENT_ADD_INCLUDEDIRS})