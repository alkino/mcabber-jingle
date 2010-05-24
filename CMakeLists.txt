## Copyright 2010 Cornu Nicolas
# This file is part of mcabber module writing howto examples.
#
# Examples are free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

cmake_minimum_required(VERSION 2.6) 
project(Notify) 
set(PROJECT_VERSION "0.0.1")

## User settable options
 
## Check for build dependencies
find_package(PkgConfig REQUIRED) 
pkg_check_modules(GLIB REQUIRED glib-2.0) 
pkg_check_modules(GTK REQUIRED gtk+-2.0) 
pkg_check_modules(LM REQUIRED loudmouth-1.0) 
pkg_check_modules(MCABBER REQUIRED mcabber)
link_directories(${GLIB_LIBRARY_DIRS}
				 ${LM_LIBRARY_DIRS}
				 ${MCABBER_LIBRARY_DIRS}
				 ${GTK_LIBRARY_DIRS})

## Target definitions
add_library(jingle MODULE jingle.c) 

## Compiler setup
#configure_file(config.h.in config.h)
include_directories(SYSTEM ${GLIB_INCLUDE_DIRS} 
                    ${LM_INCLUDE_DIRS}
					${MCABBER_INCLUDE_DIRS}
					${GTK_INCLUDE_DIRS})
target_link_libraries(jingle ${GLIB_LIBRARIES} 
					  ${LM_LIBRARIES}
					  ${MCABBER_LIBRARIES}
					  ${GTK_LIBRARIES}
					 )
include_directories(${notify_SOURCE_DIR} 
                    ${notify_BINARY_DIR})
set_target_properties(jingle PROPERTIES COMPILE_FLAGS "-Wall")

## Packaging information
set(CPACK_PACKAGE_NAME libmcabber-jingle)
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_VENDOR "alkino")
set(CPACK_PACKAGE_CONTACT "Nicolas Cornu <nicolas.cornu@ensi-bourges.fr>")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Some notification for Mcabber")
#set(CPACK_RESOURCE_FILE_LICENSE ${notify_SOURCE_DIR}/COPYING)
set(CPACK_SOURCE_GENERATOR TBZ2)
set(CPACK_GENERATOR DEB CACHE TEXT "Binary package generator, eg DEB, RPM, TGZ, NSIS...")
set(CPACK_DEBIAN_PACKAGE_SECTION libs)
find_program(DPKG_EXECUTABLE dpkg)
if(DPKG_EXECUTABLE)
	execute_process(COMMAND ${DPKG_EXECUTABLE} --print-architecture OUTPUT_VARIABLE CPACK_DEBIAN_PACKAGE_ARCHITECTURE OUTPUT_STRIP_TRAILING_WHITESPACE)
else()
	set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE i386 CACHE TEXT "Architecture of package")
endif()
set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}_${CPACK_PACKAGE_VERSION}_${CPACK_DEBIAN_PACKAGE_ARCHITECTURE}")
set(CPACK_SOURCE_IGNORE_FILES "/\\\\..*;\\\\.swp;~$;/build/;\\\\.tar\\\\.;\\\\.deb;\\\\.so")
include(CPack)

## Installation
install(TARGETS jingle DESTINATION lib/mcabber) 
