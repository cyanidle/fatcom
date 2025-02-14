cmake_minimum_required(VERSION 3.16)

project(fatcom C CXX)

include(CPM.cmake)

CPMAddPackage("gh:cyanidle/describe@3.1")

set(BOOST_INCLUDE_LIBRARIES preprocessor)
CPMAddPackage(
  NAME Boost
  VERSION 1.87.0
  URL https://github.com/boostorg/boost/releases/download/boost-1.87.0/boost-1.87.0-cmake.7z
  URL_HASH SHA256=61d61a3f33283bab8f3736745a4e47c37141f07e413986cd05d1f942c69d529a
  EXCLUDE_FROM_ALL
  OPTIONS
    "BUILD_SHARED_LIBS OFF"
)

add_library(fatcom INTERFACE)
target_link_libraries(fatcom INTERFACE describe)
target_link_libraries(fatcom INTERFACE Boost::preprocessor)
target_include_directories(fatcom INTERFACE include)


add_executable(fatcom-test test.cpp)
target_link_libraries(fatcom-test PRIVATE fatcom)
