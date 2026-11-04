# Prevent duplicate VST3 target registration.
if(TARGET HEGVSTBridge)
    message(STATUS "[HEG] VST3 target already exists")
    return()
endif()

cmake_minimum_required(VERSION 3.25)

project(HEGVSTBridgePlugin VERSION 0.1.0 LANGUAGES CXX)

smtg_add_vst3plugin(HEGVSTBridge
    processor.cpp
    processor.h
    controller.cpp
    controller.h
    factory.cpp
    ids.h
    bridge_client.h
)

target_compile_features(HEGVSTBridge PRIVATE cxx_std_17)

target_link_libraries(HEGVSTBridge
    PRIVATE sdk
)

if(MINGW)
    target_link_options(HEGVSTBridge PRIVATE
        -static
        -static-libgcc
        -static-libstdc++
    )
endif()

# Windows-Cross-Build: Linux-Einstiegspunkt durch Windows ersetzen
if(WIN32)
    get_target_property(_heg_sources HEGVSTBridge SOURCES)

    list(FILTER _heg_sources
        EXCLUDE REGEX "(^|/)linuxmain\\.cpp$"
    )

    set_property(TARGET HEGVSTBridge
        PROPERTY SOURCES "${_heg_sources}"
    )

    target_sources(HEGVSTBridge PRIVATE
        "${SDK_ROOT}/public.sdk/source/main/dllmain.cpp"
    )
endif()
