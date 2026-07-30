#include "OSXVirtualHIDBridge.h"

#include <atomic>
#include <cerrno>
#include <chrono>
#include <csignal>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <memory>
#include <pqrs/karabiner/driverkit/virtual_hid_device_driver.hpp>
#include <pqrs/karabiner/driverkit/virtual_hid_device_service.hpp>
#include <pwd.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <thread>
#include <unistd.h>

namespace {

using barrier::virtual_hid_bridge::KeyboardReport;
using pqrs::karabiner::driverkit::virtual_hid_device_driver::hid_report::
    keyboard_input;
using pqrs::karabiner::driverkit::virtual_hid_device_service::client;

constexpr auto kBarrierVendorId =
    pqrs::hid::vendor_id::value_t(0x1209);
constexpr auto kBarrierProductId =
    pqrs::hid::product_id::value_t(0x4b42);

std::atomic_bool exit_requested(false);
std::atomic_bool keyboard_ready(false);
std::atomic_bool keyboard_was_ready(false);
std::atomic_bool service_restart_requested(false);
std::atomic_int active_barrier_connection(-1);
std::atomic_uint64_t connection_sequence(0);

void handle_signal(int) {
  exit_requested = true;
  const int connection = active_barrier_connection.load();
  if (connection >= 0) {
    shutdown(connection, SHUT_RDWR);
  }
}

bool write_empty_report(const std::unique_ptr<client>& virtual_hid_client) {
  if (!keyboard_ready || !virtual_hid_client) {
    return false;
  }
  virtual_hid_client->async_post_report(keyboard_input());
  return true;
}

void post_report(const KeyboardReport& message,
                 const std::unique_ptr<client>& virtual_hid_client) {
  if (!keyboard_ready || !virtual_hid_client) {
    return;
  }

  using modifier = pqrs::karabiner::driverkit::virtual_hid_device_driver::
      hid_report::modifier;
  keyboard_input report;
  const auto mask = message.m_modifiers;

  if (mask & barrier::virtual_hid_bridge::kLeftControl) {
    report.modifiers.insert(modifier::left_control);
  }
  if (mask & barrier::virtual_hid_bridge::kLeftShift) {
    report.modifiers.insert(modifier::left_shift);
  }
  if (mask & barrier::virtual_hid_bridge::kLeftOption) {
    report.modifiers.insert(modifier::left_option);
  }
  if (mask & barrier::virtual_hid_bridge::kLeftCommand) {
    report.modifiers.insert(modifier::left_command);
  }
  if (mask & barrier::virtual_hid_bridge::kRightControl) {
    report.modifiers.insert(modifier::right_control);
  }
  if (mask & barrier::virtual_hid_bridge::kRightShift) {
    report.modifiers.insert(modifier::right_shift);
  }
  if (mask & barrier::virtual_hid_bridge::kRightOption) {
    report.modifiers.insert(modifier::right_option);
  }
  if (mask & barrier::virtual_hid_bridge::kRightCommand) {
    report.modifiers.insert(modifier::right_command);
  }

  for (uint8_t i = 0; i < message.m_keyCount; ++i) {
    report.keys.insert(message.m_keys[i]);
  }
  virtual_hid_client->async_post_report(report);
}

bool read_exact(int fd, void* buffer, size_t size, uint64_t connection_id) {
  auto* bytes = static_cast<unsigned char*>(buffer);
  size_t offset = 0;
  while (offset < size && !exit_requested && !service_restart_requested) {
    const ssize_t count = recv(fd, bytes + offset, size - offset, 0);
    if (count > 0) {
      offset += static_cast<size_t>(count);
      continue;
    }
    if (count < 0 && errno == EINTR) {
      continue;
    }
    if (count == 0) {
      std::cerr << "helper connection " << connection_id
                << " reached EOF after " << offset << "/" << size
                << " report bytes" << std::endl;
    } else {
      std::cerr << "helper connection " << connection_id
                << " recv failed after " << offset << "/" << size
                << " report bytes: " << std::strerror(errno)
                << " (errno=" << errno << ")" << std::endl;
    }
    return false;
  }
  return offset == size;
}

int create_server_socket(const std::string& path, uid_t uid, gid_t gid) {
  struct stat existing;
  if (lstat(path.c_str(), &existing) == 0) {
    if (!S_ISSOCK(existing.st_mode) ||
        (existing.st_uid != 0 && existing.st_uid != uid)) {
      std::cerr << "refusing to replace unsafe helper socket path: "
                << path << std::endl;
      return -1;
    }
    if (unlink(path.c_str()) != 0) {
      std::cerr << "cannot remove stale helper socket: "
                << std::strerror(errno) << std::endl;
      return -1;
    }
  } else if (errno != ENOENT) {
    std::cerr << "cannot inspect helper socket path: "
              << std::strerror(errno) << std::endl;
    return -1;
  }

  const int fd = socket(AF_UNIX, SOCK_STREAM, 0);
  if (fd < 0) {
    return -1;
  }

  sockaddr_un address = {};
  address.sun_family = AF_UNIX;
  if (path.size() >= sizeof(address.sun_path)) {
    close(fd);
    return -1;
  }
  std::strncpy(address.sun_path, path.c_str(), sizeof(address.sun_path) - 1);

  if (bind(fd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0 ||
      chown(path.c_str(), uid, gid) != 0 ||
      chmod(path.c_str(), 0600) != 0 ||
      listen(fd, 4) != 0) {
    std::cerr << "cannot prepare helper socket: "
              << std::strerror(errno) << std::endl;
    close(fd);
    unlink(path.c_str());
    return -1;
  }

  const int flags = fcntl(fd, F_GETFL, 0);
  fcntl(fd, F_SETFL, flags | O_NONBLOCK);
  return fd;
}

} // namespace

int main(int argc, char* argv[]) {
  if (geteuid() != 0 || argc != 3 || std::string(argv[1]) != "--uid") {
    std::cerr << "usage: sudo " << argv[0] << " --uid <console-user-uid>"
              << std::endl;
    return 2;
  }

  char* end = nullptr;
  const unsigned long parsed_uid = std::strtoul(argv[2], &end, 10);
  if (!end || *end != '\0') {
    std::cerr << "invalid uid" << std::endl;
    return 2;
  }
  const uid_t allowed_uid = static_cast<uid_t>(parsed_uid);
  const passwd* account = getpwuid(allowed_uid);
  if (!account) {
    std::cerr << "unknown uid: " << allowed_uid << std::endl;
    return 2;
  }

  std::signal(SIGINT, handle_signal);
  std::signal(SIGTERM, handle_signal);

  pqrs::dispatcher::extra::initialize_shared_dispatcher();
  const std::string socket_path =
      barrier::virtual_hid_bridge::socketPath(allowed_uid);
  std::unique_ptr<client> virtual_hid_client;
  uint64_t service_generation = 0;
  while (!exit_requested) {
    ++service_generation;
    keyboard_ready = false;
    keyboard_was_ready = false;
    service_restart_requested = false;
    virtual_hid_client = std::make_unique<client>();

    virtual_hid_client->warning_reported.connect([](auto&& message) {
      std::cerr << "VirtualHID warning: " << message << std::endl;
    });
    virtual_hid_client->connect_failed.connect([](auto&& error) {
      std::cerr << "VirtualHID connect_failed: " << error << std::endl;
      service_restart_requested = true;
    });
    virtual_hid_client->closed.connect([] {
      keyboard_ready = false;
      service_restart_requested = true;
      const int connection = active_barrier_connection.load();
      if (connection >= 0) {
        shutdown(connection, SHUT_RDWR);
      }
      std::cerr << "VirtualHID connection closed; scheduling reconnect"
                << std::endl;
    });
    virtual_hid_client->connected.connect([&virtual_hid_client] {
      pqrs::karabiner::driverkit::virtual_hid_device_service::
          virtual_hid_keyboard_parameters parameters;
      parameters.set_vendor_id(kBarrierVendorId);
      parameters.set_product_id(kBarrierProductId);
      parameters.set_country_code(pqrs::hid::country_code::us);
      virtual_hid_client->async_virtual_hid_keyboard_initialize(parameters);
    });
    virtual_hid_client->virtual_hid_keyboard_ready.connect([](auto&& ready) {
      keyboard_ready = ready;
      if (ready) {
        keyboard_was_ready = true;
      }
      else if (keyboard_was_ready) {
        service_restart_requested = true;
      }
      std::cout << "VirtualHID keyboard ready=" << (ready ? "true" : "false")
                << std::endl;
    });
    virtual_hid_client->async_start();

    for (int i = 0;
         i < 100 && !keyboard_ready && !service_restart_requested &&
             !exit_requested;
         ++i) {
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    if (!keyboard_ready) {
      std::cerr << "VirtualHID keyboard did not become ready"
                << " generation=" << service_generation
                << "; retrying" << std::endl;
      virtual_hid_client = nullptr;
      std::this_thread::sleep_for(std::chrono::seconds(1));
      continue;
    }

    const int server_fd =
        create_server_socket(socket_path, allowed_uid, account->pw_gid);
    if (server_fd < 0) {
      virtual_hid_client = nullptr;
      std::this_thread::sleep_for(std::chrono::seconds(1));
      continue;
    }

    std::cout << "Barrier VirtualHID helper listening at " << socket_path
              << " for uid " << allowed_uid
              << " generation=" << service_generation << std::endl;

    while (!exit_requested && !service_restart_requested) {
      const int connection = accept(server_fd, nullptr, nullptr);
      if (connection < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
          std::this_thread::sleep_for(std::chrono::milliseconds(100));
          continue;
        }
        std::cerr << "helper accept failed: " << std::strerror(errno)
                  << " (errno=" << errno << ")" << std::endl;
        service_restart_requested = true;
        break;
      }

      const int connection_flags = fcntl(connection, F_GETFL, 0);
      if (connection_flags < 0 ||
          fcntl(connection, F_SETFL, connection_flags & ~O_NONBLOCK) != 0) {
        std::cerr << "cannot make helper client socket blocking: "
                  << std::strerror(errno) << " (errno=" << errno << ")"
                  << std::endl;
        close(connection);
        continue;
      }

      uid_t peer_uid = 0;
      gid_t peer_gid = 0;
      if (getpeereid(connection, &peer_uid, &peer_gid) != 0 ||
          peer_uid != allowed_uid) {
        const int error = errno;
        std::cerr << "rejected helper client uid=" << peer_uid
                  << " gid=" << peer_gid;
        if (error != 0) {
          std::cerr << ": " << std::strerror(error)
                    << " (errno=" << error << ")";
        }
        std::cerr << std::endl;
        close(connection);
        continue;
      }

      active_barrier_connection = connection;
      const uint64_t connection_id = ++connection_sequence;
      uint64_t report_count = 0;
      std::cout << "Barrier client connected to VirtualHID helper"
                << " connection=" << connection_id
                << " uid=" << peer_uid << " gid=" << peer_gid << std::endl;
      KeyboardReport message;
      while (keyboard_ready &&
             read_exact(
                 connection, &message, sizeof(message), connection_id)) {
        if (!barrier::virtual_hid_bridge::isValid(message)) {
          std::cerr << "invalid helper message connection=" << connection_id
                    << " magic=0x" << std::hex << message.m_magic
                    << " version=" << std::dec << message.m_version
                    << " type=" << message.m_type
                    << " key_count="
                    << static_cast<unsigned>(message.m_keyCount)
                    << std::endl;
          break;
        }
        post_report(message, virtual_hid_client);
        ++report_count;
      }
      write_empty_report(virtual_hid_client);
      active_barrier_connection = -1;
      close(connection);
      std::cout << "Barrier client disconnected from VirtualHID helper"
                << " connection=" << connection_id
                << " reports=" << report_count << std::endl;
    }

    write_empty_report(virtual_hid_client);
    close(server_fd);
    unlink(socket_path.c_str());
    virtual_hid_client->async_virtual_hid_keyboard_terminate();
    virtual_hid_client = nullptr;
    if (!exit_requested) {
      std::cout << "restarting VirtualHID service connection"
                << " after generation=" << service_generation << std::endl;
      std::this_thread::sleep_for(std::chrono::seconds(1));
    }
  }

  std::cout << "Barrier VirtualHID helper shutting down"
            << " keyboard_ready=" << (keyboard_ready ? "true" : "false")
            << " exit_requested=" << (exit_requested ? "true" : "false")
            << std::endl;
  unlink(socket_path.c_str());
  std::this_thread::sleep_for(std::chrono::milliseconds(300));
  pqrs::dispatcher::extra::terminate_shared_dispatcher();
  return 0;
}
