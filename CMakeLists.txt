cmake_minimum_required(VERSION 3.28...3.30)

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/common/bootstrap.cmake" NO_POLICY_SCOPE)

project(${_name} VERSION ${_version})

option(ENABLE_FRONTEND_API "Use obs-frontend-api for UI functionality" ON)
option(ENABLE_QT "Use Qt functionality" ON)

include(compilerconfig)
include(defaults)
include(helpers)

add_library(${CMAKE_PROJECT_NAME} MODULE)

find_package(libobs REQUIRED)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::libobs)

if(ENABLE_FRONTEND_API)
  find_package(obs-frontend-api REQUIRED)
  target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::obs-frontend-api)
endif()

if(ENABLE_QT)
  find_package(Qt6 REQUIRED COMPONENTS Widgets Core Network)
  target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE Qt6::Core Qt6::Widgets)
  target_compile_options(
    ${CMAKE_PROJECT_NAME}
    PRIVATE $<$<C_COMPILER_ID:Clang,AppleClang>:-Wno-quoted-include-in-framework-header -Wno-comma>
  )
  set_target_properties(
    ${CMAKE_PROJECT_NAME}
    PROPERTIES AUTOMOC ON AUTOUIC ON AUTORCC ON
  )
endif()

target_sources(
  ${CMAKE_PROJECT_NAME}
  PRIVATE
    src/plugin-main.cpp
    src/platform.cpp
    src/platform.hpp
    src/mdns.cpp
    src/mdns.hpp
    src/mdns-socket.cpp
    src/mdns-socket.hpp
    src/viewer.cpp
    src/viewer.hpp
    src/i18n.cpp
    src/i18n.hpp
)
target_compile_features(${CMAKE_PROJECT_NAME} PRIVATE cxx_std_17)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE Qt6::Network)
if(WIN32)
  target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE ws2_32)
  target_sources(${CMAKE_PROJECT_NAME} PRIVATE src/villacast.rc)
endif()

set_target_properties_plugin(${CMAKE_PROJECT_NAME} PROPERTIES OUTPUT_NAME ${_name})

include(CTest)
if(BUILD_TESTING)
  add_executable(
    mdns-tests
    tests/mdns-tests.cpp
    src/mdns.cpp
    src/mdns.hpp
    src/mdns-socket.cpp
    src/mdns-socket.hpp
    src/i18n.cpp
  )
  target_link_libraries(mdns-tests PRIVATE Qt6::Core Qt6::Network)
  target_compile_features(mdns-tests PRIVATE cxx_std_17)
  target_compile_definitions(mdns-tests PRIVATE TEST_LOCALE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data/locale")
  set_target_properties(mdns-tests PROPERTIES AUTOMOC ON)
  add_test(NAME mdns-wire COMMAND mdns-tests)
  add_executable(i18n-tests tests/i18n-tests.cpp src/i18n.cpp)
  target_link_libraries(i18n-tests PRIVATE Qt6::Core)
  target_compile_features(i18n-tests PRIVATE cxx_std_17)
  add_test(NAME i18n COMMAND i18n-tests "${CMAKE_CURRENT_SOURCE_DIR}/data/locale")
  add_executable(viewer-tests tests/viewer-tests.cpp src/viewer.cpp src/i18n.cpp)
  target_link_libraries(viewer-tests PRIVATE Qt6::Core Qt6::Network)
  target_compile_features(viewer-tests PRIVATE cxx_std_17)
  add_test(
    NAME viewer
    COMMAND viewer-tests "${CMAKE_CURRENT_SOURCE_DIR}/data/viewer.html" "${CMAKE_CURRENT_SOURCE_DIR}/data/locale"
  )
  if(WIN32)
    target_link_libraries(mdns-tests PRIVATE ws2_32)
    set_tests_properties(
      mdns-wire
      PROPERTIES ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
    )
    set_tests_properties(i18n PROPERTIES ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>")
    set_tests_properties(
      viewer
      PROPERTIES ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
    )
  endif()
endif()
