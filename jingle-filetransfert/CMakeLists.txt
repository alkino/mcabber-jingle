add_library(jingle-filetransfert MODULE filetransfert.c)
set_target_properties(jingle-filetransfert PROPERTIES COMPILE_FLAGS "-Wall")
include_directories(${CMAKE_SOURCE_DIR})
install(TARGETS jingle-filetransfert DESTINATION lib/mcabber)
