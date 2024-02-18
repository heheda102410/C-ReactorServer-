cmake_minimum_required(VERSION 3.28)
project(HttpWeb)

# 库生成的路径
set(LIBPATH ${PROJECT_SOURCE_DIR}/staticLib)

set(COMMONPATH ${PROJECT_SOURCE_DIR}/common)

include_directories(${PROJECT_SOURCE_DIR}/common/include)
include_directories(${PROJECT_SOURCE_DIR}/http/include)
include_directories(${PROJECT_SOURCE_DIR}/reactor/include)
include_directories(${PROJECT_SOURCE_DIR}/tcp/include)
include_directories(${PROJECT_SOURCE_DIR}/thread/include)

# 给当前节点添加子目录
add_subdirectory(common)
add_subdirectory(http)
add_subdirectory(reactor)
add_subdirectory(tcp)
add_subdirectory(thread)

link_libraries(common http reactor tcp thread -lpthread)
add_executable(server main.c)
# target_link_libraries(server common http reactor tcp thread -lpthread)

# 指定输出的路径
set(HOME ${PROJECT_SOURCE_DIR}) # 定义一个变量用于存储一个绝对路径
set(EXECUTABLE_OUTPUT_PATH ${HOME}/bin) # 将拼接好的路径值设置给 EXECUTABLE_OUTPUT_PATH 变量

