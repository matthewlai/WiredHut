#ifndef __TIME_CHECKER_H__
#define __TIME_CHECKER_H__

template <typename F>
void CheckCallDuration(F fn, const char* name, uint32_t expected_max_time_ms) {
  uint32_t start = millis();
  fn();
  uint32_t duration = millis() - start;
  if (duration > expected_max_time_ms) {
    log(String(name) + " took " + duration + "ms. Max time budget: " + expected_max_time_ms);
  }
}

#endif // __TIME_CHECKER_H__
