# InputLeap -- separate audio relay process

add_executable(input-leap-audiod
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

if(WIN32)
    target_link_libraries(input-leap-audiod PRIVATE winmm ole32)
elseif(APPLE)
    target_link_libraries(input-leap-audiod PRIVATE
        "-framework CoreFoundation"
        "-framework CoreAudio"
        "-framework AudioToolbox")
elseif(UNIX)
    target_link_libraries(input-leap-audiod PRIVATE pthread dl m)
endif()

install(TARGETS input-leap-audiod DESTINATION bin)
