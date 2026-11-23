cmake_minimum_required(VERSION 3.28...3.30)

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/common/bootstrap.cmake" NO_POLICY_SCOPE)

project(${_name} VERSION ${_version})

# The dock UI needs both the frontend API and Qt, so they are on by default.
option(ENABLE_FRONTEND_API "Use obs-frontend-api for UI functionality" ON)
option(ENABLE_QT "Use Qt functionality" ON)

include(compilerconfig)
include(defaults)
include(helpers)

add_library(${CMAKE_PROJECT_NAME} MODULE)

target_compile_features(${CMAKE_PROJECT_NAME} PRIVATE cxx_std_17)

find_package(libobs REQUIRED)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::libobs)

if(ENABLE_FRONTEND_API)
  find_package(obs-frontend-api REQUIRED)
  target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::obs-frontend-api)
endif()

if(ENABLE_QT)
  find_package(Qt6 COMPONENTS Widgets Core)
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

target_include_directories(
  ${CMAKE_PROJECT_NAME}
  PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src" "${CMAKE_CURRENT_SOURCE_DIR}/third-party/obs-websocket"
)

target_sources(
  ${CMAKE_PROJECT_NAME}
  PRIVATE
    src/api/websocket-api.cpp
    src/api/websocket-api.hpp
    src/audio/audio-mixer.cpp
    src/audio/audio-mixer.hpp
    src/core/controller.cpp
    src/core/controller.hpp
    src/core/live-state.cpp
    src/core/live-state.hpp
    src/core/obs-utils.cpp
    src/core/obs-utils.hpp
    src/core/pipeline.cpp
    src/core/pipeline.hpp
    src/core/placement.cpp
    src/core/placement.hpp
    src/core/route-store.cpp
    src/core/route-store.hpp
    src/core/routes.cpp
    src/core/routes.hpp
    src/core/settings.cpp
    src/core/settings.hpp
    src/plugin/plugin-main.cpp
    src/plugin/plugin-main.hpp
    src/recording/encoders.cpp
    src/recording/encoders.hpp
    src/recording/recorder.cpp
    src/recording/recorder.hpp
    src/recording/recovery.cpp
    src/recording/recovery.hpp
    src/ui/canvas-tab.cpp
    src/ui/canvas-tab.hpp
    src/ui/dock.cpp
    src/ui/dock.hpp
    src/ui/icons.cpp
    src/ui/icons.hpp
    src/ui/preview-widget.cpp
    src/ui/preview-widget.hpp
    src/ui/record-button.cpp
    src/ui/record-button.hpp
    src/ui/route-button.cpp
    src/ui/route-button.hpp
    src/ui/settings-dialog.cpp
    src/ui/settings-dialog.hpp
    src/ui/simple-routes.cpp
    src/ui/simple-routes.hpp
    src/ui/source-list-buttons.cpp
    src/ui/source-list-buttons.hpp
    src/video/scene-mirror.cpp
    src/video/scene-mirror.hpp
)

set_target_properties_plugin(${CMAKE_PROJECT_NAME} PROPERTIES OUTPUT_NAME ${_name})
