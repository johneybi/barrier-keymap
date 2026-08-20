/*
 * InputLeap -- mouse and keyboard sharing utility
 * Copyright (C) 2012-2016 Symless Ltd.
 * Copyright (C) 2002 Chris Schoeneman
 *
 * This package is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * found in the file LICENSE that should have accompanied this file.
 *
 * This package is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "inputleap/ClientApp.h"
#include "arch/Arch.h"
#include "base/Log.h"
#include "base/EventQueue.h"

#if defined(INPUTLEAP_USE_KARABINER_VHID)
#include "platform/KarabinerVirtualHIDKeyboardService.h"
#include <cerrno>
#include <cstring>
#include <cstdlib>
#include <limits>
#endif

#if WINAPI_MSWINDOWS
#include "MSWindowsClientTaskBarReceiver.h"
#endif

namespace inputleap {

#if WINAPI_XWINDOWS || WINAPI_LIBEI || WINAPI_CARBON
CreateTaskBarReceiverFunc createTaskBarReceiver = nullptr;
#endif

int client_main(int argc, char** argv)
{
#if SYSAPI_WIN32
    // record window instance for tray icon, etc
    ArchMiscWindows::setInstanceWin32(GetModuleHandle(nullptr));
#endif

    Arch arch;
    arch.init();

    Log log;
    EventQueue events;

    ClientApp app(&events, createTaskBarReceiver);
    int result = app.run(argc, argv);
#if SYSAPI_WIN32
    if (IsDebuggerPresent()) {
        printf("\n\nHit a key to close...\n");
        getchar();
    }
#endif
    return result;
}

} // namespace inputleap

int main(int argc, char** argv)
{
#if defined(INPUTLEAP_USE_KARABINER_VHID)
    if (argc == 4 && std::strcmp(argv[1], "--karabiner-vhid-helper") == 0) {
        errno = 0;
        char* end = nullptr;
        const unsigned long ownerUid = std::strtoul(argv[3], &end, 10);
        if (errno != 0 || end == argv[3] || *end != '\0' ||
            ownerUid > std::numeric_limits<unsigned int>::max()) {
            return 2;
        }
        return inputleap::runKarabinerVirtualHIDKeyboardService(
            argv[2], static_cast<unsigned int>(ownerUid));
    }
#endif
    return inputleap::client_main(argc, argv);
}
