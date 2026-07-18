cmake_minimum_required(VERSION 3.20)

project(
  pyrelune
  VERSION 0.1.0
  DESCRIPTION "A Python execution kernel for interactive notebooks in Vix Note."
  HOMEPAGE_URL "https://github.com/softadastra/pyrelune"
  LANGUAGES CXX
)

include(GNUInstallDirs)

option(
  PYRELUNE_BUILD_TESTS
  "Build the Pyrelune test suite"
  ${PROJECT_IS_TOP_LEVEL}
)

add_library(pyrelune_core)

target_sources(
  pyrelune_core
  PRIVATE
    src/protocol.cpp
    src/python_process.cpp
    src/kernel.cpp
)

target_compile_features(
  pyrelune_core
  PUBLIC
    cxx_std_20
)

target_include_directories(
  pyrelune_core
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

set_target_properties(
  pyrelune_core
  PROPERTIES
    OUTPUT_NAME pyrelune
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
)

add_executable(
  pyrelune
  src/main.cpp
)

target_compile_features(
  pyrelune
  PRIVATE
    cxx_std_20
)

target_link_libraries(
  pyrelune
  PRIVATE
    pyrelune_core
)

install(
  TARGETS
    pyrelune
    pyrelune_core
  EXPORT pyrelune-targets
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(
  DIRECTORY include/
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(
  EXPORT pyrelune-targets
  FILE pyrelune-targets.cmake
  NAMESPACE pyrelune::
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/pyrelune
)

if(PYRELUNE_BUILD_TESTS)
  include(CTest)

  if(BUILD_TESTING)
    add_subdirectory(tests)
  endif()
endif()
