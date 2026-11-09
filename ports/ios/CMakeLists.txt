cmake_minimum_required(VERSION 3.25)
set(CMAKE_OSX_DEPLOYMENT_TARGET "15.0" CACHE STRING "Minimum supported iOS version")
if(NOT TARGET ModernGekko::Runtime)
  project(TurbulenceImport LANGUAGES Swift)
endif()
if(NOT CMAKE_SYSTEM_NAME STREQUAL "iOS")
  message(FATAL_ERROR "Configure with -DCMAKE_SYSTEM_NAME=iOS and the Xcode generator")
endif()
add_executable(TurbulenceImport MACOSX_BUNDLE TurbulenceImportApp.swift DiscPicker.swift DiscImporter.swift)
set_target_properties(TurbulenceImport PROPERTIES
  MACOSX_BUNDLE_GUI_IDENTIFIER "io.github.bittahbandit1.turbulence.import"
  MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/Info.plist"
  XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "io.github.bittahbandit1.turbulence.import"
  XCODE_ATTRIBUTE_TARGETED_DEVICE_FAMILY "1,2"
  XCODE_ATTRIBUTE_SWIFT_VERSION "5.0"
  XCODE_ATTRIBUTE_CODE_SIGN_STYLE Automatic
  MACOSX_BUNDLE_BUNDLE_NAME "Turbulence"
  MACOSX_BUNDLE_SHORT_VERSION_STRING "0.1.0"
  MACOSX_BUNDLE_BUNDLE_VERSION "1")

if(TARGET ModernGekko::Runtime)
  if(NOT TURBULENCE_IOS_GENERATED_DIR)
    message(FATAL_ERROR "Gameplay requires personal generated game code")
  endif()
  set(GAME_ID GXEE8P CACHE STRING "" FORCE)
  set(GENERATED_DIR "${TURBULENCE_IOS_GENERATED_DIR}" CACHE PATH "" FORCE)
  set(REL_GENERATED_DIR "${TURBULENCE_IOS_REL_GENERATED_DIR}" CACHE PATH "" FORCE)
  set(RECOMPCORE_MODULE_STATIC ON CACHE BOOL "" FORCE)
  set(RECOMPCORE_MODULE_ENABLE_IPO OFF CACHE BOOL "" FORCE)
  add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../../lib/ModernGekko/vendor/dolphin/module-template"
                   "${CMAKE_CURRENT_BINARY_DIR}/game-module")
  set_source_files_properties(IOSRuntimeServices.mm PROPERTIES COMPILE_OPTIONS "-fobjc-arc")
  target_sources(TurbulenceImport PRIVATE GameplayView.swift TouchControlModel.swift TouchControls.swift GameplayBridge.mm
    "${CMAKE_CURRENT_SOURCE_DIR}/../../lib/ModernGekko/tools/dol_patch.cpp")
  target_include_directories(TurbulenceImport PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/../../src"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../lib/ModernGekko/tools"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../lib/ModernGekko/vendor/dolphin/Source/Core")
  target_compile_options(TurbulenceImport PRIVATE
    "$<$<COMPILE_LANGUAGE:Swift>:-DTURBULENCE_GAMEPLAY>"
    "$<$<COMPILE_LANGUAGE:Swift>:-import-objc-header>"
    "$<$<COMPILE_LANGUAGE:Swift>:${CMAKE_CURRENT_SOURCE_DIR}/GameplayBridge.h>")
  target_link_libraries(TurbulenceImport PRIVATE ModernGekko::Runtime core imgui gGXEE8P_recomp
    "-framework UIKit" "-framework Metal" "-framework QuartzCore"
    "-framework AVFoundation" "-framework GameController")
  target_compile_features(TurbulenceImport PRIVATE cxx_std_23)
  execute_process(COMMAND xcrun --sdk iphoneos --show-sdk-path
    OUTPUT_VARIABLE _turbulence_ios_sdk OUTPUT_STRIP_TRAILING_WHITESPACE COMMAND_ERROR_IS_FATAL ANY)
  target_link_options(TurbulenceImport PRIVATE
    "SHELL:-Xclang-linker -isysroot -Xclang-linker ${_turbulence_ios_sdk}")
  add_custom_command(TARGET TurbulenceImport POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
      "${CMAKE_CURRENT_SOURCE_DIR}/../COPYING"
      "$<TARGET_BUNDLE_DIR:TurbulenceImport>/COPYING"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
      "${CMAKE_CURRENT_SOURCE_DIR}/../../lib/ModernGekko/vendor/dolphin/Data/Sys"
      "$<TARGET_BUNDLE_DIR:TurbulenceImport>/Sys"
    COMMAND ${CMAKE_COMMAND} -E copy
      "${CMAKE_CURRENT_SOURCE_DIR}/../../config/GFX.ini"
      "$<TARGET_BUNDLE_DIR:TurbulenceImport>/GFX.ini"
    VERBATIM)
endif()
