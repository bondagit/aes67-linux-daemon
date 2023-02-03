cmake_minimum_required(VERSION 3.7.0)

project(aes67-daemon CXX)

option(ENABLE_TESTS "Enable tests. Not recommended when cross-compiling." OFF)
if( ENABLE_TESTS )
    enable_testing()
endif()

option(WITH_AVAHI "Include mDNS support via Avahi" OFF)
option(FAKE_DRIVER "Use fake driver instead of RAVENNA" OFF)
set(CMAKE_CXX_STANDARD 17)

option(WITH_SYSTEMD "Include systemd notify and watchdog support" OFF)

# ravena lkm _should_ be provided by the CLI. Nonetheless, we should be able
# to find it in system dirs too...
if (NOT RAVENNNA_ALSA_LKM_DIR)
    find_path(RAVENNA_ALSA_LKM_DIR "common/MergingRAVENNACommon.h" REQUIRED)
endif()

# use sysroot cpp-http lib unless one was explicitly provided  in cmdline
if (NOT CPP_HTTPLIB_DIR)
    find_path( CPP_HTTPLIB_DIR "httplib.h" REQUIRED)
endif()

find_library(AVAHI_LIBRARY-COMMON NAMES avahi-common)
find_library(AVAHI_LIBRARY-CLIENT NAMES avahi-client)
find_path(AVAHI_INCLUDE_DIR avahi-client/publish.h)
set(AVAHI_LIBRARIES ${AVAHI_LIBRARY-COMMON} ${AVAHI_LIBRARY-CLIENT})
set(AVAHI_INCLUDE_DIRS ${AVAHI_INCLUDE_DIR})

find_package(Boost COMPONENTS system thread log program_options REQUIRED)
include_directories(aes67-daemon ${RAVENNA_ALSA_LKM_DIR}/common ${RAVENNA_ALSA_LKM_DIR}/driver ${CPP_HTTPLIB_DIR} ${Boost_INCLUDE_DIR})
add_definitions( -DBOOST_LOG_DYN_LINK -DBOOST_LOG_USE_NATIVE_SYSLOG )
add_compile_options( -Wall )
set(SOURCES error_code.cpp json.cpp main.cpp session_manager.cpp http_server.cpp config.cpp interface.cpp log.cpp sap.cpp browser.cpp rtsp_client.cpp mdns_client.cpp mdns_server.cpp rtsp_server.cpp utils.cpp)

if(FAKE_DRIVER)
  MESSAGE(STATUS "FAKE_DRIVER")
  add_definitions(-D_USE_FAKE_DRIVER_)
  list(APPEND SOURCES fake_driver_manager.cpp)
else()
  list(APPEND SOURCES driver_handler.cpp driver_manager.cpp)
endif()
add_executable(aes67-daemon ${SOURCES})

if(ENABLE_TESTS)
    add_subdirectory(tests)
endif()

target_link_libraries(aes67-daemon ${Boost_LIBRARIES})
if(WITH_AVAHI)
  MESSAGE(STATUS "WITH_AVAHI")
  add_definitions(-D_USE_AVAHI_)
  include_directories(aes67-daemon ${AVAHI_INCLUDE_DIRS})
  target_link_libraries(aes67-daemon ${AVAHI_LIBRARIES})
endif()

if(WITH_SYSTEMD)
	MESSAGE(STATUS "WITH_SYSTEMD")
	add_definitions(-D_USE_SYSTEMD_)
	target_link_libraries(aes67-daemon systemd)
endif()
