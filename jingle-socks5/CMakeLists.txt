add_library(jingle-socks5 MODULE socks5.c socks5.h)
set_target_properties(jingle-socks5 PROPERTIES COMPILE_FLAGS "-Wall -O2 -g")
include_directories(${CMAKE_SOURCE_DIR})
install(TARGETS jingle-socks5 DESTINATION lib/mcabber)
