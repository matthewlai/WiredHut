#include "solar.h"

#include <RateLimiter.h>

static const int kMaxLineLength = 32;

void Solar::Handle() {
  while (port_->available()) {
    if ((block_index_ + 1) >= kBlockBufferSize) {
      log("Solar block buffer overflow. Data dropped.");
      block_index_ = 0;
    } else {
      block_buf_[block_index_++] = port_->read();
    }

    if (block_index_ > 16) {
      bool block_ended = true;
      int match_begin = block_index_ - 10;
      for (int i = 0; i < 8; ++i) {
        if (block_buf_[match_begin + i] != kEndBlockMatch[i]) {
          block_ended = false;
          break;
        }
      }
      if (block_ended) {
        static uint32_t last_successful_solar_data_time = 0;
        uint32_t now = millis();
        if (VerifyBlockChecksum()) {
          last_successful_solar_data_time = now;
          ProcessBlock();
        } else {
          // The charger will occassionally send hex commands (that aren't checksummed),
          // so we only log if we haven't gotten new solar data for 2 minutes.
          static RateLimiter<60 * 60 * 1000, 1> error_rate_limiter;
          uint32_t time_since_last_success = now - last_successful_solar_data_time;
          if (time_since_last_success > (60 * 1000)) {
            error_rate_limiter.CallOrDrop([&]() {
              log(String("No valid solar data for: ") + (time_since_last_success / 1000 / 60) + " minute(s)");
            });
          }
        }
        block_index_ = 0;
      }
    }
  }
}

void Solar::ProcessBlock() {
  int current_line_length = 0;
  char line[kMaxLineLength];
  for (int i = 0; i < block_index_; ++i) {
    // This drops the last line, which is fine, because that's guaranteed to be the
    // checksum, and we have verfied the checksum already.
    if (block_buf_[i] == '\n') {
      if (current_line_length >= 3) {
        line[current_line_length] = '\0';
        ProcessLine(String(line));
        current_line_length = 0;
      }
    } else {
      line[current_line_length++] = block_buf_[i];
    }
  }
  new_data_ = true;
}

void Solar::ProcessLine(const String& line) {
  int tab_index = line.indexOf('\t');
  String field_name;
  String field_value;
  if (tab_index == -1) {
    return;
  }

  field_name = line.substring(0, tab_index);
  field_value = line.substring(tab_index + 1);
  int int_value = field_value.toInt();

  if (field_name == "VPV") {
    panel_voltage_ = int_value / 1000.0f;
  } else if (field_name == "PPV") {
    panel_power_ = int_value;
  } else if (field_name == "I") {
    output_current_ = int_value / 1000.0f;
  } else if (field_name == "H20") {
    yield_today_ = int_value / 100.0f;
  } else if (field_name == "H22") {
    yield_yesterday_ = int_value / 100.0f;
  } else if (field_name == "ERR") {
    error_code_ = int_value;
    if (error_code_ != 0) {
      log(String("Solar error: Code ") + error_code_);
    }
  } else if (field_name == "CS") {
    int code = int_value;
    switch (code) {
      case 0:
        mode_ = "Off";
        break;
      case 2:
        mode_ = "Fault";
        break;
      case 3:
        mode_ = "Bulk";
        break;
      case 4:
        mode_ = "Absorption";
        break;
      case 5:
        mode_ = "Float";
        break;
      default:
        mode_ = String("Unknown: ") + code;
        break;
    }
  }
}
