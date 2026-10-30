#include <cassert>
#include <cstring>
#include <string>
#include "../src/platform/SerialLineBuffer.h"

template <size_t N>
static std::string feed(SerialLineBuffer<N>& buffer, const std::string& text,
                        int& lines, int& overflows) {
  std::string last;
  for (char c : text) {
    switch (buffer.feed(c)) {
      case SerialLineBuffer<N>::Line: lines++; last = buffer.line(); break;
      case SerialLineBuffer<N>::Overflow: overflows++; last = buffer.line(); break;
      case SerialLineBuffer<N>::Pending: break;
    }
  }
  return last;
}

int main() {
  int lines = 0, overflows = 0;
  SerialLineBuffer<1024> usb;
  assert(feed(usb, "USBGET,CONTROL\r\n", lines, overflows) == "USBGET,CONTROL");
  assert(lines == 1 && overflows == 0);
  assert(feed(usb, "\n\r\n", lines, overflows).empty() && lines == 1);

  // Lines beyond 255 bytes stay intact.
  const std::string command = "USBPOST,CONTROL,{\"pad\":\"" + std::string(900, 'x') + "\"}";
  assert(feed(usb, command + "\n", lines, overflows) == command && lines == 2);

  // An oversize line is reported once and its tail never runs as a command.
  const std::string oversize = "USBPOST,CONTROL," + std::string(1100, 'y') + "WIFI,RESET";
  assert(feed(usb, oversize + "\r\n", lines, overflows).rfind("USBPOST,CONTROL,", 0) == 0);
  assert(lines == 2 && overflows == 1);
  assert(feed(usb, "WIFI,STATUS\n", lines, overflows) == "WIFI,STATUS" && lines == 3);

  // Exactly Capacity - 1 bytes fit; one more overflows.
  SerialLineBuffer<8> small;
  assert(feed(small, "1234567\n", lines, overflows) == "1234567" && lines == 4);
  feed(small, "12345678\n", lines, overflows);
  assert(lines == 4 && overflows == 2 && std::strlen(small.line()) == 7);
  return 0;
}
