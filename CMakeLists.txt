project(scorep_plugin_ucx)

cmake_minimum_required(VERSION 3.8)

set(CMAKE_MODULE_PATH "${CMAKE_MODULE_PATH};${CMAKE_CURRENT_SOURCE_DIR}/cmake")

set(UCX_INCLUDE_DIRS "$ENV{UCX_SRC_PATH}/src")

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/common")

include(scorep_plugin_common/DefaultBuildType.cmake)

set(NITRO_POSITION_INDEPENDENT_CODE ON CACHE INTERNAL "")
add_subdirectory(scorep_plugin_cxx_wrapper)

add_library(scorep_plugin_ucx
            SHARED
            src/scorep_plugin_ucx.cpp
            src/utils.cpp
            src/ucx_sampling.cpp)

add_library(scorep_plugin_ucx_profile
            SHARED
            src/scorep_plugin_ucx.cpp
            src/utils.cpp
            src/ucx_sampling.cpp)

set_target_properties(scorep_plugin_ucx PROPERTIES CXX_STANDARD 17)
set_target_properties(scorep_plugin_ucx_profile PROPERTIES CXX_STANDARD 17)


target_include_directories(scorep_plugin_ucx PRIVATE
  src 
  include
  ${UCX_INCLUDE_DIRS})


target_include_directories(scorep_plugin_ucx_profile PRIVATE
  src 
  include
  ${UCX_INCLUDE_DIRS})
  
  
target_compile_options(scorep_plugin_ucx INTERFACE -Wall -pedantic -Wextra) # -fPIC)

target_compile_options(scorep_plugin_ucx_profile INTERFACE -Wall -pedantic -Wextra) #-fPIC)
target_compile_definitions(scorep_plugin_ucx_profile PUBLIC -DSCOREP_PLUGIN_PROFILING_ENABLE)


target_link_libraries(scorep_plugin_ucx PRIVATE 
  Scorep::scorep-plugin-cxx
  "$ENV{UCX_INSTALL_PATH}/lib/libucs.so")
  
target_link_libraries(scorep_plugin_ucx_profile PRIVATE 
  Scorep::scorep-plugin-cxx
  "$ENV{UCX_INSTALL_PATH}/lib/libucs.so")
  

install(TARGETS scorep_plugin_ucx DESTINATION lib)
install(TARGETS scorep_plugin_ucx_profile DESTINATION lib)


add_custom_command(TARGET scorep_plugin_ucx_profile POST_BUILD
    COMMAND rm -rf profile && mkdir profile && cp libscorep_plugin_ucx_profile.so profile/libscorep_plugin_ucx.so 
    COMMENT "Running post build commands..."
)