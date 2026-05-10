module;

#include <cctype>
#include <cstdio>
#include <iostream>
#include <poll.h>
#include <string>
#include <termios.h>
#include <unistd.h>
#include <vector>

export module client:input;

import :pastevents;

namespace client::input {

struct RawMode {
  struct termios orig_termios;
  bool active = false;

  RawMode() {
    if (tcgetattr(STDIN_FILENO, &orig_termios) == -1)
      return;
    struct termios raw = orig_termios;
    // Keep ISIG so Ctrl+C still works
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN);
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw.c_cflag |= (CS8);
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1)
      return;
    active = true;
  }

  ~RawMode() {
    if (active) {
      tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
    }
  }
};

export std::string read_line(const pastevents::History &history,
                             const std::string &prompt) {
  std::string buffer;
  RawMode raw;
  if (!raw.active) {
    // Fallback if raw mode fails
    std::cout << prompt << std::flush;
    std::string line;
    if (!std::getline(std::cin, line))
      return "exit";
    std::cout << "\n";
    return line;
  }

  const auto &events = history.get_all();
  int history_idx = events.size(); // Current position in history
  std::string current_saved; // The line being typed before navigating history

  auto redraw = [&](const std::string &line) {
    // Clear current line: \r moves to start, \033[K clears to end
    std::cout << "\r\033[K" << prompt << line << std::flush;
  };

  redraw(buffer);

  // FEEDBACK: Is there genuinely no clean
  // approach to this in C++? Without using a third-party lib?
  while (true) {
    char c;
    if (read(STDIN_FILENO, &c, 1) <= 0)
      break;

    if (c == '\n' || c == '\r') {
      std::cout << "\n";
      break;
    } else if (c == 127 || c == 8) { // Backspace
      if (!buffer.empty()) {
        buffer.pop_back();
        redraw(buffer);
      }
    } else if (c == 27) { // Escape sequence
      struct pollfd fds;
      fds.fd = STDIN_FILENO;
      fds.events = POLLIN;
      if (poll(&fds, 1, 50) > 0) { // 50ms timeout
        char seq[2];
        if (read(STDIN_FILENO, &seq[0], 1) > 0) {
          if (poll(&fds, 1, 50) > 0) {
            if (read(STDIN_FILENO, &seq[1], 1) > 0) {
              if (seq[0] == '[') {
                if (seq[1] == 'A') { // Up Arrow
                  if (history_idx > 0) {
                    if (history_idx == (int)events.size()) {
                      current_saved = buffer;
                    }
                    history_idx--;
                    buffer = events[history_idx];
                    redraw(buffer);
                  }
                } else if (seq[1] == 'B') { // Down Arrow
                  if (history_idx < (int)events.size()) {
                    history_idx++;
                    if (history_idx == (int)events.size()) {
                      buffer = current_saved;
                    } else {
                      buffer = events[history_idx];
                    }
                    redraw(buffer);
                  }
                }
              }
            }
          }
        }
      }
    } else if (c == 4) { // Ctrl+D
      if (buffer.empty())
        return "exit";
    } else if (c == 12) { // Ctrl+L
      std::cout << "\033[H\033[2J" << std::flush;
      redraw(buffer);
    } else if (!std::iscntrl(c)) {
      buffer += c;
      redraw(buffer);
    }
  }

  return buffer;
}

} // namespace client::input
