add_library(jingle-ibb MODULE ibb.c ibb.h)
set_target_properties(jingle-ibb PROPERTIES COMPILE_FLAGS "-Wall -O2 -g")
include_directories(${CMAKE_SOURCE_DIR})
install(TARGETS jingle-ibb DESTINATION lib/mcabber)
