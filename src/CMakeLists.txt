
add_executable(osmium-filter main.cpp object_filter.cpp)
target_link_libraries(osmium-filter ${OSMIUM_LIBRARIES} ${Boost_LIBRARIES})
set_pthread_on_target(osmium-filter)

add_executable(osmium-filter-fromdump fromdump.cpp object_filter.cpp)
target_link_libraries(osmium-filter-fromdump ${OSMIUM_LIBRARIES} ${Boost_LIBRARIES})
set_pthread_on_target(osmium-filter-fromdump)

add_executable(osmium-filter-test test.cpp object_filter.cpp)
target_link_libraries(osmium-filter-test ${OSMIUM_LIBRARIES} ${Boost_LIBRARIES})
set_pthread_on_target(osmium-filter-test)

add_custom_target(runtest ${CMAKE_BINARY_DIR}/src/osmium-filter-test ${CMAKE_SOURCE_DIR}/test/tests.txt
    DEPENDS osmium-filter-test)

