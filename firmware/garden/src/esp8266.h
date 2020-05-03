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

#ifndef __ESP8266_H__
#define __ESP8266_H__

#include <utility>
#include <vector>

#include "formatting.h"
#include "gpio.h"
#include "ostrich.h"
#include "systick.h"
#include "usart.h"

#include "str_util.h"

using namespace Ostrich;

constexpr auto kUartTxPin = PIN_G14;
constexpr auto kUartRxPin = PIN_G9;
constexpr auto kUart = USART6;

template <uint32_t kUart, GPIOPortPin kTxPin, GPIOPortPin kRxPin,
          GPIOPortPin kChEnPin, GPIOPortPin kResetPin, GPIOPortPin kGPIO0Pin>
class ESP8266 {
 public:
  enum class CommandStatus {
    OK, ERROR
  };

  struct APInfo {
    std::string ssid;
    std::string mac;
    int rssi;

    // TODO: Add fields for all information returned by CWLAP.
  };

  ESP8266(int baud_rate) :
      usart_(baud_rate) {
    Reset();
  }

  void Reset(bool bootloader = false) {
    gpio0_ = bootloader ? 0 : 1;
    reset_ = 0;
    chen_ = 0;
    DelayMilliseconds(100);
    chen_ = 1;
    reset_ = 1;

    // Wait for "ready".
    std::string line;
    while (line.find("ready") != 0) {
      line = usart_.GetLine();
    }

    // Disable echo.
    SendCommand("ATE0");

    // Station mode.
    SendCommand("AT+CWMODE_CUR=1");

    // Multiple connection mode.
    SendCommand("AT+CIPMUX=1");

    // Passive mode (ESP8266 holds incoming data in internal buffer).
    SendCommand("AT+CIPRECVMODE=1");
  }

  std::string Version() {
    auto response = SendCommand("AT+GMR");
    if (response.first == CommandStatus::OK) {
      return response.second;
    } else {
      return "";
    }
  }

  std::vector<APInfo> ScanForAPs() {
    auto response = SendCommand("AT+CWLAP");
    std::vector<APInfo> aps;
    if (response.first != CommandStatus::OK) {
      return aps;
    } else {
      std::string line;
      for (char c : response.second) {
        if (c == '\n') {
          if (line.find("+CWLAP") == 0) {
            auto fields = Split(line.substr(7), ',');
            APInfo ap_info;
            ap_info.ssid = RemoveAll(fields[1], '"');
            ap_info.mac = RemoveAll(fields[3], '"');
            ap_info.rssi = Parse<int>(fields[2]);
            aps.push_back(ap_info);
          }
          line = "";
        } else {
          line += c;
        }
      }
    }

    return aps;
  }

  bool ConnectToAP(const std::string& ssid, const std::string& password,
                   std::optional<std::string> mac = std::nullopt) {
    std::string command_str = "AT+CWJAP_CUR=\"" + ssid + "\",\"" + password +
        "\"";

    if (mac) {
      command_str += std::string("\"") + *mac + "\"";
    }

    auto response = SendCommand(command_str);
    return response.first == CommandStatus::OK;
  }

  // Once a connection has been made, subsequent transmissions will either reuse
  // or reconnect as appropriate.
  bool ConnectToTCPServer(int link_id, const std::string& host, int port) {
    std::string command_str = "AT+CIPSTART=" + Format(link_id) + ",\"TCP\",\"" +
        host + "\"," + Format(port);
    auto response = SendCommand(command_str);
    return response.first == CommandStatus::OK;
  }

  bool SendData(int link_id, const std::string& data) {
    usart_ << "AT+CIPSEND=" << link_id << "," << data.size() << "\r\n";
    usart_.Flush();
    Log(std::string("[ESP8266] waiting for '>' or 'CLOSED'"));
    // Wait till we get a '>'
    std::string prompt;
    while (true) {
      char c;
      usart_ >> c;
      if (c == '>') {
        break;
      } else {
        prompt.push_back(c);
        if (prompt.find("CLOSED") != std::string::npos) {
          return false;
        }
      }
    }
    Log(std::string("[ESP8266] starting transmission"));

    auto response = SendCommand(data);
    return response.first == CommandStatus::OK;
  }

  std::string ReceiveData(int link_id) {
    // The AT instruction set manual says we should receive:
    // "+CIPRECVDATA:<actual len>,<data>"
    // We actually get:
    // "+CIPRECVDATA,<actual len>:<data>"
    // ...
    auto response = SendCommand(
      "AT+CIPRECVDATA=" + Format(link_id) + ",256");
    if (response.first == CommandStatus::OK) {
      std::string response_str = response.second;
      std::size_t response_begin = response_str.find("+CIPRECVDATA");
      if (response_begin == std::string::npos) {
        return "";
      } else {
        response_str = response_str.substr(response_begin);
      }
      std::size_t comma = response_str.find(',');
      if (comma == std::string::npos) {
        Log("No comma");
        return "";
      } else {
        std::size_t actual_len_end = response_str.find(':');
        std::size_t actual_len = Parse<int>(
            response_str.substr(comma + 1, (actual_len_end - comma - 1)));
        std::size_t data_start = actual_len_end + 1;
        std::size_t data_end = data_start + actual_len;
        if (data_end <= response_str.size()) {
          auto data_received = response_str.substr(data_start, (data_end - data_start));
          Log(std::string("[ESP8266] Received: ") + data_received);
          return data_received;
        }
      }
    }
    return "";
  }

 private:
  std::pair<CommandStatus, std::string> SendCommand(
      const std::string& command) {
    usart_ << command << "\r\n";
    Log(std::string("[ESP8266] << ") + command);

    // Wait for either OK or ERROR.
    std::string response;
    std::string line;
    while (true) {
      line = ReadLine();
      if (line.find("OK") == 0 || line.find("SEND OK") == 0) {
        return std::make_pair(CommandStatus::OK, response);
      } else if (line.find("ERROR") == 0 || line.find("SEND FAIL") == 0 ||
                 (line.find("CLOSED") == 0) || (line.find("CLOSED") == 2)) {
        Log(std::string("[ESP8266] Command: \"") + command + "\" failed");
        Log(std::string("[ESP8266] Response: ") + response);
        return std::make_pair(CommandStatus::ERROR, response);
      } else {
        response += "\n";
        response += line;
      }
    }
  }

  std::string ReadLine() {
    std::string line = usart_.GetLine();
    Log(std::string("[ESP8266] >> ") + line);
    return line;
  }

  USART<kUart, kUartTxPin, kUartRxPin> usart_;
  OutputPin<PIN_G10> chen_;
  OutputPin<PIN_G11> gpio0_;
  OutputPin<PIN_G13> reset_;
};

// Try sending data if esp if not nullptr, and reset esp if sending failed.
template <typename ESP8266_t>
inline bool TrySend(std::unique_ptr<ESP8266_t>* esp, int link_id,
                    const std::string& data) {
  if (*esp) {
    bool success = (*esp)->SendData(link_id, data);
    if (!success) {
      esp->reset();
    }
    return success;
  } else {
    return false;
  }
}

#endif // __ESP8266_H__
