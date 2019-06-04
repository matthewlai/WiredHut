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

#include <functional>

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

#endif // __UTILS_H__
