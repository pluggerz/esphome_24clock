#pragma once

#include "Arduino.h"

template <class VALUE_TYPE, int16_t MAX_SIZE>
class RingBuffer {
  volatile uint16_t writeIndex, readIndex, bufferLength;
  volatile bool overflow_{false};
  VALUE_TYPE buffer[MAX_SIZE];
  volatile uint16_t SIZE = MAX_SIZE;

 public:
  void setup(int _buffer_size = -1) {
    if (_buffer_size < 0)
      SIZE = MAX_SIZE;
    else
      SIZE = _buffer_size <= MAX_SIZE ? _buffer_size : MAX_SIZE;
  }

  void reset() {
    writeIndex = readIndex = bufferLength = 0;
    overflow_ = false;
  }

  RingBuffer() { reset(); }

  uint16_t size() const { return bufferLength; }

  bool is_empty() const { return bufferLength == 0; }

  bool overflow() const { return overflow_; }

  VALUE_TYPE pop() {
    // Check if buffer is empty
    if (bufferLength == 0) {
      // empty
      return 0;
    }

    bufferLength--;  //	Decrease buffer size after reading
    VALUE_TYPE ret = buffer[readIndex];
    readIndex++;  //	Increase readIndex position to prepare for next read
    overflow_ = false;

    // If at last index in buffer, set readIndex back to 0
    if (readIndex == SIZE) {
      readIndex = 0;
    }
    return ret;
  }

  void push(VALUE_TYPE ch) {
    if (bufferLength == SIZE) {
      // Oops, read one line and go to the next
      pop();
      overflow_ = true;
    }

    buffer[writeIndex] = ch;

    bufferLength++;  //	Increase buffer size after writing
    writeIndex++;    //	Increase writeIndex position to prepare for next write

    // If at last index in buffer, set writeIndex back to 0
    if (writeIndex == SIZE) {
      writeIndex = 0;
    }
  }
};
