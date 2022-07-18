CMAKE_MINIMUM_REQUIRED(VERSION 2.6)
project("dcache")

# configuration
set(SOURCES
	"dcache.cpp"
	"dcache_ACS.cpp"
	"dcache_Analysis.cpp"
	"dcache_CLPAccessBuilder.cpp"
	"dcache_EventBuilder.cpp"
	"dcache_MultiPERS.cpp"
	"dcache_MAY.cpp"
	"dcache_MUST.cpp"
	"dcache_PERS.cpp"
	"dcache_CategoryBuilder.cpp"
)


# C++ flags
if(CMAKE_BUILD_TYPE MATCHES Release)
	add_definitions(-DNDEBUG)
endif()
add_compile_options(-Wall)

# look for OTAWA
if(NOT OTAWA_CONFIG)
	find_program(OTAWA_CONFIG "otawa-config" DOC "path to otawa-config")
endif()
execute_process(COMMAND "${OTAWA_CONFIG}" --cmake OUTPUT_VARIABLE OTAWA_CMAKE)
string(STRIP "${OTAWA_CMAKE}" OTAWA_CMAKE)
include("${OTAWA_CMAKE}")

# build all
OTAWA_PLUGIN("dcache" "otawa" "${SOURCES}")
OTAWA_INSTALL_INCLUDE("otawa")

# tests
if(WITH_TEST)
	add_subdirectory(test)
endif()
