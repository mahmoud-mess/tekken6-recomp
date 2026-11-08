// ISO-only Linux builder. No guest code or game assets are linked into this tool.
#include <SDL3/SDL.h>
#include <SDL_render_debug_font.h>

#include <fcntl.h>
#include <signal.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <filesystem>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

extern char** environ;
namespace fs = std::filesystem;

struct View {
  std::string iso;
  std::string output;
  std::string status = "Choose your own Tekken 6 Xbox 360 Europe/USA Rev 1 ISO.";
  std::deque<std::string> log;
  bool advanced = false;
  bool fma = __builtin_cpu_supports("fma");
  bool running = false;
  int jobs = 2;
};

struct State {
  std::mutex mutex;
  View view;
  std::atomic<pid_t> pid{0};
  std::atomic<bool> cancel{false};
  std::atomic<bool> dialog{false};
};

constexpr int width = 960, height = 700;
constexpr SDL_Rect iso_button{30, 166, 200, 38};
constexpr SDL_Rect output_button{30, 274, 245, 38};
constexpr SDL_Rect options_button{30, 330, 320, 36};
constexpr SDL_Rect fma_button{30, 390, 490, 36};
constexpr SDL_Rect minus_button{625, 390, 38, 36};
constexpr SDL_Rect plus_button{855, 390, 38, 36};
constexpr SDL_Rect build_button{30, 450, 200, 42};
constexpr SDL_Rect check_button{245, 450, 210, 42};
constexpr SDL_Rect cancel_button{470, 450, 150, 42};

bool contains(SDL_Rect rect, float x, float y) {
  return x >= rect.x && x < rect.x + rect.w && y >= rect.y && y < rect.y + rect.h;
}

void text(SDL_Surface* surface, int x, int y, const std::string& value,
          int scale = 2, Uint32 color = 0) {
  if (!color) color = SDL_MapSurfaceRGB(surface, 226, 233, 242);
  for (unsigned char c : value) {
    if (c > 126) c = '?';
    if (c >= 33) {
      for (int row = 0; row < 8; ++row) {
        const Uint8 bits = SDL_RenderDebugTextFontData[(c - 33) * 8 + row];
        for (int column = 0; column < 8; ++column) {
          if (bits & (1u << column)) {
            SDL_Rect pixel{x + column * scale, y + row * scale, scale, scale};
            SDL_FillSurfaceRect(surface, &pixel, color);
          }
        }
      }
    }
    x += 8 * scale;
    if (x > width - 20) break;
  }
}

std::string shorten(const std::string& value, size_t count) {
  return value.size() <= count ? value : "..." + value.substr(value.size() - count + 3);
}

void button(SDL_Surface* surface, SDL_Rect rect, const std::string& label,
            bool enabled = true) {
  SDL_FillSurfaceRect(surface, &rect, enabled ? SDL_MapSurfaceRGB(surface, 35, 98, 108)
                                             : SDL_MapSurfaceRGB(surface, 48, 56, 69));
  text(surface, rect.x + 12, rect.y + 12, label);
}

void draw(SDL_Surface* surface, const View& view) {
  SDL_FillSurfaceRect(surface, nullptr, SDL_MapSurfaceRGB(surface, 19, 27, 39));
  text(surface, 30, 30, "TEKKEN 6 / LINUX BUILDER", 3);
  text(surface, 30, 78, "Tools only. Your ISO and all game files stay on your computer.", 1);
  text(surface, 30, 116, "Your disc image");
  text(surface, 30, 145, shorten(view.iso.empty() ? "No ISO selected" : view.iso, 110), 1);
  button(surface, iso_button, "Choose ISO", !view.running);
  text(surface, 30, 224, "Game output folder");
  text(surface, 30, 253, shorten(view.output, 110), 1);
  button(surface, output_button, "Choose folder", !view.running);
  button(surface, options_button, view.advanced ? "[-] Build options" : "[+] Build options",
         !view.running);
  if (view.advanced) {
    button(surface, fma_button, view.fma ? "[x] Enable FMA3" : "[ ] Enable FMA3", !view.running);
    button(surface, minus_button, "-", !view.running);
    text(surface, 680, 402, "Jobs: " + std::to_string(view.jobs));
    button(surface, plus_button, "+", !view.running);
  } else {
    text(surface, 30, 395, "Options: " + std::to_string(view.jobs) +
         " build jobs; FMA3 " + (view.fma ? "on" : "off") + ".", 1);
    text(surface, 30, 412, "Install the README build dependencies; allow 25 GB beyond your ISO.", 1);
  }
  button(surface, build_button, "Build game", !view.running && !view.iso.empty());
  button(surface, check_button, "Check setup", !view.running && !view.iso.empty());
  button(surface, cancel_button, "Cancel", view.running);
  text(surface, 30, 518, shorten(view.status, 112), 1);
  const size_t start = view.log.size() > 8 ? view.log.size() - 8 : 0;
  for (size_t n = start; n < view.log.size(); ++n) {
    text(surface, 30, 548 + int(n - start) * 16, shorten(view.log[n], 112), 1);
  }
}

struct Dialog {
  State* state;
  bool folder;
};

void SDLCALL selected(void* userdata, const char* const* paths, int) {
  auto* context = static_cast<Dialog*>(userdata);
  auto& state = *context->state;
  {
    std::lock_guard lock(state.mutex);
    if (!paths) state.view.status = std::string("File dialog: ") + SDL_GetError();
    else if (paths[0]) {
      (context->folder ? state.view.output : state.view.iso) = paths[0];
      state.view.status = "Ready. Check setup verifies tools and ISO without building.";
    }
  }
  state.dialog = false;
  delete context;
}

void choose(State& state, SDL_Window* window, bool folder) {
  if (state.dialog.exchange(true)) return;
  auto* context = new Dialog{&state, folder};
  if (folder) SDL_ShowOpenFolderDialog(selected, context, window, nullptr, false);
  else {
    static const SDL_DialogFileFilter filters[] = {{"Xbox 360 disc image", "iso"}};
    SDL_ShowOpenFileDialog(selected, context, window, filters, 1, nullptr, false);
  }
}

void cancel(State& state) {
  state.cancel = true;
  const pid_t pid = state.pid.load();
  if (pid > 0) kill(-pid, SIGTERM);
}

void work(State& state, const fs::path& root, View options, bool check) {
  int exit_code = 1;
  std::vector<std::string> args = {"python3", (root / "builder/build_from_iso_linux.py").string(),
      "--builder-root", root.string(), "--iso", options.iso, "--output", options.output,
      "--jobs", std::to_string(options.jobs), "--fma", options.fma ? "on" : "off"};
  if (check) args.push_back("--check");
  std::vector<char*> argv;
  for (auto& arg : args) argv.push_back(arg.data());
  argv.push_back(nullptr);
  int pipes[2];
  if (pipe2(pipes, O_CLOEXEC) == 0) {
    posix_spawn_file_actions_t actions;
    posix_spawn_file_actions_init(&actions);
    posix_spawn_file_actions_addopen(&actions, STDIN_FILENO, "/dev/null", O_RDONLY, 0);
    posix_spawn_file_actions_adddup2(&actions, pipes[1], STDOUT_FILENO);
    posix_spawn_file_actions_adddup2(&actions, pipes[1], STDERR_FILENO);
    posix_spawn_file_actions_addclose(&actions, pipes[0]);
    posix_spawn_file_actions_addclose(&actions, pipes[1]);
    posix_spawnattr_t attributes;
    posix_spawnattr_init(&attributes);
    posix_spawnattr_setflags(&attributes, POSIX_SPAWN_SETPGROUP);
    posix_spawnattr_setpgroup(&attributes, 0);
    pid_t pid = 0;
    const int result = posix_spawnp(&pid, "python3", &actions, &attributes, argv.data(), environ);
    posix_spawn_file_actions_destroy(&actions);
    posix_spawnattr_destroy(&attributes);
    close(pipes[1]);
    if (result == 0) {
      state.pid = pid;
      if (state.cancel) kill(-pid, SIGTERM);
      FILE* output = fdopen(pipes[0], "r");
      if (output) {
        char buffer[4096];
        while (fgets(buffer, sizeof(buffer), output)) {
          std::string line(buffer);
          while (!line.empty() && (line.back() == '\n' || line.back() == '\r')) line.pop_back();
          std::lock_guard lock(state.mutex);
          if (line.starts_with("STEP ")) state.view.status = line.substr(5);
          state.view.log.push_back(line);
          if (state.view.log.size() > 100) state.view.log.pop_front();
        }
        fclose(output);
      } else close(pipes[0]);
      int status = 0;
      if (waitpid(pid, &status, 0) == pid) {
        exit_code = WIFEXITED(status) ? WEXITSTATUS(status) : 128 + WTERMSIG(status);
      }
      state.pid = 0;
    } else {
      close(pipes[0]);
      std::lock_guard lock(state.mutex);
      state.view.log.push_back("Cannot start python3. Install Python and the README dependencies.");
    }
  }
  std::lock_guard lock(state.mutex);
  state.view.running = false;
  if (state.cancel) state.view.status = "Cancelled. Build files are kept for a later retry.";
  else if (exit_code) state.view.status = "Failed. See the messages below and the build log.";
  else state.view.status = check ? "Checks passed. No game was built." : "Complete. Open Play.sh in your game output folder.";
}

int main(int argc, char** argv) {
  char executable[4096];
  const ssize_t length = readlink("/proc/self/exe", executable, sizeof(executable) - 1);
  if (length < 0) return 1;
  executable[length] = 0;
  fs::path root = fs::path(executable).parent_path().parent_path();
  std::string preview;
  bool smoke = false;
  for (int n = 1; n < argc; ++n) {
    const std::string arg = argv[n];
    if (arg == "--help") {
      puts("Tekken 6 Linux Builder: select your own ISO and an output folder.\n"
           "Optional FMA3 and job settings. Check setup does not compile the game.");
      return 0;
    }
    if (arg == "--builder-root" && n + 1 < argc) root = argv[++n];
    else if (arg == "--preview" && n + 1 < argc) preview = argv[++n];
    else if (arg == "--smoke") smoke = true;
    else { fprintf(stderr, "Unknown builder argument: %s\n", arg.c_str()); return 2; }
  }
  State state;
  const char* home = getenv("HOME");
  state.view.output = (fs::path(home ? home : ".") / "Games/Tekken6-Linux").string();
  SDL_Surface* canvas = SDL_CreateSurface(width, height, SDL_PIXELFORMAT_XRGB8888);
  if (!canvas) return 1;
  if (!preview.empty()) {
    draw(canvas, state.view);
    const bool saved = SDL_SaveBMP(canvas, preview.c_str());
    SDL_DestroySurface(canvas);
    return saved ? 0 : 1;
  }
  SDL_SetHint(SDL_HINT_FRAMEBUFFER_ACCELERATION, "0");
  if (!SDL_Init(SDL_INIT_VIDEO)) {
    fprintf(stderr, "SDL: %s\n", SDL_GetError());
    SDL_DestroySurface(canvas);
    return 1;
  }
  SDL_Window* window = SDL_CreateWindow("Tekken 6 Linux Builder", width, height, 0);
  if (!window) {
    fprintf(stderr, "SDL: %s\n", SDL_GetError());
    SDL_DestroySurface(canvas);
    SDL_Quit();
    return 1;
  }
  std::thread worker;
  bool quit = false;
  int exit_status = 0;
  while (!quit) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
      if (event.type == SDL_EVENT_QUIT && !state.dialog) quit = true;
      if (event.type != SDL_EVENT_MOUSE_BUTTON_DOWN || event.button.button != SDL_BUTTON_LEFT) continue;
      View view;
      { std::lock_guard lock(state.mutex); view = state.view; }
      if (view.running) {
        if (contains(cancel_button, event.button.x, event.button.y)) {
          cancel(state);
        }
        continue;
      }
      if (state.dialog) continue;
      if (contains(iso_button, event.button.x, event.button.y)) choose(state, window, false);
      else if (contains(output_button, event.button.x, event.button.y)) choose(state, window, true);
      else if (contains(options_button, event.button.x, event.button.y)) {
        std::lock_guard lock(state.mutex); state.view.advanced = !view.advanced;
      } else if (view.advanced && contains(fma_button, event.button.x, event.button.y)) {
        std::lock_guard lock(state.mutex); state.view.fma = !view.fma;
      } else if (view.advanced && contains(minus_button, event.button.x, event.button.y)) {
        std::lock_guard lock(state.mutex); state.view.jobs = std::max(1, view.jobs - 1);
      } else if (view.advanced && contains(plus_button, event.button.x, event.button.y)) {
        std::lock_guard lock(state.mutex); state.view.jobs = std::min(128, view.jobs + 1);
      } else if (!view.iso.empty() && (contains(build_button, event.button.x, event.button.y) ||
                                      contains(check_button, event.button.x, event.button.y))) {
        if (worker.joinable()) worker.join();
        state.cancel = false;
        { std::lock_guard lock(state.mutex); state.view.running = true; state.view.log.clear(); }
        worker = std::thread(work, std::ref(state), root, view,
                            contains(check_button, event.button.x, event.button.y));
      }
    }
    View view;
    { std::lock_guard lock(state.mutex); view = state.view; }
    draw(canvas, view);
    SDL_Surface* surface = SDL_GetWindowSurface(window);
    if (!surface) {
      fprintf(stderr, "SDL surface: %s\n", SDL_GetError());
      exit_status = 1;
      break;
    }
    SDL_BlitSurfaceScaled(canvas, nullptr, surface, nullptr, SDL_SCALEMODE_NEAREST);
    SDL_UpdateWindowSurface(window);
    if (smoke) quit = true;
    SDL_Delay(33);
  }
  cancel(state);
  if (worker.joinable()) worker.join();
  SDL_DestroySurface(canvas);
  SDL_DestroyWindow(window);
  SDL_Quit();
  return exit_status;
}
