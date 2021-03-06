CMAKE_MINIMUM_REQUIRED(VERSION 3.0)

project(wallyd)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(PKG_CONFIG_USE_CMAKE_PREFIX_PATH on)
set(MACOSX_RPATH on)

# Compile video support (requires libffmpeg)
set(WITH_VIDEO on)

# Do we use the clumsy but powerfull curl interface?
set(WITH_CURL on)

# We want to gently wait for events or poll agressivly (more performant)
set(WITH_KMS_DEMO off)

# We want to gently wait for events or poll agressivly (more performant)
set(WAIT_EV off)
# We lock each call to the ui core (currently required)
set(LOCK_CALL on)

# Set number of compile cpu's
set(MULTITHREADED_BUILD 1)

set(CMAKE_C_FLAGS  "${CMAKE_C_FLAGS} -std=c99 -g")

IF(CMAKE_COMPILER_IS_GNUCC)
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0")
    set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -O0")
ENDIF(CMAKE_COMPILER_IS_GNUCC)

find_package(PkgConfig)
pkg_check_modules(LIBUV libuv>=1.7)
pkg_check_modules(LIBSDL sdl2>=2.0)
pkg_check_modules(LIBSDLTTF SDL2_ttf>=2.0)
pkg_check_modules(LIBSDLIMG SDL2_image>=2.0)
pkg_check_modules(LIBDUV seaduk)
set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} -L../lib -L./lib -lwally")
set(CMAKE_C_FLAGS  "${CMAKE_C_FLAGS} -std=gnu99 -DWITH_SEADUK")
set(CMAKE_MAKE_PROGRAM "${CMAKE_MAKE_PROGRAM} -j${MULTITHREADED_BUILD}")

string(REGEX REPLACE "^([0-9]+)\\.([0-9]+).*$" "\\1" DARWIN_MAJOR_VERSION "${CMAKE_SYSTEM_VERSION}")
string(REGEX REPLACE "^([0-9]+)\\.([0-9]+).*$" "\\2" DARWIN_MINOR_VERSION "${CMAKE_SYSTEM_VERSION}")
message("OS Version : " ${CMAKE_SYSTEM_VERSION})
if(${CMAKE_SYSTEM_NAME} STREQUAL "Darwin")
 if("${DARWIN_MAJOR_VERSION}" LESS 16)
    set(CMAKE_C_FLAGS  "${CMAKE_C_FLAGS} -DWITHOUT_CLOCK_GETTIME")
 endif()
endif()


if(WITH_VIDEO STREQUAL on)
	set(CMAKE_C_FLAGS  "${CMAKE_C_FLAGS} -DWITH_VIDEO")
	pkg_check_modules(LIBSWRE libswresample)
	pkg_check_modules(LIBAVUTIL libavutil)
	pkg_check_modules(LIBAVCODEC libavcodec)
	pkg_check_modules(LIBAVFORMAT libavformat)
	pkg_check_modules(LIBSWSCALE libswscale)
endif()

if(WITH_CURL STREQUAL on)
    pkg_check_modules(LIBCURL libcurl)
endif()

if(WAIT_EV STREQUAL on)
	set(CMAKE_C_FLAGS  "${CMAKE_C_FLAGS} -DWAIT_EV")
endif()
if(WAIT_EV STREQUAL on)
    set(CMAKE_C_FLAGS  "${CMAKE_C_FLAGS} -DLOCK_CALL")
endif()

include_directories(${LIBUV_INCLUDE_DIRS} ${LIBSDL_INCLUDE_DIRS} ${LIBDUV_INCLUDE_DIRS} lib src)
link_directories(${LIBUV_LIBRARY_DIRS} ${LIBSDL_LIBRARY_DIRS})

# TODO : Build Versioning
file (STRINGS "version" CURR_BUILD_NUMBER)
execute_process(COMMAND bin/incrementVersion ${CURR_BUILD_NUMBER} RESULT_VARIABLE CMAKE_BUILD_NUMBER OUTPUT_FILE version)
file (STRINGS "version" CMAKE_BUILD_NUMBER)

string(TIMESTAMP CMAKE_BUILD_DATE "%Y-%m-%d")

set(BIN_INSTALL_DIR "${CMAKE_INSTALL_PREFIX}/bin" CACHE STRING "Directory where lib will install")
set(LIB_INSTALL_DIR "${CMAKE_INSTALL_PREFIX}/lib${LIB_SUFFIX}" CACHE STRING "Directory where lib will install")
set(DATA_INSTALL_DIR "${CMAKE_INSTALL_PREFIX}/etc" CACHE PATH "The directory the configs are installed in")
set(PLUGIN_INSTALL_DIR "${LIB_INSTALL_DIR}/wallyd" CACHE PATH "The directory the plugins are installed in")

if(${CMAKE_SYSTEM_NAME} MATCHES "FreeBSD") 
    set(CMAKE_C_FLAGS  "${CMAKE_C_FLAGS} -DDARWIN")
endif()
if(${CMAKE_SYSTEM_NAME} MATCHES "Darwin") 
    set(CMAKE_C_FLAGS  "${CMAKE_C_FLAGS} -DDARWIN")
endif()
if(${CMAKE_SYSTEM_NAME} MATCHES "Linux") 
    set(CMAKE_C_FLAGS  "${CMAKE_C_FLAGS} -DLINUX")
endif()
if(${CMAKE_SYSTEM_NAME} MATCHES "Windows") 
    set(CMAKE_C_FLAGS  "${CMAKE_C_FLAGS} -DWINDOWS")
endif()

add_subdirectory(lib)
add_subdirectory(src)
add_subdirectory(plugins)

add_dependencies(wallyd wally)

set(PROJECT_DESCRIPTION "Wally Daemon")

set(SYSCONFDIR "${CMAKE_INSTALL_PREFIX}/etc" CACHE PATH "System configuration directory")
set(PREFSDIR ${SYSCONFDIR}/${CMAKE_PROJECT_NAME}/wallyd.d)

configure_file( "${CMAKE_CURRENT_SOURCE_DIR}/etc/wallyd.conf" "${CMAKE_CURRENT_SOURCE_DIR}/wallyd.conf")
configure_file( "${CMAKE_CURRENT_SOURCE_DIR}/lib/autoversion.h.in" "${CMAKE_CURRENT_SOURCE_DIR}/lib/autoversion.h" )
install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/wallyd.conf" DESTINATION ${SYSCONFDIR})
install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/etc/wallystart.conf" DESTINATION ${SYSCONFDIR})
install(DIRECTORY etc/wallyd.d DESTINATION ${SYSCONFDIR})

