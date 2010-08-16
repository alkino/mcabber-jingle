add_library(jingle-ft MODULE filetransfer.c)
set_target_properties(jingle-ft PROPERTIES COMPILE_FLAGS "-Wall -O2 -g")
include_directories(${CMAKE_SOURCE_DIR})
install(TARGETS jingle-ft DESTINATION lib/mcabber)
