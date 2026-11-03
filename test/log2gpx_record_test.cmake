# SPDX-License-Identifier: GPL-2.0-or-later

set(input "${TEST_DIR}/log2gpx-record-test.csv")
set(prefix "0,0,2026-11-03T16:00:00Z,N0CALL,N0CALL,,0,,STATION,,51.5,-0.1,,,,,,,,,,")

string(REPEAT "\"\"" 255 doubled_quotes)
file(WRITE "${input}" "${prefix}\"${doubled_quotes}\"\n")
execute_process(COMMAND "${LOG2GPX}" INPUT_FILE "${input}"
                OUTPUT_VARIABLE output ERROR_VARIABLE errors RESULT_VARIABLE result)
if(NOT result EQUAL 0)
  message(FATAL_ERROR "log2gpx rejected a valid bounded quoted record: ${errors}")
endif()
string(FIND "${output}" "<wpt lat=\"51.500000\" lon=\"-0.100000\">" waypoint)
string(FIND "${output}" "<cmt>&quot;" encoded_comment)
if(waypoint EQUAL -1 OR encoded_comment EQUAL -1)
  message(FATAL_ERROR "log2gpx lost the waypoint or escaped comment")
endif()

file(WRITE "${input}" "${prefix}\"first\nsecond\"\n")
execute_process(COMMAND "${LOG2GPX}" INPUT_FILE "${input}"
                OUTPUT_VARIABLE output ERROR_VARIABLE errors RESULT_VARIABLE result)
if(NOT result EQUAL 0)
  message(FATAL_ERROR "log2gpx rejected a valid multiline record: ${errors}")
endif()
string(FIND "${output}" "first\nsecond" multiline_comment)
if(multiline_comment EQUAL -1)
  message(FATAL_ERROR "log2gpx lost a quoted line break")
endif()

file(WRITE "${input}" "0,1,2\n")
execute_process(COMMAND "${LOG2GPX}" INPUT_FILE "${input}"
                OUTPUT_VARIABLE output ERROR_VARIABLE errors RESULT_VARIABLE result)
if(result EQUAL 0 OR output MATCHES "<gpx")
  message(FATAL_ERROR "log2gpx accepted an incomplete record")
endif()
file(REMOVE "${input}")
