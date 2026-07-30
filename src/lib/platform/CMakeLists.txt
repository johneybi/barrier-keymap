# barrier -- mouse and keyboard sharing utility
# Copyright (C) 2012-2016 Symless Ltd.
# Copyright (C) 2009 Nick Bolton
#
# This package is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# found in the file LICENSE that should have accompanied this file.
#
# This package is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

if (WIN32)
    file(GLOB headers "MSWindows*.h" "ImmuneKeysReader.h" "synwinhk.h")
    file(GLOB sources "MSWindows*.cpp" "ImmuneKeysReader.cpp")
elseif (APPLE)
    file(GLOB headers "OSX*.h" "IOSX*.h")
    file(GLOB sources "OSX*.cpp" "IOSX*.cpp" "OSX*.m" "OSX*.mm")
    option(BARRIER_ENABLE_MAC_VIRTUAL_HID
        "Enable experimental macOS Karabiner VirtualHID keyboard output" OFF)
    set(KARABINER_VHID_REPO "/tmp/Karabiner-DriverKit-VirtualHIDDevice"
        CACHE PATH "Karabiner-DriverKit-VirtualHIDDevice checkout")
elseif (UNIX)
    file(GLOB headers "XWindows*.h")
    file(GLOB sources "XWindows*.cpp")
endif()

if (BARRIER_ADD_HEADERS)
    list(APPEND sources ${headers})
endif()

if (APPLE)
    list(APPEND inc
        /System/Library/Frameworks
    )
endif()

include_directories(${inc})
add_library(platform STATIC ${sources})
target_link_libraries(platform client ${libs})

if (UNIX)
    target_link_libraries(platform io net ipc synlib client ${libs})
endif()

if (APPLE)
    find_library(COCOA_LIBRARY Cocoa)
    target_link_libraries(platform ${COCOA_LIBRARY})
    if (BARRIER_ENABLE_MAC_VIRTUAL_HID)
        if (NOT EXISTS "${KARABINER_VHID_REPO}/include" OR
            NOT EXISTS "${KARABINER_VHID_REPO}/vendor/vendor/include")
            message(FATAL_ERROR
                "Karabiner VirtualHID headers not found. "
                "Set KARABINER_VHID_REPO to a Karabiner-DriverKit-VirtualHIDDevice checkout.")
        endif()

        set_source_files_properties(
            "${CMAKE_CURRENT_SOURCE_DIR}/OSXKeyState.cpp"
            PROPERTIES
                COMPILE_FLAGS "-std=c++23")
        target_compile_definitions(platform PRIVATE BARRIER_ENABLE_MAC_VIRTUAL_HID=1)
        target_include_directories(platform PRIVATE
            "${KARABINER_VHID_REPO}/include"
            "${KARABINER_VHID_REPO}/vendor/vendor/include")
    endif()
endif()
