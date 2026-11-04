# SPDX-License-Identifier: GPL-2.0-or-later
file(MAKE_DIRECTORY "${TEST_DIR}")
file(WRITE "${TEST_DIR}/packet.txt" "N0CALL>APRS:synthetic bandpass initialization test\n")
execute_process(COMMAND "${GEN_PACKETS}" -o "${TEST_DIR}/input.wav" -
  INPUT_FILE "${TEST_DIR}/packet.txt" RESULT_VARIABLE generated
  OUTPUT_QUIET ERROR_VARIABLE generation_error TIMEOUT 15)
if(NOT generated EQUAL 0)
  message(FATAL_ERROR "Synthetic waveform generation failed: ${generation_error}")
endif()
foreach(profile IN ITEMS A Q)
  if(profile STREQUAL "A")
    set(baud 1200)
  else()
    set(baud 2400)
  endif()
  foreach(value IN ITEMS nan inf 3e38)
    execute_process(COMMAND "${CMAKE_COMMAND}" -E env "TUNE_PRE_BAUD=${value}"
      "${ATEST}" -B "${baud}" -P "${profile}" "${TEST_DIR}/input.wav"
      RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error TIMEOUT 15)
    if(NOT result EQUAL 1 OR NOT "${output}${error}" MATCHES "Invalid bandpass filter configuration or tuning; initialization stopped")
      message(FATAL_ERROR "Profile ${profile} did not reject malformed tuning ${value}; result=${result}")
    endif()
  endforeach()
endforeach()
file(REMOVE "${TEST_DIR}/packet.txt" "${TEST_DIR}/input.wav")
