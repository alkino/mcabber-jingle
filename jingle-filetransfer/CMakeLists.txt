add_library(jingle-filetransfer MODULE filetransfer.c)
set_target_properties(jingle-filetransfer PROPERTIES COMPILE_FLAGS "-Wall -O0 -g")
include_directories(${CMAKE_SOURCE_DIR})
install(TARGETS jingle-filetransfer DESTINATION lib/mcabber)
