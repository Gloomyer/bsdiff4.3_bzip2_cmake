## 编译配置

修改 cmakelists.txt

include_directories(D:/Tools/jdk1.8/include)

include_directories(D:/Tools/jdk1.8/include/win32/)


## 编译方式

编译成可执行文件

add_executable(diff_server ${SRC_SOURCE} ${SRC_MAIN})

编译成动态链接库

add_library(diff_server SHARED ${SRC_SOURCE} ${SRC_MAIN})
target_link_libraries(diff_server)