
add_executable(osmium-filter main.cpp object_filter.cpp)
target_link_libraries(osmium-filter ${NativeJIT_LIBRARIES} ${OSMIUM_LIBRARIES} ${Boost_LIBRARIES})

add_executable(osmium-filter-test test.cpp object_filter.cpp)
target_link_libraries(osmium-filter-test ${NativeJIT_LIBRARIES} ${OSMIUM_LIBRARIES} ${Boost_LIBRARIES})

add_custom_target(runtest ${CMAKE_BINARY_DIR}/src/osmium-filter-test ${CMAKE_SOURCE_DIR}/test/tests.txt
    DEPENDS osmium-filter-test)

