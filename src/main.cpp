#include <chrono>
#include <csignal>
#include <fast_io.h>
#include <fast_io_dsal/span.h>
#include <fast_io_dsal/string.h>
#include <fast_io_dsal/string_view.h>
#include <random>
#include <string>
#include <thread>
#include <vector>

volatile std::atomic_bool signaled{};

void signal_handler(int /*sig*/)
{
  using fast_io::mnp::chvw;
  // Shows the cursor.
  fast_io::print(fast_io::out(), chvw(0x1B), chvw(0x5B), chvw('?'), 25,
                 chvw('h'));

  signaled = true;
}

struct Settings {
  std::size_t n, m; // Size of the game map.
  std::size_t fps;
  double live_probability;
  std::size_t on_minimum;
  std::size_t on_maximum;
  std::size_t off_minimum;
  std::size_t off_maximum;
};

void scan(fast_io::ibuffer_view &ibf, Settings &s)
{
  scan(ibf, s.n, s.m);
  scan(ibf, s.fps);
  // scan(ibf, settings.live_probability);
  scan(ibf, s.on_minimum, s.on_maximum);
  scan(ibf, s.off_minimum, s.off_maximum);
}

// With probability of p to be true.
auto gen_bool(double p) -> bool
{
  // Ensure the probability is within the range [0, 1]
  if (p < 0.0 || p > 1.0) {
    throw std::invalid_argument("Probability p must be in the range [0, 1]");
  }

  // Create a random number generator
  std::random_device rd;  // Obtain a random number from hardware
  std::mt19937 gen(rd()); // Seed the generator
  std::uniform_real_distribution<> dis(0.0, 1.0); // Define the range

  // Generate a random number in the range [0, 1]
  double random_value = dis(gen);

  // Return true if the random value is less than p
  return random_value < p;
}

using state_t = std::vector<std::string>;

auto read_state(Settings const &s, fast_io::ibuffer_view &ibf) -> state_t
{
  state_t state(s.n, std::string(s.m, '.'));

  state_t state_map;
  std::string buf;
  while (scan<true>(ibf, buf)) {
    state_map.push_back(buf);
  }
  auto const width{state_map[0].size()};
  auto const height{state_map.size()};

  // Initialize the state.
  auto const off_x{(s.n - height) / 2};
  auto const off_y{(s.m - width) / 2};
  for (std::size_t i{}; i != height; ++i) {
    for (std::size_t j{}; j != width; ++j) {
      state[off_x + i][off_y + j] = state_map[i][j];
    }
  }

  return state;
}

auto read_config(std::string_view filename) -> std::pair<Settings, state_t>
{
  fast_io::native_file_loader file_data{filename};
  fast_io::ibuffer_view ibf{file_data};

  Settings settings{};
  scan(ibf, settings);

  return {settings, read_state(settings, ibf)};
}

auto count_surrounding(state_t const &state, std::size_t x, std::size_t y)
    -> std::size_t
{
  std::size_t counter{};
  for (int i{-1}; i != 2; ++i) {
    for (int j{-1}; j != 2; ++j) {
      if (x + i < state.size() && y + j < state[x + i].size() &&
          (i != 0 || j != 0)) {
        counter += state[x + i][y + j] == 'o' ? 1 : 0;
      }
    }
  }
  return counter;
}

void render(state_t const &state)
{
  using fast_io::mnp::chvw;
  fast_io::print(fast_io::out(), chvw(0x1B), chvw(0x5B), 1, chvw(';'), 1,
                 chvw('H'));

  fast_io::string buffer;
  for (auto const &line : state) {
    for (auto const column : line) {
      buffer.append(" ");
      buffer.push_back(column);
    }
    buffer.push_back('\n');
  }
  println(fast_io::out(), buffer);
}

void update(state_t &state, Settings const &s)
{
  auto next_cell_live{[&s](bool on, std::size_t surrounding) {
    return on ? s.on_minimum <= surrounding && surrounding <= s.on_maximum
              : s.off_minimum <= surrounding && surrounding <= s.off_maximum;
  }};

  auto next_state{state};
  for (std::size_t i{}; i != next_state.size(); ++i) {
    for (std::size_t j{}; j != next_state[i].size(); ++j) {
      auto const surrounding{count_surrounding(state, i, j)};
      next_state[i][j] =
          next_cell_live(state[i][j] == 'o', surrounding) ? 'o' : '.';
    }
  }
  state = std::move(next_state);
}

auto main(int argc, char **argv) -> int
{
  using namespace std::chrono_literals;
  using fast_io::mnp::chvw;

  std::signal(SIGINT, signal_handler);

  if (argc == 0) {
    return 1;
  }

  if (argc < 2) {
    fast_io::println("Usage: ", fast_io::mnp::os_c_str(*argv),
                     " <config-file>");
    fast_io::println("");
    fast_io::println("Example config file can be found as state.zzh in project "
                     "root directory.");
    return 1;
  }

  fast_io::span argv_s{argv, static_cast<std::size_t>(argc)};

  // Clears the screen.
  fast_io::print(fast_io::out(), chvw(0x1B), chvw(0x5B), 128, chvw('T'));
  // Hides the cursor.
  fast_io::print(fast_io::out(), chvw(0x1B), chvw(0x5B), chvw('?'), 25,
                 chvw('l'));

  auto [settings, state]{read_config(argv_s[1])};

  auto const wait_time{1000ms / settings.fps};
  auto last_start_point{std::chrono::steady_clock::now()};
  while (!signaled) {
    render(state);
    update(state, settings);

    std::this_thread::sleep_until(last_start_point + wait_time);
    last_start_point = std::chrono::steady_clock::now();
  }
}
