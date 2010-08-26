add_library(jingle-ibb MODULE ibb.c ibb.h)
include_directories(${CMAKE_SOURCE_DIR} ${CMAKE_BINARY_DIR})
install(TARGETS jingle-ibb DESTINATION lib/mcabber)
