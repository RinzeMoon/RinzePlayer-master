# Additional clean files
cmake_minimum_required(VERSION 3.16)

if("${CONFIG}" STREQUAL "" OR "${CONFIG}" STREQUAL "")
  file(REMOVE_RECURSE
  "CMakeFiles/RinzePlayer_autogen.dir/AutogenUsed.txt"
  "CMakeFiles/RinzePlayer_autogen.dir/ParseCache.txt"
  "RinzePlayer_autogen"
  )
endif()
