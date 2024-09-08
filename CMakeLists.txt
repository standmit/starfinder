cmake_minimum_required(VERSION 3.16.3)
project(starfinder)


add_compile_options(-std=c++17)


find_package(Boost REQUIRED COMPONENTS
    program_options
)

find_package(OpenCV REQUIRED)

find_package(TBB REQUIRED)

include_directories(
    include
    ${Boost_INCLUDE_DIRS}
    ${OpenCV_INCLUDE_DIRS}
)


add_library(${PROJECT_NAME}_parsing SHARED
    src/parsing.cpp
)
target_link_libraries(${PROJECT_NAME}_parsing
    ${Boost_LIBRARIES}
    TBB::tbb
)
set_target_properties(${PROJECT_NAME}_parsing
    PROPERTIES
        OUTPUT_NAME parsing
)

add_executable(${PROJECT_NAME}_render
    src/render.cpp
)
target_link_libraries(${PROJECT_NAME}_render
    ${PROJECT_NAME}_parsing
    ${Boost_LIBRARIES}
    ${OpenCV_LIBRARIES}
    TBB::tbb
)
set_target_properties(${PROJECT_NAME}_render
    PROPERTIES
        OUTPUT_NAME render
)


install(
    TARGETS
        ${PROJECT_NAME}_parsing
        ${PROJECT_NAME}_render
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)

install(DIRECTORY include/starfinder
    DESTINATION include
)