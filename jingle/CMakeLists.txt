add_library(jingle MODULE jingle.c jingle.h check.c check.h action-handlers.c action-handlers.c register.c register.h sessions.c sessions.h send.c send.h)
include_directories(${CMAKE_SOURCE_DIR} ${CMAKE_BINARY_DIR})
install(TARGETS jingle DESTINATION lib/mcabber)
