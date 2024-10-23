#include <chrono>
#include <fast_io.h>
#include <random>
#include <string>
#include <thread>
#include <vector>

struct Settings {
  std::size_t fps;
  double live_probability;
  std::size_t on_minimum;
  std::size_t on_maximum;
  std::size_t off_minimum;
  std::size_t off_maximum;
};

namespace fast_io::io {
void scan(fast_io::ibuffer_view &ibf, Settings &settings)
{
  scan(ibf, settings.fps);
  // scan(ibf, settings.live_probability);
  scan(ibf, settings.on_minimum, settings.on_maximum);
  scan(ibf, settings.off_minimum, settings.off_maximum);
}
} // namespace fast_io::io

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

auto main(int argc, char **argv) -> int
{
  using namespace fast_io::io;
  using namespace fast_io::mnp;

  if (argc < 2) {
    println("Usage: game-of-life <initial-state-file>");
    println("");
    println("Format of the file is: 10x10 char array of '.' and 'o'.");
    return 1;
  }

  std::span argv_s{argv, static_cast<std::size_t>(argc)};

  auto *const state_filename{argv_s[1]};
  fast_io::native_file_loader file_data{os_c_str(state_filename)};

  fast_io::ibuffer_view ibf{file_data};

  std::size_t n;
  std::size_t m;
  scan(ibf, n, m);

  Settings settings{};
  scan(ibf, settings);

  std::vector<std::string> state(n, std::string(m, '.'));

  std::vector<std::string> initial_state;
  std::string buf;
  while (scan<true>(ibf, buf)) {
    initial_state.push_back(buf);
  }
  auto const width{initial_state[0].size()};
  auto const height{initial_state.size()};

  // Initialize the state.
  auto const off_x{(n - height) / 2};
  auto const off_y{(m - width) / 2};
  for (std::size_t i{}; i != height; ++i) {
    for (std::size_t j{}; j != width; ++j) {
      state[off_x + i][off_y + j] = initial_state[i][j];
    }
  }

  using namespace std::chrono_literals;
  auto const wait_time{1000ms / settings.fps};
  auto last_start_point{std::chrono::steady_clock::now()};
  while (true) {
    // Render
    for (auto const &e : state) {
      for (auto const f : e) {
        print(" ", chvw(f));
      }
      println("");
    }
    println("");

    // Update
    auto count_surrounding{[](auto const &state, std::size_t x, std::size_t y) {
      std::size_t counter{};
      for (int i{-1}; i != 2; ++i) {
        for (int j{-1}; j != 2; ++j) {
          if (x + i < state.size() && y + j < state[0].size() &&
              (i != 0 || j != 0)) {
            counter += state[x + i][y + j] == 'o';
          }
        }
      }
      return counter;
    }};

    auto next_cell_live{[&settings](bool on, std::size_t surrounding) {
      if (on) {
        return settings.on_minimum <= surrounding &&
               surrounding <= settings.on_maximum;
      }
      return settings.off_minimum <= surrounding &&
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
