# A T E N T I O N   T H I S   IS  S E C O N D   I M P L E M E N T A T I O N ! ! ! 

cmake_minimum_required(VERSION 3.18.4)
project("smart_rtp_static")

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O0 -g -fPIC")

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

file(GLOB_RECURSE SMART_RTP_SRC ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)

# add_definitions(-DDEBUG_PRINTS)

add_library(smart_rtp_static STATIC ${SMART_RTP_SRC})

add_executable(tcp_server_client_tester ${CMAKE_CURRENT_SOURCE_DIR}/tests/main_tester.cpp)
target_link_libraries(tcp_server_client_tester pthread smart_rtp_static ${CMAKE_DL_LIBS})
