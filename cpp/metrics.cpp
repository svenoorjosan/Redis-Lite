#include "metrics.h"
#include <chrono>

using Steady = std::chrono::steady_clock;

static Steady::time_point g_start;
static std::int64_t g_total_commands = 0;
static std::int64_t g_expired = 0;
static int          g_clients = 0;

void metrics_init() { g_start = Steady::now(); g_total_commands = 0; g_expired = 0; g_clients = 0; }
void metrics_inc_commands() { ++g_total_commands; }
void metrics_add_expired(std::int64_t n) { g_expired += n; }
void metrics_set_clients(int n) { g_clients = n; }

std::int64_t metrics_total_commands() { return g_total_commands; }
std::int64_t metrics_expired()        { return g_expired; }
int          metrics_clients()         { return g_clients; }
std::int64_t metrics_uptime_sec() {
  return std::chrono::duration_cast<std::chrono::seconds>(Steady::now() - g_start).count();
}
