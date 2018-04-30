cmake_minimum_required(VERSION 2.8)

project( SocketImageSender )

INCLUDE_DIRECTORIES( include )

AUX_SOURCE_DIRECTORY( src DIR_SRCS )

find_package( OpenCV REQUIRED )

find_package( Threads REQUIRED )

SET( TEST_MATH ${DIR_SRCS} )

add_executable( SocketImageSender ${TEST_MATH} )

set(THREADS_PREFER_PTHREAD_FLAG ON)

target_link_libraries( SocketImageSender ${OpenCV_LIBS} Threads::Threads)

target_link_libraries(SocketImageSender Threads::Threads)

add_definitions(-std=c++11)
