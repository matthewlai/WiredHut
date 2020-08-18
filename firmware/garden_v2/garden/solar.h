#ifndef __SOLAR_H__
#define __SOLAR_H__

#include <Arduino.h>
#include <HardwareSerial.h>

#include <InfluxDbClient.h>

void log(const String& line);

class Solar {
  public:
    static constexpr int kMaxBlockSize = 256;
    
    // The last field in a block will always be "Checksum"
    // So the last bytes should be "Checksum\tX", where X is the checksum.
    static constexpr char* kEndBlockMatch = "Checksum";
    
    Solar(HardwareSerial* port)
      : port_(port), block_index_(0), panel_voltage_(0.0f),
        panel_power_(0.0f), output_current_(0.0f), yield_today_(0.0f),
        yield_yesterday_(0.0f), error_code_(0) {
      // Make sure we can buffer an entire block. This allows us to sleep for
      // up to the block interval (1 second).
      port_->setRxBufferSize(kMaxBlockSize);

    }

    void Handle();

    float PanelVoltage() const { return panel_voltage_; }
    float PanelPower() const { return panel_power_; }
    float OutputCurrent() const { return output_current_; }
    float YieldToday() const { return yield_today_; }
    float YieldYesterday() const { return yield_yesterday_; }
    int ErrorCode() const { return error_code_; }
    bool IsFloating() const { return mode_ == "Float"; }

    Point MakeInfluxDbPoint() const {
      Point pt("garden_solar");
      pt.addField("panel_v", panel_voltage_, 2);
      pt.addField("panel_p", static_cast<int>(panel_power_));
      pt.addField("panel_net_i", output_current_, 4);
      pt.addField("panel_yield_today", yield_today_, 1);
      pt.addField("panel_yield_yesterday", yield_yesterday_, 1);
      pt.addField("panel_bulk", mode_ == "Bulk");
      pt.addField("panel_absorption", mode_ == "Absorption");
      pt.addField("panel_float", mode_ == "Float");
      return pt;    
    }

  private:
    bool VerifyBlockChecksum() {
      unsigned int sum = 0;
      for (int i = 0; i < block_index_; ++i) {
        sum += block_buf_[i];
      }
      return (sum & 0xFF) == 0;
    }

    void ProcessBlock();
    void ProcessLine(const String& line);

    HardwareSerial* port_;
    byte block_buf_[kMaxBlockSize];
    int block_index_;

    float panel_voltage_;
    float panel_power_;
    float output_current_;
    float yield_today_;
    float yield_yesterday_;
    int error_code_;
    String mode_;
};

#endif // __SOLAR_H__
