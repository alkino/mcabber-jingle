## Copyright 2010 Cornu Nicolas
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
project(mcabber-jingle C) 
set(PROJECT_VERSION "0.0.1")

## User settable options
#option(USE_LIBNAME "Option Description")
#if(USE_LIBNAME)
#
#else(USE_LIBNAME)
#
#endif(USE_LIBNAME)
 
## Check for build dependencies
find_package(PkgConfig REQUIRED)
pkg_check_modules(GLIB REQUIRED glib-2.0)
pkg_check_modules(LM REQUIRED loudmouth-1.0)
pkg_check_modules(MCABBER REQUIRED mcabber)
link_directories(${GLIB_LIBRARY_DIRS} ${MCABBER_LIBRARY_DIRS})

## Compiler setup
configure_file(config.h.in config.h)
include_directories(SYSTEM ${GLIB_INCLUDE_DIRS} ${MCABBER_INCLUDE_DIRS})

## Target definitions
add_subdirectory(jingle)
add_subdirectory(jingle-filetransfer)
add_subdirectory(jingle-ibb)

## Packaging information
set(CPACK_PACKAGE_NAME mcabber-jingle)
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_VENDOR "alkino")
set(CPACK_PACKAGE_CONTACT "Nicolas Cornu <nicolas.cornu@ensi-bourges.fr>")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Base jingle functions")
set(CPACK_SOURCE_GENERATOR TBZ2)
set(CPACK_GENERATOR DEB CACHE TEXT "Binary package generator, eg DEB, RPM, TGZ, NSIS...")
set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}_${CPACK_PACKAGE_VERSION}_${CPACK_DEBIAN_PACKAGE_ARCHITECTURE}")
set(CPACK_SOURCE_IGNORE_FILES "/\\\\..*;\\\\.swp;~$;/build/;\\\\.tar\\\\.;\\\\.deb;\\\\.so")
include(CPack)
