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

void handle_signal(int) {
  exit_requested = true;
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

bool read_exact(int fd, void* buffer, size_t size) {
  auto* bytes = static_cast<unsigned char*>(buffer);
  size_t offset = 0;
  while (offset < size && !exit_requested) {
    const ssize_t count = recv(fd, bytes + offset, size - offset, 0);
    if (count > 0) {
      offset += static_cast<size_t>(count);
      continue;
    }
    if (count < 0 && errno == EINTR) {
      continue;
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
      listen(fd, 1) != 0) {
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
  auto virtual_hid_client = std::make_unique<client>();

  virtual_hid_client->warning_reported.connect([](auto&& message) {
    std::cerr << "VirtualHID warning: " << message << std::endl;
  });
  virtual_hid_client->connect_failed.connect([](auto&& error) {
    std::cerr << "VirtualHID connect_failed: " << error << std::endl;
  });
  virtual_hid_client->closed.connect([] {
    keyboard_ready = false;
    std::cerr << "VirtualHID connection closed" << std::endl;
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
    std::cout << "VirtualHID keyboard ready=" << (ready ? "true" : "false")
              << std::endl;
  });
  virtual_hid_client->async_start();

  for (int i = 0; i < 100 && !keyboard_ready && !exit_requested; ++i) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
  if (!keyboard_ready) {
    std::cerr << "VirtualHID keyboard did not become ready" << std::endl;
    virtual_hid_client = nullptr;
    pqrs::dispatcher::extra::terminate_shared_dispatcher();
    return 1;
  }

  const std::string socket_path =
      barrier::virtual_hid_bridge::socketPath(allowed_uid);
  const int server_fd =
      create_server_socket(socket_path, allowed_uid, account->pw_gid);
  if (server_fd < 0) {
    virtual_hid_client = nullptr;
    pqrs::dispatcher::extra::terminate_shared_dispatcher();
    return 1;
  }

  std::cout << "Barrier VirtualHID helper listening at " << socket_path
            << " for uid " << allowed_uid << std::endl;

  while (!exit_requested) {
    const int connection = accept(server_fd, nullptr, nullptr);
    if (connection < 0) {
      if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        continue;
      }
      break;
    }

    uid_t peer_uid = 0;
    gid_t peer_gid = 0;
    if (getpeereid(connection, &peer_uid, &peer_gid) != 0 ||
        peer_uid != allowed_uid) {
      std::cerr << "rejected helper client uid " << peer_uid << std::endl;
      close(connection);
      continue;
    }

    std::cout << "Barrier client connected to VirtualHID helper" << std::endl;
    KeyboardReport message;
    while (read_exact(connection, &message, sizeof(message))) {
      if (!barrier::virtual_hid_bridge::isValid(message)) {
        std::cerr << "invalid helper message" << std::endl;
        break;
      }
      post_report(message, virtual_hid_client);
    }
    write_empty_report(virtual_hid_client);
    close(connection);
    std::cout << "Barrier client disconnected from VirtualHID helper"
              << std::endl;
  }

  write_empty_report(virtual_hid_client);
  close(server_fd);
  unlink(socket_path.c_str());
  virtual_hid_client->async_virtual_hid_keyboard_terminate();
  virtual_hid_client = nullptr;
  std::this_thread::sleep_for(std::chrono::milliseconds(300));
  pqrs::dispatcher::extra::terminate_shared_dispatcher();
  return 0;
}
