if(WIN32)
    # Windows playback receiver milestone.
    add_executable(input-leap-audiod
        AudioProcessWait.h
        AudioReceiver.cpp
        AudioReceiver.h
        input-leap-audiod.cpp
        miniaudio.c
    )
    target_compile_features(input-leap-audiod PRIVATE cxx_std_17)
    target_include_directories(input-leap-audiod PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/ext/aoo/include
        ${CMAKE_SOURCE_DIR}/ext/miniaudio
    )
    target_link_libraries(input-leap-audiod PRIVATE audio Aoo::aoo)
    target_link_libraries(input-leap-audiod PRIVATE winmm ole32)
    install(TARGETS input-leap-audiod DESTINATION bin)
elseif(APPLE)
    # macOS system-audio sender milestone.
    add_executable(input-leap-audios
        AudioProcessWait.h
        MacAudioCapture.h
        MacAudioCapture.mm
        AudioSender.h
        AudioSender.mm
        input-leap-audios.cpp
    )
    set_source_files_properties(AudioSender.mm PROPERTIES
        COMPILE_OPTIONS "-fobjc-arc")
    target_compile_features(input-leap-audios PRIVATE cxx_std_17)
    target_compile_options(input-leap-audios PRIVATE
        "-mmacosx-version-min=13.0")
    target_link_options(input-leap-audios PRIVATE
        "-mmacosx-version-min=13.0")
    target_include_directories(input-leap-audios PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/ext/aoo/include
    )
    target_link_libraries(input-leap-audios PRIVATE
        audio Aoo::aoo
        "-framework AudioToolbox"
        "-framework CoreAudio"
        "-framework ScreenCaptureKit"
        "-framework CoreMedia"
        "-framework CoreFoundation")
    install(TARGETS input-leap-audios DESTINATION bin)
endif()
