cmake_minimum_required(VERSION 3.31)

if(APPLE)
    project(RuinPulse VERSION 0.1.0 LANGUAGES C CXX OBJC OBJCXX)
else()
    project(RuinPulse VERSION 0.1.0 LANGUAGES C CXX)
endif()

option(RUINPULSE_BUILD_PLUGIN "Build the YUP Standalone/VST3/AU targets" ON)
option(RUINPULSE_BUILD_TESTS "Build deterministic DSP regression tests" ON)
option(RUINPULSE_ADHOC_SIGN "Ad-hoc sign local macOS plugin and app bundles after building" ON)
set(RUINPULSE_CREATE_AU OFF)
if(APPLE)
    set(RUINPULSE_CREATE_AU ON)
endif()
set(RUINPULSE_YUP_SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/../yup" CACHE PATH
    "Path to a local YUP checkout; fetched from upstream when unavailable")

add_library(ruinpulse_engine STATIC
    source/RuinPulseEngine.cpp)
target_include_directories(ruinpulse_engine PUBLIC
    "${CMAKE_CURRENT_LIST_DIR}/include")
target_compile_features(ruinpulse_engine PUBLIC cxx_std_20)
set_target_properties(ruinpulse_engine PROPERTIES POSITION_INDEPENDENT_CODE ON)
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(ruinpulse_engine PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(RUINPULSE_BUILD_TESTS)
    include(CTest)

    function(enable_test_assertions target_name)
        if(MSVC)
            target_compile_options(${target_name} PRIVATE /UNDEBUG)
        elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
            target_compile_options(${target_name} PRIVATE -UNDEBUG)
        endif()
    endfunction()

    add_executable(ruinpulse_engine_tests
        tests/RuinPulseEngineTests.cpp)
    target_link_libraries(ruinpulse_engine_tests PRIVATE ruinpulse_engine)
    target_compile_features(ruinpulse_engine_tests PRIVATE cxx_std_20)
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(ruinpulse_engine_tests PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    enable_test_assertions(ruinpulse_engine_tests)
    add_test(NAME ruinpulse_engine_tests COMMAND ruinpulse_engine_tests)
endif()

if(RUINPULSE_BUILD_PLUGIN)
    set(YUP_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(YUP_BUILD_PLUGINS OFF CACHE BOOL "" FORCE)
    set(YUP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(YUP_ENABLE_PLUGINVAL OFF CACHE BOOL "" FORCE)
    set(YUP_ENABLE_VST3_VALIDATOR OFF CACHE BOOL "" FORCE)
    set(YUP_ENABLE_CLAP_VALIDATOR OFF CACHE BOOL "" FORCE)
    set(YUP_ENABLE_AUVAL_VALIDATOR OFF CACHE BOOL "" FORCE)

    if(EXISTS "${RUINPULSE_YUP_SOURCE_DIR}/CMakeLists.txt")
        add_subdirectory("${RUINPULSE_YUP_SOURCE_DIR}" "${CMAKE_BINARY_DIR}/yup")
    else()
        include(FetchContent)
        FetchContent_Declare(yup
            GIT_REPOSITORY https://github.com/kunitoki/yup.git
            GIT_TAG 9a1c9bc699b6a714f6f52486462d98a140c8bf95)
        FetchContent_MakeAvailable(yup)
    endif()

    # YUP's iconutil-based helper rejects its generated iconset on current
    # macOS. Keep the compatibility fix project-local until upstream changes.
    if(APPLE)
        include(cmake/YupMacOSIconWorkaround.cmake)
    endif()

    yup_audio_plugin(
        TARGET_NAME ruinpulse
        TARGET_VERSION ${PROJECT_VERSION}
        TARGET_IDE_GROUP "RuinPulse"
        TARGET_APP_ID "jp.ehl.ruinpulse"
        TARGET_APP_NAMESPACE "jp.ehl"
        TARGET_CXX_STANDARD 20
        PLUGIN_ID "jp.ehl.ruinpulse"
        PLUGIN_NAME "RuinPulse"
        PLUGIN_VENDOR "ehl_"
        PLUGIN_EMAIL ""
        PLUGIN_VERSION "${PROJECT_VERSION}"
        PLUGIN_DESCRIPTION "Phase-distorted pulse-avalanche synth"
        PLUGIN_URL "https://github.com/EsionHsrahLatigid/RuinPulse"
        PLUGIN_IS_SYNTH ON
        PLUGIN_IS_MONO OFF
        PLUGIN_CREATE_CLAP OFF
        PLUGIN_CREATE_VST3 ON
        PLUGIN_CREATE_AU ${RUINPULSE_CREATE_AU}
        PLUGIN_AU_SUBTYPE "RnPu"
        PLUGIN_AU_MANUFACTURER "EHL1"
        PLUGIN_AU_SANDBOX_SAFE ON
        PLUGIN_CREATE_AUv3 OFF
        PLUGIN_CREATE_LV2 OFF
        PLUGIN_CREATE_AAX OFF
        PLUGIN_CREATE_STANDALONE ON
        PLUGIN_COPY_AFTER_BUILD OFF
        MODULES
            yup::yup_gui
            yup::yup_audio_processors)

    target_sources(ruinpulse_shared INTERFACE
        source/ParameterGridEditor.cpp
        source/ParameterGridEditor.h
        source/ProductState.h
        source/RuinPulsePlugin.cpp
        source/RuinPulsePlugin.h)
    target_link_libraries(ruinpulse_shared INTERFACE ruinpulse_engine)
    target_include_directories(ruinpulse_shared INTERFACE
        "${CMAKE_CURRENT_LIST_DIR}/include"
        "${CMAKE_CURRENT_LIST_DIR}/source")

    if(RUINPULSE_BUILD_TESTS)
        add_executable(ruinpulse_plugin_bridge_tests
            tests/RuinPulsePluginBridgeTests.cpp
            source/RuinPulsePlugin.cpp)
        target_link_libraries(ruinpulse_plugin_bridge_tests PRIVATE ruinpulse_engine yup::yup_audio_processors)
        target_include_directories(ruinpulse_plugin_bridge_tests PRIVATE
            "${CMAKE_CURRENT_LIST_DIR}/include"
            "${CMAKE_CURRENT_LIST_DIR}/source")
        target_compile_definitions(ruinpulse_plugin_bridge_tests PRIVATE RUINPULSE_HEADLESS_TEST=1)
        target_compile_features(ruinpulse_plugin_bridge_tests PRIVATE cxx_std_20)
        if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
            target_compile_options(ruinpulse_plugin_bridge_tests PRIVATE -Wall -Wextra -Wpedantic)
        endif()
        enable_test_assertions(ruinpulse_plugin_bridge_tests)
        add_test(NAME ruinpulse_plugin_bridge_tests COMMAND ruinpulse_plugin_bridge_tests)
    endif()

    set(ruinpulse_release_bundle_targets
        ruinpulse_standalone_plugin
        ruinpulse_vst3_plugin)
    if(APPLE)
        list(APPEND ruinpulse_release_bundle_targets ruinpulse_au_plugin)
    endif()

    add_custom_target(ruinpulse_release_bundles
        DEPENDS ${ruinpulse_release_bundle_targets})

    if(APPLE)
        set_target_properties(ruinpulse_standalone_plugin PROPERTIES
            MACOSX_BUNDLE_GUI_IDENTIFIER "jp.ehl.ruinpulse"
            MACOSX_BUNDLE_BUNDLE_NAME "RuinPulse"
            XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "jp.ehl.ruinpulse")
    endif()

    if(APPLE AND RUINPULSE_ADHOC_SIGN)
        find_program(RUINPULSE_CODESIGN_EXECUTABLE codesign REQUIRED)
        add_custom_command(TARGET ruinpulse_vst3_plugin POST_BUILD
            COMMAND "${RUINPULSE_CODESIGN_EXECUTABLE}" --force --deep --sign -
                    "$<TARGET_BUNDLE_DIR:ruinpulse_vst3_plugin>"
            COMMENT "Ad-hoc signing RuinPulse VST3 for local loading"
            VERBATIM)
        add_custom_command(TARGET ruinpulse_standalone_plugin POST_BUILD
            COMMAND "${RUINPULSE_CODESIGN_EXECUTABLE}" --force --deep --sign -
                    "$<TARGET_BUNDLE_DIR:ruinpulse_standalone_plugin>"
            COMMENT "Ad-hoc signing RuinPulse standalone app for local launch"
            VERBATIM)
    endif()
endif()
