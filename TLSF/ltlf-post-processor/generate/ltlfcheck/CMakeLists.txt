cmake_minimum_required(VERSION 3.1)
project(ltlfcheck)

# Set the default value of CMAKE_INSTALL_PREFIX to /usr/local
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(CMAKE_INSTALL_PREFIX /usr/local CACHE PATH "Default installation directory" FORCE)
endif()

set(CMAKE_FIND_LIBRARY_SUFFIXES .a)

find_library (myspot spot)
find_library (mybddx bddx)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wextra -Wall -O3")
include_directories(./src/ /usr/local/include/spot/)
link_directories(/usr/local/lib/)

add_library(symaut STATIC src/utils.cpp src/sym_aut.cc)

add_library(ltlfmodelcheck STATIC src/ltlf_modelcheck.cpp)

add_executable(ltlfcheck src/ltlfcheck.cpp)
target_link_libraries(ltlfcheck PRIVATE ${myspot} ${mybddx} symaut ltlfmodelcheck)

install(TARGETS ltlfcheck DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)
install(TARGETS ltlfmodelcheck DESTINATION ${CMAKE_INSTALL_PREFIX}/lib)
install(FILES src/ltlf_modelcheck.h src/sym_aut.h src/utils.h DESTINATION ${CMAKE_INSTALL_PREFIX}/include)