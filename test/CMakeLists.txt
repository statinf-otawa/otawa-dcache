set(PREFIX "arm-none-eabi-")
set(CC "${PREFIX}gcc")
set(CFLAGS "-static")
set(OFLAGS
	"require:otawa::clp::FILTER_FEATURE"
	"require:otawa::dcache::CLP_BLOCK_FEATURE"
	"-P" "--dump"
	"--add-prop" "otawa::CACHE_CONFIG_PATH=cache-16-4-12.xml"
	"-v"
)

set(TESTS
	"singlevar"
	"array"
	"pointer"
	"pointer2"
)

list(TRANSFORM TESTS APPEND ".elf" OUTPUT_VARIABLE ELVES)
add_custom_target(elfves ALL DEPENDS ${ELVES})


foreach(TEST IN LISTS TESTS)
	add_custom_command(
		OUTPUT "${TEST}.elf"
		COMMAND "${CC}"
		ARGS ${CFLAGS} "${TEST}.s" -o "${TEST}.elf"
		MAIN_DEPENDENCY "${TEST}.s"
	)
	add_custom_target(test-${TEST}
		DEPENDS "${TEST}.elf"
		COMMAND "operform" "${TEST}.elf" ${OFLAGS}
	)	
endforeach()
