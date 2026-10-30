#pragma once
#include <stddef.h>

// Assembles newline-terminated serial commands. An oversize line is discarded
// whole: neither its truncated head nor its tail is ever run as a command.
template <size_t Capacity>
class SerialLineBuffer {
 public:
  enum Result { Pending, Line, Overflow };

  Result feed(char incoming) {
    if (incoming == '\n' || incoming == '\r') {
      const bool overflowed = overflowed_;
      const size_t length = length_;
      overflowed_ = false;
      length_ = 0;
      if (overflowed) return Overflow;
      if (length == 0) return Pending;
      line_[length] = '\0';
      return Line;
    }
    if (overflowed_) return Pending;
    if (length_ < Capacity - 1) {
      line_[length_++] = incoming;
    } else {
      line_[length_] = '\0';
      overflowed_ = true;
    }
    return Pending;
  }

  // The completed line, or the retained head of a discarded oversize line.
  const char* line() const { return line_; }

 private:
  char line_[Capacity] = {};
  size_t length_ = 0;
  bool overflowed_ = false;
};
