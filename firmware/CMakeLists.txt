# 顶层 CMakeLists —— 引入 ESP-IDF 构建系统
cmake_minimum_required(VERSION 3.16)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(stock_watcher)
