cmake_minimum_required(VERSION 3.5)
cmake_policy(SET CMP0048 NEW)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

project(flashmq-dbus-plugin VERSION 0.0.1 LANGUAGES CXX)

add_compile_options(-Wall -Wno-psabi)

include(GNUInstallDirs)

find_package(PkgConfig REQUIRED)

pkg_check_modules(DBUS REQUIRED IMPORTED_TARGET dbus-1)

add_library(flashmq-dbus-plugin SHARED
  vendor/json.hpp
  vendor/flashmq_plugin.h

  src/dbus_functions.h
  src/flashmq-dbus-plugin.cpp 
  src/dbus_functions.cpp
  src/state.h src/state.cpp
  src/utils.h src/utils.cpp
  src/queuedtasks.cpp src/queuedtasks.h
  src/dbusmessageguard.h src/dbusmessageguard.cpp
  src/types.h src/types.cpp
  src/exceptions.h src/exceptions.cpp
  src/dbusmessageitersignature.h src/dbusmessageitersignature.cpp
  src/dbusutils.h src/dbusutils.cpp
  src/vevariant.h src/vevariant.cpp
  src/shortservicename.h src/shortservicename.cpp
  src/dbuserrorguard.h src/dbuserrorguard.cpp
  src/cachedstring.h src/cachedstring.cpp
  src/dbusmessageiteropencontainerguard.h src/dbusmessageiteropencontainerguard.cpp
  src/boomstring.h src/boomstring.cpp
  src/fdguard.h src/fdguard.cpp
  src/serviceidentifier.h src/serviceidentifier.cpp
  src/dbuspendingmessagecallguard.h src/dbuspendingmessagecallguard.cpp
  src/network.h src/network.cpp
)

add_executable(flashmq-dbus-plugin-tests
  vendor/json.hpp
  vendor/flashmq_plugin.h

  src/flashmq-dbus-plugin-tests.h
  src/dbus_functions.h
  src/dbus_functions.cpp
  src/flashmq-dbus-plugin.cpp
  src/flashmq-dbus-plugin-tests.cpp
  src/state.h src/state.cpp
  src/flashmqfunctionreplacements.cpp src/flashmqfunctionreplacements.h
  src/testerglobals.h src/testerglobals.cpp
  src/utils.h src/utils.cpp
  src/queuedtasks.cpp src/queuedtasks.h
  src/dbusmessageguard.h src/dbusmessageguard.cpp
  src/types.h src/types.cpp
  src/exceptions.h src/exceptions.cpp
  src/dbusmessageitersignature.h src/dbusmessageitersignature.cpp
  src/dbusutils.h src/dbusutils.cpp
  src/vevariant.h src/vevariant.cpp
  src/shortservicename.h src/shortservicename.cpp
  src/dbuserrorguard.h src/dbuserrorguard.cpp
  src/cachedstring.h src/cachedstring.cpp
  src/dbusmessageiteropencontainerguard.h src/dbusmessageiteropencontainerguard.cpp
  src/boomstring.h src/boomstring.cpp
  src/fdguard.h src/fdguard.cpp
  src/serviceidentifier.h src/serviceidentifier.cpp
  src/dbuspendingmessagecallguard.h src/dbuspendingmessagecallguard.cpp
  src/network.h src/network.cpp
)

target_include_directories(flashmq-dbus-plugin PUBLIC ${DBUS_INCLUDE_DIRS} .)
target_include_directories(flashmq-dbus-plugin-tests PUBLIC ${DBUS_INCLUDE_DIRS} .)

target_link_libraries(flashmq-dbus-plugin pthread dbus-1 resolv crypt)

target_link_libraries(flashmq-dbus-plugin-tests pthread dbus-1 resolv crypt)

install(TARGETS flashmq-dbus-plugin-tests RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(TARGETS flashmq-dbus-plugin DESTINATION "${CMAKE_INSTALL_LIBEXECDIR}/flashmq")
