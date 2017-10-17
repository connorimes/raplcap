# Libraries

add_library(raplcap-cray-pm raplcap-cray-pm.c)
if(BUILD_SHARED_LIBS)
  set_target_properties(raplcap-cray-pm PROPERTIES VERSION ${PROJECT_VERSION}
                                                   SOVERSION ${VERSION_MAJOR})
endif()

# pkg-config

set(PKG_CONFIG_EXEC_PREFIX "\${prefix}")
set(PKG_CONFIG_LIBDIR "\${prefix}/${CMAKE_INSTALL_LIBDIR}")
set(PKG_CONFIG_INCLUDEDIR "\${prefix}/${CMAKE_INSTALL_INCLUDEDIR}/${PROJECT_NAME}")
set(PKG_CONFIG_CFLAGS "-I\${includedir}")

set(PKG_CONFIG_NAME "raplcap-cray-pm")
set(PKG_CONFIG_DESCRIPTION "Implementation of RAPLCap that uses Cray Power Monitor")
set(PKG_CONFIG_REQUIRES_PRIVATE "")
set(PKG_CONFIG_LIBS "-L\${libdir} -lraplcap-cray-pm")
set(PKG_CONFIG_LIBS_PRIVATE "")
configure_file(
  ${CMAKE_SOURCE_DIR}/pkgconfig.in
  ${CMAKE_BINARY_DIR}/raplcap-cray-pm.pc)

# Install

install(TARGETS raplcap-cray-pm DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(FILES ${CMAKE_BINARY_DIR}/raplcap-cray-pm.pc DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)
