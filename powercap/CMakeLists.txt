# Could compile on any UNIX system, but will only work on Linux
if(NOT ${CMAKE_SYSTEM_NAME} MATCHES "Linux")
  return()
endif()

set(POWERCAP_MIN_VERSION 0.4.0)
find_package(PkgConfig)
if(NOT PKG_CONFIG_FOUND)
  return()
endif()
pkg_check_modules(Powercap IMPORTED_TARGET powercap>=${POWERCAP_MIN_VERSION})
if(NOT Powercap_FOUND)
  return()
endif()

# Libraries

add_library(raplcap-powercap raplcap-powercap.c powercap-intel-rapl.c)
target_include_directories(raplcap-powercap PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/inc>
                                                   $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/${PROJECT_NAME}>)
set_target_properties(raplcap-powercap PROPERTIES PUBLIC_HEADER "${PROJECT_SOURCE_DIR}/inc/raplcap.h")
target_compile_definitions(raplcap-powercap PRIVATE RAPLCAP_IMPL="raplcap-powercap"
                                                    RAPLCAP_ALLOW_DEPRECATED)
target_link_libraries(raplcap-powercap PRIVATE PkgConfig::Powercap)
if(BUILD_SHARED_LIBS)
  set_target_properties(raplcap-powercap PROPERTIES VERSION ${PROJECT_VERSION}
                                                    SOVERSION ${PROJECT_VERSION_MAJOR})
endif()
install(TARGETS raplcap-powercap
        EXPORT RAPLCapTargets
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/${PROJECT_NAME})
add_raplcap_pkg_config(raplcap-powercap "Implementation of RAPLCap that uses libpowercap (powercap)" "powercap >= ${POWERCAP_MIN_VERSION}" "")

# Tests

add_executable(raplcap-powercap-unit-test ${PROJECT_SOURCE_DIR}/test/raplcap-unit-test.c)
target_link_libraries(raplcap-powercap-unit-test PRIVATE raplcap-powercap)
add_test(raplcap-powercap-unit-test raplcap-powercap-unit-test)

# must be run manually
add_executable(raplcap-powercap-integration-test ${PROJECT_SOURCE_DIR}/test/raplcap-integration-test.c)
target_compile_definitions(raplcap-powercap-integration-test PRIVATE RAPLCAP_ALLOW_DEPRECATED)
target_link_libraries(raplcap-powercap-integration-test PRIVATE raplcap-powercap)

# rapl-configure

add_rapl_configure("powercap")
