#-----------------------------------------------------------------------------
#
#  CMake Config
#
#  Osmium Filter Tests
#
#-----------------------------------------------------------------------------

include_directories(include)

add_executable(test_parser test_parser.cpp ../src/object_filter.cpp)
target_link_libraries(test_parser ${OSMIUM_LIBRARIES} ${Boost_LIBRARIES})

add_test(NAME test_parser COMMAND test_parser)

