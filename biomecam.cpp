#include <cerrno>
#include <thread>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <sys/wait.h>
#include <unistd.h>
#include <iostream>
#include <fstream>
#include <algorithm>
#include <vector>
#include <gpiod.hpp>

#define LENGTH 10   // length of video in seconds

namespace fs = std::filesystem;

static void build_file_list(const fs::path& dir);

static void death(const char *err) {
  std::fprintf(stderr, "%s: %s\n", err, std::strerror(errno));
  std::exit(1);
}

int main() {
  using offset = gpiod::line::offset;
  using direction = gpiod::line::direction;
  using edge = gpiod::line::edge;

  constexpr int WIDTH = 1640;
  constexpr int HEIGHT = 1232;
  constexpr int FRAMERATE = 24;
  const std::string CODEC = "h264";

  gpiod::chip chip("/dev/gpiochip0");
  if (!chip) {
    std::perror("gpiod_chip_open");
    return 1;
  }

  gpiod::line_settings settings;
  settings.set_direction(direction::INPUT);
  settings.set_edge_detection(edge::RISING);

  gpiod::line_config lcfg;
  lcfg.add_line_settings(std::vector<offset>{23}, settings);
  gpiod::request_config rcfg;
  rcfg.set_consumer("trigger");

  auto rb = chip.prepare_request();
  rb.set_request_config(rcfg);
  rb.set_line_config(lcfg);

  auto req = rb.do_request();

  std::cout << "Waiting for rising edge on GPIO 23" << std::endl;

  // req.wait_edge_events((std::chrono::nanoseconds)-1);

  std::cout << "Event detected!" << std::endl;
  pid_t rpicam_pid = fork();

  if (rpicam_pid < 0) death("fork rpicam-vid");

  // child
  if (rpicam_pid == 0) {
    std::vector<std::string> args = {
      "rpicam-vid",
      "--width", std::to_string(WIDTH),
      "--height", std::to_string(HEIGHT),
      "--framerate", std::to_string(FRAMERATE),

      "--codec", CODEC,
      "--bitrate", "6000000",
      "--profile", "high",
      "--level", "4.1",
      "--intra", std::to_string(FRAMERATE * 2),

      "--segment", "1000", // TODO: change to something like 120000
      "--timeout", "0",

      "--flush",
      "--inline",
      "--listen",
      "--tuning-file", "/usr/share/libcamera/ipa/rpi/vc4/imx219_noir.json",
      "-n",
      "-o", "flight_%04d.mkv"
    };

    std::vector<char*> argv;
    argv.reserve(args.size() + 1);
    for (auto &s : args)
      argv.push_back(s.data());
    argv.push_back(nullptr);

    execvp(argv[0], argv.data());
    death("execvp rpicam-vid");  // if we get here, execvp failed
  }

  lcfg.reset();
  lcfg.add_line_settings(std::vector<offset>{24}, settings);

  auto rb_done = chip.prepare_request();
  rb_done.set_request_config(rcfg);
  rb_done.set_line_config(lcfg);

  auto req_done = rb_done.do_request();

  std::cout << "Waiting for rising edge on GPIO 24" << std::endl;

  // TODO: Uncomment
  // req_done.wait_edge_events((std::chrono::nanoseconds)-1);

  // TODO: Remove this
  // wait for 17 seconds for the purpose of testing
  std::this_thread::sleep_for(std::chrono::seconds(17));
  std::cout << "Attempting to kill rpicam-vid" << std::endl;
  if (kill(rpicam_pid, SIGINT) == -1)
    std::cout << "Error killing rpicam-vid!" << std::endl;

  int rpicam_status = 0;
  waitpid(rpicam_pid, &rpicam_status, 0);

  if (!WIFEXITED(rpicam_status) || WEXITSTATUS(rpicam_status) != 0)
    std::fprintf(stderr, "rpicam-vid failed, status is %d\n", rpicam_status);

  // parent
  std::cout << "Flight finished, building file list..." << std::endl;
  build_file_list(".");
  return 0;
}

static void build_file_list(const fs::path& dir) {
  std::vector<fs::path> files;

  for (auto& p : fs::directory_iterator(dir)) {
    if (p.path().extension() == ".mkv" &&
       (p.path().filename().string().rfind("flight_", 0) == 0)) {
        files.push_back(p.path());
      }
  }

  std::sort(files.begin(), files.end(), [](const fs::path& a, const fs::path& b)
      {
      return std::stoi(a.stem().string().substr(7)) <
             std::stoi(b.stem().string().substr(7));
      });

  std::ofstream file_list("list.txt");
  for (auto& f : files) {
    file_list << "file '" << f.string() << "'\n";
  }
}

