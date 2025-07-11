# Component CMakeLists.txt for main_controller

# Get all source files
file(GLOB_RECURSE COMPONENT_SRCS "*.cpp" "*.c")

# Register the component
idf_component_register(
    SRCS ${COMPONENT_SRCS}
    INCLUDE_DIRS "."
)
