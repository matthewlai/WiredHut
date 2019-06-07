/*
 * This file is part of the WiredHut project.
 *
 * Copyright (C) 2019 Matthew Lai <m@matthewlai.ca>
 *
 * This library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __UTILS_H__
#define __UTILS_H__

#include <array>
#include <functional>

#include <libopencm3/stm32/iwdg.h>
#include <systick.h>

class ThrottledExecutor {
 public:
  ThrottledExecutor(int64_t min_period_ms)
      : min_period_ms_(min_period_ms), next_execute_time_(0) {}

  bool ExecuteNow() {
    auto time_now = Ostrich::GetTimeMilliseconds();
    if (time_now > next_execute_time_) {
      next_execute_time_ = time_now + min_period_ms_;
      return true;
    } else {
      return false;
    }
  }

 public:
  uint64_t min_period_ms_;
  uint64_t next_execute_time_;
};

template <std::size_t kSize>
class WindowFilteredValue {
 public:
  WindowFilteredValue() {
    for (auto& v : window_) {
      v = 0.0f;
    }
    current_sum_ = 0.0f;
    next_ = 0;
    current_num_elements_ = 0;
  }

  void AddValue(float new_val) {
    current_sum_ -= window_[next_];
    window_[next_] = new_val;
    current_sum_ += new_val;
    next_ = (next_ + 1) % kSize;

    if (current_num_elements_ < kSize) {
      ++current_num_elements_;
    }
  }

  float AvgValue() const {
    return current_sum_ / current_num_elements_;
  }

 private:
  std::array<float, kSize> window_;
  float current_sum_;
  int next_;
  std::size_t current_num_elements_;
};

void StartWDG() {
  iwdg_set_period_ms(30000);
  iwdg_start();
}

void StrokeWDG() {
  iwdg_reset();
}

#endif // __UTILS_H__
