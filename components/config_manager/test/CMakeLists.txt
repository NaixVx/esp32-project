set(EXTRA_COMPONENT_DIRS "../../")

# set(TEST_COMPONENTS "config_manager" CACHE STRING "Components to test")

# include($ENV{IDF_PATH}/tools/cmake/project.cmake)

cmake_minimum_required(VERSION 3.16)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
idf_build_set_property(MINIMAL_BUILD ON)
project(config_manager_test)

