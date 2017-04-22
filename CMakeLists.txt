cmake_minimum_required(VERSION 3.0)

# Target C++14
set (CMAKE_CXX_STANDARD 14)

# Make sure warning flags are always set
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -Wall")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -Wall")

# Bring in the headers
include_directories(inc)

# Project Name
project(TomasuloPipeline)

# Add all the files from the directory
file(GLOB SRCS src/*.cxx src/*.cpp src/*.cc)

add_executable(procsim ${SRCS})
