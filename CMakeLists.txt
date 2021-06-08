cmake_minimum_required(VERSION 3.16)
project(diff_server)

include_directories(D:/Tools/jdk1.8/include)
#include_directories(/home/gloomy/jdk1.8.0_291/include)
include_directories(D:/Tools/jdk1.8/include/win32/)
#include_directories(/home/gloomy/jdk1.8.0_291/include/linux/)

#find_package(BZip2)
#if (BZIP2_FOUND)
#    include_directories(${BZIP_INCLUDE_DIRS})
#    target_link_libraries(diff_server ${BZIP2_LIBRARIES})
#endif (BZIP2_FOUND)

set(CMAKE_CXX_STANDARD 11)

FILE(GLOB SRC_MAIN "./src/*.c" "./src/*.h")
FILE(GLOB SRC_SOURCE "./src/*/*.c" "./src/*/*.h")

#add_executable(diff_server ${SRC_SOURCE} ${SRC_MAIN})

add_library(diff_server SHARED ${SRC_SOURCE} ${SRC_MAIN})
target_link_libraries(diff_server)