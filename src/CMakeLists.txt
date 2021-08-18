project (persist)
cmake_minimum_required(VERSION 3.16)

enable_testing()

add_library(persist SHARED persist.cpp persist_unix.cpp shared_data.h ../include/persist.h ../include/persist_unix.h)

include_directories(../include)

set_target_properties(persist PROPERTIES
    CXX_STANDARD 17
    CXX_EXTENSIONS OFF)

add_executable(persist-tests test.cpp)
target_link_libraries(persist-tests persist)

set_target_properties(persist-tests PROPERTIES
    CXX_STANDARD 17
    CXX_EXTENSIONS OFF)


add_test(NAME Persist COMMAND persist-tests)


