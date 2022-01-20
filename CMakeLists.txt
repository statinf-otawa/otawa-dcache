CMAKE_MINIMUM_REQUIRED(VERSION 2.6)

# configuration
set(PLUGIN		"dcache")
set(NAMESPACE	"otawa")
set(FILES		)
set(SOURCES
	"dcache.cpp"
	"dcache_ACS.cpp"
	"dcache_Analysis.cpp"
	"dcache_CLPAccessBuilder.cpp"
	"dcache_EventBuilder.cpp"
	"dcache_MUST.cpp"
	"dcache_PERS.cpp"
)


# C++ flags
if(CMAKE_BUILD_TYPE MATCHES Release)
	add_definitions(-DNDEBUG)
endif()
add_compile_options(-Wall)
if(CMAKE_VERSION LESS "3.1")
	add_compile_options(--std=c++11)
	message(STATUS "C++11 set using cflags")
else()
	set(CMAKE_CXX_STANDARD 11)
	message(STATUS "C++ set using CMAKE_CXX_STANDARD")
endif()


# script
project("${PLUGIN}")


# look for OTAWA
if(NOT OTAWA_CONFIG)
	find_program(OTAWA_CONFIG "otawa-config" DOC "path to otawa-config")
	if(NOT OTAWA_CONFIG)
		message(FATAL_ERROR "ERROR: otawa-config is required !")
	endif()
endif()
message(STATUS "otawa-config at ${OTAWA_CONFIG}")

execute_process(COMMAND "${OTAWA_CONFIG}" --prefix
	OUTPUT_VARIABLE OTAWA_PREFIX 	OUTPUT_STRIP_TRAILING_WHITESPACE)
execute_process(COMMAND "${OTAWA_CONFIG}" --plugdir
	OUTPUT_VARIABLE OTAWA_PLUGDIR	OUTPUT_STRIP_TRAILING_WHITESPACE)
execute_process(COMMAND "${OTAWA_CONFIG}" --make-plug "${PLUGIN}" --cflags
	OUTPUT_VARIABLE OTAWA_CFLAGS 	OUTPUT_STRIP_TRAILING_WHITESPACE)
execute_process(COMMAND "${OTAWA_CONFIG}" --make-plug "${PLUGIN}" --libs -r
	OUTPUT_VARIABLE OTAWA_LDFLAGS 	OUTPUT_STRIP_TRAILING_WHITESPACE)


# plugin definition
include_directories		("${CMAKE_SOURCE_DIR}")
add_library				("${PLUGIN}" MODULE ${SOURCES})
set_property			(TARGET "${PLUGIN}" PROPERTY PREFIX "")
set_property			(TARGET "${PLUGIN}" PROPERTY COMPILE_FLAGS "${OTAWA_CFLAGS}")
target_link_libraries	("${PLUGIN}" "${TARGET_LIB}" "${OTAWA_LDFLAGS}")


# installation
install(TARGETS		"${PLUGIN}"		LIBRARY DESTINATION "${OTAWA_PLUGDIR}/${NAMESPACE}")
install(FILES		"${PLUGIN}.eld"	DESTINATION 		"${OTAWA_PLUGDIR}/${NAMESPACE}")
install(DIRECTORY	"otawa"			DESTINATION			"${OTAWA_PREFIX}/include")

# tests
if(WITH_TEST)
add_subdirectory(test)
endif()
