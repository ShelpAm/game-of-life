#include <chrono>
#include <fast_io.h>
#include <fast_io_dsal/span.h>
#include <random>
#include <string>
#include <thread>
#include <vector>

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

auto initialize_state(Settings const &s, fast_io::ibuffer_view &ibf) -> state_t
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

auto main(int argc, char **argv) -> int
{
  using namespace fast_io::io;
  using namespace fast_io::mnp;
  using namespace std::chrono_literals;

  if (argc == 0) {
    return 1;
  }

  if (argc < 2) {
    println("Usage: ", os_c_str(*argv), " <config-file>");
    println("");
    println("Example can be found as state.zzh in project root directory.");
    return 1;
  }

  fast_io::span argv_s{argv, static_cast<std::size_t>(argc)};

  char const *const state_filename{argv_s[1]};
  fast_io::native_file_loader file_data{os_c_str(state_filename)};
  fast_io::ibuffer_view ibf{file_data};

  Settings settings{};
  scan(ibf, settings);

  state_t state{initialize_state(settings, ibf)};

  auto const wait_time{1000ms / settings.fps};
  auto last_start_point{std::chrono::steady_clock::now()};
  while (true) {
    // Render
    for (auto const &e : state) {
      for (auto const f : e) {
        print(fast_io::out(), " ", chvw(f));
      }
      println(fast_io::out(), "");
    }
    println(fast_io::out(), "");

    // Update
    auto next_cell_live{[&settings](bool on, std::size_t surrounding) {
      return on ? settings.on_minimum <= surrounding &&
                      surrounding <= settings.on_maximum
                : settings.off_minimum <= surrounding &&
                      surrounding <= settings.off_maximum;
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

    std::this_thread::sleep_until(last_start_point + wait_time);
    last_start_point = std::chrono::steady_clock::now();
  }
}
