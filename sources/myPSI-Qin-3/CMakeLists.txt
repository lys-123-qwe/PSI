cmake_minimum_required (VERSION 2.8)

project("libPSI")


if("${CMAKE_CURRENT_SOURCE_DIR}" STREQUAL "${CMAKE_SOURCE_DIR}")

	############################################
	#          If top level cmake              #
	############################################
	if(${CMAKE_VERSION} VERSION_LESS "3.12.0") 
		message("Please consider updating CMake to 3.12+")
	endif()

	set( CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_HOME_DIRECTORY}/bin)
	set( CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_HOME_DIRECTORY}/lib)
	set( CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_HOME_DIRECTORY}/lib)

	############################################
	#           Flag and #defines              #
	############################################
	set(CMAKE_C_FLAGS "-march=native -Wall  -maes -msse2 -msse4.1 -lntl -lgmp -lm -mpclmul -Wfatal-errors -pthread -fPIC -no-pie -Wno-ignored-attributes -Wno-parentheses")
	set(CMAKE_CXX_FLAGS  "${CMAKE_C_FLAGS}  -std=c++14")
	
	# Select flags.
	SET(CMAKE_CXX_FLAGS_RELEASE "-O3  -DNDEBUG") 
	SET(CMAKE_CXX_FLAGS_RELWITHDEBINFO " -O2 -g -ggdb -rdynamic")
	SET(CMAKE_CXX_FLAGS_DEBUG  "-O0 -g -ggdb -rdynamic")
	

	############################################
	#           Build mode checks              #
	############################################
	
	# Set a default build type for single-configuration
	# CMake generators if no build type is set.
	if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
	   SET(CMAKE_BUILD_TYPE Release)
	endif()

	if(    NOT "${CMAKE_BUILD_TYPE}" STREQUAL "Release"
       AND NOT "${CMAKE_BUILD_TYPE}" STREQUAL "Debug" 
       AND NOT "${CMAKE_BUILD_TYPE}" STREQUAL "RelWithDebInfo" )
		  
        message(FATAL_ERROR ": Unknown build type - \${CMAKE_BUILD_TYPE}=${CMAKE_BUILD_TYPE}.  Please use one of Debug, Release, or RelWithDebInfo. e.g. call\n\tcmake . -DCMAKE_BUILD_TYPE=Release\n" )        
	endif()
endif()

add_definitions(-DSOLUTION_DIR=\"${CMAKE_SOURCE_DIR}\")


#############################################
#       Library/includes locations          #
#############################################

include_directories("${CMAKE_SOURCE_DIR}")

# default location for libOTe
set(libOTe_Dirs "${CMAKE_SOURCE_DIR}/../libOTe/")
include_directories("${libOTe_Dirs}/") 
include_directories("${libOTe_Dirs}/cryptoTools") 

# default location for miracl
set(Miracl_Dirs "${libOTe_Dirs}/cryptoTools/thirdparty/linux/miracl/")
include_directories("${Miracl_Dirs}") 

# default location for NTL
set(NTL_Dirs "${CMAKE_SOURCE_DIR}/thirdparty/linux/ntl/")
include_directories("${NTL_Dirs}/include/")
include_directories("/usr/local/include/")

# default location for Boost
set(BOOST_ROOT "${libOTe_Dirs}/cryptoTools/thirdparty/linux/boost/")


#############################################
#                CONFIGURE                  #
#############################################




#############################################
#               Build libPSI                 #
#############################################
add_subdirectory(libPSI)
include_directories(libPSI)



#############################################
#              Build Frontend               #
#############################################
add_subdirectory(frontend)


