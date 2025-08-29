cmake_minimum_required(VERSION 3.31)
project(ThreadPool)

set(CMAKE_CXX_STANDARD 17)


add_executable(ThreadPool
        threadpool.cpp test.cpp
)
