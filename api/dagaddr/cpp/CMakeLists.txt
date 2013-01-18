cmake_minimum_required (VERSION 2.6)

# TODO: figure out why this isn't working and set XIADIR for real
# hacky way of finding the root of the XIA source tree
# assumes this script is in a directory off the parent of the XIA tree
# and that the daemons it starts are rooted off of the same location
#execute_process(COMMAND $(dirname $(dirname $(cd ${0%/*} && echo $PWD/${0##*/}))) OUTPUT_VARIABLE XIADIR)
set(XIADIR ../..)

set(CMAKE_INSTALL_PREFIX ${XIADIR})
include_directories(${XIADIR}/include)


# For the C++ version
project(libdagaddr)

add_definitions(-std=c++0x -O2 -g)

add_library(dagaddr SHARED dagaddr.cpp)
add_library(utils utils.cpp)
target_link_libraries(dagaddr utils)
install(TARGETS dagaddr LIBRARY DESTINATION lib)

add_executable(test test.cpp)
target_link_libraries(test dagaddr)



# For the python version
FIND_PACKAGE(SWIG REQUIRED)
INCLUDE(${SWIG_USE_FILE})

FIND_PACKAGE(PythonLibs)
INCLUDE_DIRECTORIES(${PYTHON_INCLUDE_PATH})

INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR})

SET(CMAKE_SWIG_FLAGS "")
SET(CMAKE_SWIG_OUTDIR ../python)

SET_SOURCE_FILES_PROPERTIES(../python/dagaddr.i PROPERTIES CPLUSPLUS ON)
#SET_SOURCE_FILES_PROPERTIES(../python/dagaddr.i PROPERTIES SWIG_FLAGS "-includeall")
SWIG_ADD_MODULE(dagaddr python ../python/dagaddr.i ${CMAKE_CURRENT_SOURCE_DIR}/dagaddr.cpp)  # Used to be dagaddr.cxx -- not sure why that didn't work
SWIG_LINK_LIBRARIES(dagaddr ${PYTHON_LIBRARIES} utils)
INSTALL(FILES ${CMAKE_BINARY_DIR}/${outdir}/../python/dagaddr.py ${CMAKE_BINARY_DIR}/${outdir}/_dagaddr.so
	      DESTINATION lib)

set_directory_properties(PROPERTIES ADDITIONAL_MAKE_CLEAN_FILES ../python/dagaddr.pyc)


# For the documentation
find_package(Doxygen)
if(DOXYGEN_FOUND)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile.in ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile @ONLY)
add_custom_target(doc
${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile
WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
COMMENT "Generating API documentation with Doxygen" VERBATIM
)
endif(DOXYGEN_FOUND)
