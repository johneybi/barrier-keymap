#include <atomic>
#include <chrono>
#include <csignal>
#include <filesystem>
#include <iostream>
#include <memory>
#include <mutex>
#include <pqrs/karabiner/driverkit/virtual_hid_device_driver.hpp>
#include <pqrs/karabiner/driverkit/virtual_hid_device_service.hpp>
#include <thread>
#include <unistd.h>

namespace {

// Provisional Barrier input keyboard identity. The important property for this
// probe is that it differs from Karabiner's own output virtual keyboard.
constexpr auto kBarrierVendorId =
    pqrs::hid::vendor_id::value_t(0x1209);
constexpr auto kBarrierProductId =
    pqrs::hid::product_id::value_t(0x4b42);

std::atomic_bool exit_requested(false);

void handle_signal(int) {
  exit_requested = true;
}

} // namespace

int main(int argc, char* argv[]) {
  bool send_test_key = false;

  for (int i = 1; i < argc; ++i) {
    std::string_view arg(argv[i]);
    if (arg == "--send-test-key") {
      send_test_key = true;
    } else {
      std::cerr << "usage: " << argv[0] << " [--send-test-key]" << std::endl;
      return 2;
    }
  }

  std::signal(SIGINT, handle_signal);
  std::signal(SIGTERM, handle_signal);

  const auto socket =
      pqrs::karabiner::driverkit::virtual_hid_device_service::constants::
          get_server_socket_file_path();

  if (access(socket.c_str(), R_OK) != 0) {
    std::cerr << "cannot access Karabiner VirtualHID root-only socket: "
              << socket << std::endl;
    std::cerr << "run this probe with sudo." << std::endl;
    return 1;
  }

  pqrs::dispatcher::extra::initialize_shared_dispatcher();

  std::mutex client_mutex;
  auto client =
      std::make_unique<
          pqrs::karabiner::driverkit::virtual_hid_device_service::client>();

  client->warning_reported.connect([](auto&& message) {
    std::cout << "warning: " << message << std::endl;
  });

  client->connect_failed.connect([](auto&& error_code) {
    std::cout << "connect_failed " << error_code << std::endl;
  });

  client->closed.connect([] {
    std::cout << "closed" << std::endl;
  });

  client->error_occurred.connect([](auto&& error_code) {
    std::cout << "error_occurred " << error_code << std::endl;
  });

  client->driver_activated.connect([](auto&& value) {
    std::cout << "driver_activated " << value << std::endl;
  });

  client->driver_connected.connect([](auto&& value) {
    std::cout << "driver_connected " << value << std::endl;
  });

  client->driver_version_mismatched.connect([](auto&& value) {
    std::cout << "driver_version_mismatched " << value << std::endl;
  });

  client->connected.connect([&client] {
    std::cout << "connected" << std::endl;

    pqrs::karabiner::driverkit::virtual_hid_device_service::
        virtual_hid_keyboard_parameters parameters;
    parameters.set_vendor_id(kBarrierVendorId);
    parameters.set_product_id(kBarrierProductId);
    parameters.set_country_code(pqrs::hid::country_code::us);

    std::cout << "initializing Barrier input virtual keyboard "
              << "vendor_id=0x" << std::hex
              << type_safe::get(parameters.get_vendor_id())
              << " product_id=0x"
              << type_safe::get(parameters.get_product_id())
              << std::dec << std::endl;

    client->async_virtual_hid_keyboard_initialize(parameters);
  });

  client->virtual_hid_keyboard_ready.connect(
      [&client, &client_mutex, send_test_key](auto&& ready) {
        std::cout << "virtual_hid_keyboard_ready " << ready << std::endl;

        if (ready && send_test_key) {
          std::lock_guard<std::mutex> lock(client_mutex);

          if (client) {
            using pqrs::karabiner::driverkit::virtual_hid_device_driver::
                hid_report::keyboard_input;

            keyboard_input down;
            down.keys.insert(type_safe::get(
                pqrs::hid::usage::keyboard_or_keypad::keyboard_b));
            client->async_post_report(down);

            std::this_thread::sleep_for(std::chrono::milliseconds(80));

            keyboard_input up;
            client->async_post_report(up);
          }
        }
      });

  client->async_start();

  std::cout << "Press control-c to quit." << std::endl;
  std::cout << "Then check Karabiner-EventViewer Devices for:" << std::endl;
  std::cout << "  vendor_id: " << type_safe::get(kBarrierVendorId)
            << " product_id: " << type_safe::get(kBarrierProductId)
            << std::endl;

  while (!exit_requested) {
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
  }

  {
    std::lock_guard<std::mutex> lock(client_mutex);
    if (client) {
      client->async_virtual_hid_keyboard_terminate();
      client = nullptr;
    }
  }

  std::this_thread::sleep_for(std::chrono::milliseconds(500));
  pqrs::dispatcher::extra::terminate_shared_dispatcher();

  return 0;
}
