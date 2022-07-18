set(PREFIX "arm-none-eabi-")
set(CC "${PREFIX}gcc")
set(CFLAGS "-static")
set(OFLAGS
	"require:otawa::clp::FILTER_FEATURE"
	"require:otawa::dcache::CLP_ACCESS_FEATURE"
	"--dump"
	"--add-prop" "otawa::CACHE_CONFIG_PATH=cache-16-4-12.xml"
	"-v"
)
set(EVENT_FLAGS
	${OFLAGS}
	"require:otawa::dcache::MULTI_PERS_FEATURE"
	"require:otawa::dcache::MAY_FEATURE"
	"require:otawa::dcache::EVENTS_FEATURE"
)

set(TESTS
	"singlevar"
	"array"
	"pointer"
	"pointer2"
	"multitop"
)

list(TRANSFORM TESTS APPEND ".elf" OUTPUT_VARIABLE ELVES)
add_custom_target(elfves ALL DEPENDS ${ELVES})

add_custom_target(test-options
	COMMAND "echo" ${EVENT_FLAGS}
)

foreach(TEST IN LISTS TESTS)
	add_custom_command(
		OUTPUT "${TEST}.elf"
		COMMAND "${CC}"
		ARGS ${CFLAGS} "${TEST}.s" -o "${TEST}.elf"
		MAIN_DEPENDENCY "${TEST}.s"
	)
	add_custom_target(test-access-${TEST}
		DEPENDS "${TEST}.elf"
		COMMAND "operform" "${TEST}.elf" ${OFLAGS}
	)
	add_custom_target(test-must-${TEST}
		DEPENDS "${TEST}.elf"
		COMMAND "operform" "${TEST}.elf" ${OFLAGS} "require:otawa::dcache::MUST_FEATURE"
	)

	add_custom_target(test-pers-${TEST}
		DEPENDS "${TEST}.elf"
		COMMAND "operform" "${TEST}.elf" ${OFLAGS} "require:otawa::dcache::PERS_FEATURE"
	)

	add_custom_target(test-multi-${TEST}
		DEPENDS "${TEST}.elf"
		COMMAND "operform" "${TEST}.elf" ${OFLAGS} "require:otawa::dcache::MULTI_PERS_FEATURE"
	)

	add_custom_target(test-may-${TEST}
		DEPENDS "${TEST}.elf"
		COMMAND "operform" "${TEST}.elf" ${OFLAGS} "require:otawa::dcache::MAY_FEATURE"
	)

	add_custom_target(test-event-${TEST}
		DEPENDS "${TEST}.elf"
		COMMAND "operform" "${TEST}.elf" ${EVENT_FLAGS}
	)
	
endforeach()
