cmake_minimum_required (VERSION 2.6)
project (libmumbleclient)
set (libmumbleclient_VERSION_MAJOR 0)
set (libmumbleclient_VERSION_MINOR 0)

SET(CMAKE_BUILD_TYPE Debug)
SET(CMAKE_C_FLAGS_DEBUG "-g -Wall -Wextra -pedantic -Wfatal-errors -Wshadow")
SET(CMAKE_CXX_FLAGS_DEBUG "-g -Wall -Wextra -Wfatal-errors -Wshadow -Woverloaded-virtual -Wold-style-cast")

include_directories(/usr/include/boost ${CMAKE_CURRENT_BINARY_DIR})

add_executable (main main.cc client.cc client_lib.cc ${CMAKE_CURRENT_BINARY_DIR}/Mumble.pb.cc)
target_link_libraries (main boost_system pthread ssl crypto protobuf)

add_custom_command (
	OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/Mumble.pb.h ${CMAKE_CURRENT_BINARY_DIR}/Mumble.pb.cc
	COMMAND protoc -I${CMAKE_CURRENT_SOURCE_DIR} --cpp_out=${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/Mumble.proto
)
