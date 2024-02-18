cmake_minimum_required(VERSION 3.28)
project(tcp)

link_libraries(common http reactor)
aux_source_directory(./src/ SRC)# 搜索源文件
set(LIBRARY_OUTPUT_PATH ${LIBPATH}) # 库生成的路径 LIBPATH

# include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include) # 头文件路径 HEADPATH
include_directories(./include) # 头文件路径 HEADPATH

add_library(tcp STATIC ${SRC}) # 静态库
# add_library(tcp SHARED ${SRC}) # 动态库
