// Build: cmake --build build/linux-amd64 --target tekken6_linux_wait_test
// Run: build/linux-amd64/tekken6_linux_wait_test
#include <rex/thread.h>

#include <ctime>
#include <iostream>
#include <stdexcept>

namespace {
void Check(bool condition, const char* message) {
  if (!condition) throw std::runtime_error(message);
}
}

int main() {
  using namespace rex::thread;
  using namespace std::chrono_literals;
  auto a = Event::CreateAutoResetEvent(false);
  auto b = Event::CreateAutoResetEvent(false);
  WaitHandle* handles[] = {a.get(), b.get()};
  const auto start = std::chrono::steady_clock::now();
  const auto cpu_start = std::clock();
  for (int i = 0; i < 200; ++i) {
    Check(WaitMultiple(handles, 2, false, false, 1ms).first == WaitResult::kTimeout,
          "unsignaled waits must time out");
  }
  const double cpu_ms = 1000.0 * (std::clock() - cpu_start) / CLOCKS_PER_SEC;
  const double wall_ms = std::chrono::duration<double, std::milli>(
      std::chrono::steady_clock::now() - start).count();
  Check(wall_ms >= 200, "timed waits must not return early");
  std::thread signal([&] { std::this_thread::sleep_for(2ms); b->Set(); });
  const auto result = WaitMultiple(handles, 2, false, false, 100ms);
  signal.join();
  Check(result.first == WaitResult::kSuccess && result.second == 1,
        "wait-any must report the signaled event");
  Check(WaitMultiple(handles, 2, false, false, 0ms).first == WaitResult::kTimeout,
        "wait-any must consume the auto-reset event");
  a->Set();
  b->Set();
  Check(WaitMultiple(handles, 2, true, false, 100ms).first == WaitResult::kSuccess,
        "wait-all must succeed when both events are signaled");
  Check(WaitMultiple(handles, 2, false, false, 0ms).first == WaitResult::kTimeout,
        "wait-all must consume both auto-reset events");
  // Report CPU use rather than asserting a machine-dependent timing threshold.
  std::cout << "PASS: timeout, wait-any, wait-all, auto-reset; 200 x 1ms waits: "
            << wall_ms << "ms wall, " << cpu_ms << "ms CPU\n";
}
