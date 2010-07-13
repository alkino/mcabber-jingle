add_library(jingle-inbandbytestream MODULE ibb.c ibb.h)
set_target_properties(jingle-inbandbytestream PROPERTIES COMPILE_FLAGS "-Wall")
include_directories(${CMAKE_SOURCE_DIR})
install(TARGETS jingle-inbandbytestream DESTINATION lib/mcabber)
