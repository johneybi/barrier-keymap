# InputLeap -- audio relay core

file(GLOB headers "*.h")
file(GLOB sources "*.cpp")

if(INPUTLEAP_ADD_HEADERS)
    list(APPEND sources ${headers})
endif()

add_library(audio STATIC ${sources})

target_include_directories(audio PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
