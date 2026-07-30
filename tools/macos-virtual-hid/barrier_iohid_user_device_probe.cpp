#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/hid/IOHIDDeviceKeys.h>
#include <IOKit/hidsystem/IOHIDUserDevice.h>
#include <mach/mach_time.h>

#include <array>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <thread>

namespace {

constexpr std::array<uint8_t, 63> kKeyboardReportDescriptor = {
    0x05, 0x01,       // Usage Page (Generic Desktop)
    0x09, 0x06,       // Usage (Keyboard)
    0xa1, 0x01,       // Collection (Application)
    0x05, 0x07,       //   Usage Page (Keyboard)
    0x19, 0xe0,       //   Usage Minimum (Left Control)
    0x29, 0xe7,       //   Usage Maximum (Right GUI)
    0x15, 0x00,       //   Logical Minimum (0)
    0x25, 0x01,       //   Logical Maximum (1)
    0x75, 0x01,       //   Report Size (1)
    0x95, 0x08,       //   Report Count (8)
    0x81, 0x02,       //   Input (Data, Variable, Absolute)
    0x95, 0x01,       //   Report Count (1)
    0x75, 0x08,       //   Report Size (8)
    0x81, 0x01,       //   Input (Constant)
    0x95, 0x05,       //   Report Count (5)
    0x75, 0x01,       //   Report Size (1)
    0x05, 0x08,       //   Usage Page (LEDs)
    0x19, 0x01,       //   Usage Minimum (Num Lock)
    0x29, 0x05,       //   Usage Maximum (Kana)
    0x91, 0x02,       //   Output (Data, Variable, Absolute)
    0x95, 0x01,       //   Report Count (1)
    0x75, 0x03,       //   Report Size (3)
    0x91, 0x01,       //   Output (Constant)
    0x95, 0x06,       //   Report Count (6)
    0x75, 0x08,       //   Report Size (8)
    0x15, 0x00,       //   Logical Minimum (0)
    0x25, 0x73,       //   Logical Maximum (115)
    0x05, 0x07,       //   Usage Page (Keyboard)
    0x19, 0x00,       //   Usage Minimum (Reserved)
    0x29, 0x73,       //   Usage Maximum (Keyboard F24)
    0x81, 0x00,       //   Input (Data, Array)
    0xc0              // End Collection
};

void setNumber(CFMutableDictionaryRef properties,
               CFStringRef key,
               int32_t value) {
  CFNumberRef number =
      CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &value);
  CFDictionarySetValue(properties, key, number);
  CFRelease(number);
}

} // namespace

int main() {
  CFMutableDictionaryRef properties = CFDictionaryCreateMutable(
      kCFAllocatorDefault,
      0,
      &kCFTypeDictionaryKeyCallBacks,
      &kCFTypeDictionaryValueCallBacks);
  CFDataRef descriptor = CFDataCreate(kCFAllocatorDefault,
                                      kKeyboardReportDescriptor.data(),
                                      kKeyboardReportDescriptor.size());

  CFDictionarySetValue(properties, CFSTR(kIOHIDReportDescriptorKey), descriptor);
  CFDictionarySetValue(properties,
                       CFSTR(kIOHIDManufacturerKey),
                       CFSTR("Barrier Keymap"));
  CFDictionarySetValue(properties,
                       CFSTR(kIOHIDProductKey),
                       CFSTR("Barrier Input Virtual Keyboard"));
  CFDictionarySetValue(properties,
                       CFSTR(kIOHIDSerialNumberKey),
                       CFSTR("org.barrier.keymap.input-keyboard"));
  setNumber(properties, CFSTR(kIOHIDVendorIDKey), 0x1209);
  setNumber(properties, CFSTR(kIOHIDProductIDKey), 0x4b42);

  IOHIDUserDeviceRef device = IOHIDUserDeviceCreateWithProperties(
      kCFAllocatorDefault,
      properties,
      0);
  CFRelease(descriptor);
  CFRelease(properties);

  if (!device) {
    std::cerr
        << "IOHIDUserDeviceCreateWithProperties failed; the process likely "
           "does not have the com.apple.developer.hid.virtual.device "
           "entitlement."
        << std::endl;
    return 1;
  }

  std::cout << "created Barrier Input Virtual Keyboard 0x1209:0x4b42"
            << std::endl;
  std::this_thread::sleep_for(std::chrono::seconds(2));

  std::array<uint8_t, 8> report = {};
  report[2] = 0x6e; // USB HID Keyboard F19.
  IOReturn result = IOHIDUserDeviceHandleReportWithTimeStamp(
      device,
      mach_absolute_time(),
      report.data(),
      report.size());
  if (result == kIOReturnSuccess) {
    std::this_thread::sleep_for(std::chrono::milliseconds(80));
    report.fill(0);
    result = IOHIDUserDeviceHandleReportWithTimeStamp(
        device,
        mach_absolute_time(),
        report.data(),
        report.size());
  }

  if (result != kIOReturnSuccess) {
    std::cerr << "IOHIDUserDeviceHandleReport failed: 0x" << std::hex
              << result << std::dec << std::endl;
    CFRelease(device);
    return 1;
  }

  std::cout << "sent F19 down/up; keeping device alive for inspection"
            << std::endl;
  std::this_thread::sleep_for(std::chrono::seconds(10));
  CFRelease(device);
  return 0;
}
