cmake_minimum_required (VERSION 2.6)
project (libmumbleclient)
set (libmumbleclient_VERSION_MAJOR 0)
set (libmumbleclient_VERSION_MINOR 0)

SET(CMAKE_BUILD_TYPE Debug)

include_directories(/usr/include/nss /usr/include/nspr ${CMAKE_CURRENT_BINARY_DIR})

add_executable (main main.cc ${CMAKE_CURRENT_BINARY_DIR}/Mumble.pb.cc)
target_link_libraries (main nss3 ssl3 protobuf)

add_custom_command (
	OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/Mumble.pb.h ${CMAKE_CURRENT_BINARY_DIR}/Mumble.pb.cc
	COMMAND protoc -I${CMAKE_CURRENT_SOURCE_DIR} --cpp_out=${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/Mumble.proto
)
