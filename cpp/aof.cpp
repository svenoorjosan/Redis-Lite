#include "aof.h"
#include "resp.h"
#include "commands.h"
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <cerrno>
#include <chrono>
#include <string>
#include <vector>
#include <cstdio>

using Steady = std::chrono::steady_clock;

static int g_fd = -1;
static std::string g_buf;
static Steady::time_point g_last_flush, g_last_fsync;
static long long g_bytes = 0;
static bool g_replaying = false;

static std::string to_resp(const std::vector<std::string>& args) {
  std::string out;
  out += "*" + std::to_string(args.size()) + "\r\n";
  for (const auto& s: args) {
    out += "$" + std::to_string(s.size()) + "\r\n";
    out += s; out += "\r\n";
  }
  return out;
}

void aof_init(const char* path) {
  if (g_fd != -1) return;
  g_fd = ::open(path, O_CREAT|O_APPEND|O_RDWR, 0644);
  if (g_fd < 0) { perror("aof open"); return; }
  struct stat st{};
  if (fstat(g_fd, &st) == 0) g_bytes = st.st_size;
  g_last_flush = g_last_fsync = Steady::now();
}

bool aof_is_replaying() { return g_replaying; }

static void write_all(int fd, const std::string& s) {
  size_t off = 0;
  while (off < s.size()) {
    ssize_t w = ::write(fd, s.data()+off, s.size()-off);
    if (w < 0) { if (errno == EINTR) continue; perror("aof write"); break; }
    off += (size_t)w;
  }
}

void aof_append(const std::vector<std::string>& argv) {
  if (g_fd < 0 || g_replaying) return;
  g_buf += to_resp(argv);
}

void aof_periodic_flush() {
  if (g_fd < 0) return;
  auto now = Steady::now();

  auto ms_since_flush = std::chrono::duration_cast<std::chrono::milliseconds>(now - g_last_flush).count();
  if (!g_buf.empty() && ms_since_flush >= 100) { // flush every 100ms
    write_all(g_fd, g_buf);
    g_bytes += (long long)g_buf.size();
    g_buf.clear();
    g_last_flush = now;
  }

  auto ms_since_fsync = std::chrono::duration_cast<std::chrono::milliseconds>(now - g_last_fsync).count();
  if (ms_since_fsync >= 1000) { // fsync every 1s
    ::fsync(g_fd);
    g_last_fsync = now;
  }
}

long long aof_bytes() { return g_bytes + (long long)g_buf.size(); }

void aof_replay() {
  if (g_fd < 0) return;

  // read whole file
  ::lseek(g_fd, 0, SEEK_SET);
  std::string data; data.reserve((size_t)g_bytes);
  char buf[8192];
  while (true) {
    ssize_t r = ::read(g_fd, buf, sizeof(buf));
    if (r < 0) { if (errno==EINTR) continue; perror("aof read"); break; }
    if (r == 0) break;
    data.append(buf, r);
  }

  g_replaying = true;
  std::string in = std::move(data);
  while (true) {
    auto maybe = resp_parse(in);
    if (!maybe.has_value()) break;
    (void)handle_command(maybe->args); // don't re-append during replay
  }
  g_replaying = false;

  ::lseek(g_fd, 0, SEEK_END); // continue appending
}
