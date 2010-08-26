add_library(jingle-ft MODULE filetransfer.c)
include_directories(${CMAKE_SOURCE_DIR} ${CMAKE_BINARY_DIR})
install(TARGETS jingle-ft DESTINATION lib/mcabber)
