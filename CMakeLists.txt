cmake_minimum_required (VERSION 2.6)
project (libmumbleclient)
set (libmumbleclient_VERSION_MAJOR 0)
set (libmumbleclient_VERSION_MINOR 0)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

find_package(Boost 1.38.0 REQUIRED COMPONENTS system)
find_package(OpenSSL REQUIRED)
find_package(Protobuf REQUIRED)

SET(CMAKE_BUILD_TYPE Debug)
SET(CMAKE_C_FLAGS_DEBUG "-g -Wall -Wextra -pedantic -Wfatal-errors -Wshadow")
SET(CMAKE_CXX_FLAGS_DEBUG "-g -Wall -Wextra -Wfatal-errors -Wshadow -Woverloaded-virtual -Wold-style-cast")

include_directories(${Boost_INCLUDE_DIRS} ${OPENSSL_INCLUDE_DIR})

PROTOBUF_GENERATE_CPP(PROTO_SRCS PROTO_HDRS Mumble.proto)

add_library(mumbleclient client.cc client_lib.cc ${PROTO_SRCS} ${PROTO_HDRS})
add_executable (main main.cc)
target_link_libraries (main mumbleclient ${Boost_LIBRARIES} ${OPENSSL_LIBRARIES} ${PROTOBUF_LIBRARY})
