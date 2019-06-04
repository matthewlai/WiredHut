#ifndef __CONFIG_H__
#define __CONFIG_H__

namespace Config {

// Wifi config
const char* kSSID = "SSID";
const char* kPass = "Password";
constexpr int kGardenControllerPort = 2938;
constexpr int kEnvironmentControllerPort = 2939;
constexpr int kGardenControllerLinkId = 0;
constexpr int kEnvironmentControllerLinkId = 1;
const char* kHubHost = "wiredhut_hub";

constexpr int kEsp8266BaudRate = 115200;

// Board constants (the final factor is calibrated gain error due to bad board
// layout). There is also an offset but we correct for that at run time.
constexpr float kFullScalePumpCurrent = 3.3f / 20 / 0.01f / 1.28f;
constexpr float kFullScaleSw1Current = 3.3f / 20 / 0.01f / 1.32f;
constexpr float kPressureSensorFullScaleCurrent = 3.3f / 20 / 6.8f;

// Debug
// Wait for USB serial port to be opened before starting (for debugging)
constexpr bool kWaitForPortOpen = false; 

} // namespace Config

#endif // __CONFIG_H__
