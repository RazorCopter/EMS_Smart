# M3A Negative Compile Test
# Verifies setForceReadOnly() is unavailable under AFORE_SHADOW_BUILD

# Attempt to compile disable_guard.cpp - should FAIL
try_compile(COMPILE_RESULT
    ${CMAKE_CURRENT_BINARY_DIR}
    SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/disable_guard.cpp
    CXX_STANDARD 17
    COMPILE_DEFINITIONS -DAFORE_SHADOW_BUILD=1
    CMAKE_FLAGS 
        "-DINCLUDE_DIRECTORIES=${CMAKE_CURRENT_SOURCE_DIR}/../..;${CMAKE_CURRENT_SOURCE_DIR}/.."
    OUTPUT_VARIABLE COMPILE_OUTPUT
)

if(COMPILE_RESULT)
    message(FATAL_ERROR "NEGATIVE TEST FAILED: disable_guard.cpp compiled successfully when it should have failed")
else()
    message(STATUS "NEGATIVE TEST PASSED: setForceReadOnly() correctly unavailable under AFORE_SHADOW_BUILD")
endif()

# Save compile output for inspection
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/negative_compile_output.txt "${COMPILE_OUTPUT}")
