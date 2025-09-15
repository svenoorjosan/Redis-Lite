// cpp/commands.cpp  — Day 2 version
#include "commands.h"
#include "resp.h"
#include "metrics.h"
#include "aof.h"

#include <unordered_map>
#include <queue>
#include <algorithm>
#include <cctype>
#include <chrono>
#include <string>
#include <vector>

using namespace std;

static long long now_ms()
{
  using clock_t = std::chrono::steady_clock;
  return std::chrono::duration_cast<std::chrono::milliseconds>(clock_t::now().time_since_epoch()).count();
}

struct Value
{
  std::string data;
  long long expire_at_ms = 0; // 0 => no expiry
};

static unordered_map<string, Value> g_kv;

// min-heap of (expire_at_ms, key)
static priority_queue<pair<long long, string>, vector<pair<long long, string>>, greater<pair<long long, string>>> g_exp_heap;

static string upper(string s)
{
  transform(s.begin(), s.end(), s.begin(), [](unsigned char c)
            { return std::toupper(c); });
  return s;
}

static bool is_expired(const Value &v, long long now)
{
  return v.expire_at_ms != 0 && v.expire_at_ms <= now;
}

static bool lazily_remove_if_expired(const string &key, long long now)
{
  auto it = g_kv.find(key);
  if (it == g_kv.end())
    return false;
  if (is_expired(it->second, now))
  {
    g_kv.erase(it);
    metrics_add_expired(1);
    return true;
  }
  return false;
}

void ttl_sweep(long long now, int budget)
{
  while (budget > 0 && !g_exp_heap.empty())
  {
    auto [ts, key] = g_exp_heap.top();
    if (ts > now)
      break;
    g_exp_heap.pop();

    auto it = g_kv.find(key);
    if (it != g_kv.end() && is_expired(it->second, now) && it->second.expire_at_ms == ts)
    {
      g_kv.erase(it);
      metrics_add_expired(1);
      --budget;
    }
  }
}

static string cmd_ping(const vector<string> &a)
{
  return a.size() == 2 ? resp_bulk(a[1]) : resp_simple("PONG");
}

static string cmd_echo(const vector<string> &a)
{
  if (a.size() != 2)
    return resp_err("wrong number of arguments for 'ECHO'");
  return resp_bulk(a[1]);
}

static string cmd_set(const vector<string> &a)
{
  if (a.size() != 3)
    return resp_err("usage: SET key value");
  g_kv[a[1]] = Value{a[2], 0};
  if (!aof_is_replaying())
    aof_append(a);
  return resp_simple("OK");
}

static string cmd_get(const vector<string> &a)
{
  if (a.size() != 2)
    return resp_err("usage: GET key");
  long long now = now_ms();
  lazily_remove_if_expired(a[1], now);
  auto it = g_kv.find(a[1]);
  if (it == g_kv.end())
    return "$-1\r\n";
  return resp_bulk(it->second.data);
}

static string cmd_del(const vector<string> &a)
{
  if (a.size() < 2)
    return resp_err("usage: DEL key [key ...]");
  long long now = now_ms();
  long long cnt = 0;
  for (size_t i = 1; i < a.size(); ++i)
  {
    lazily_remove_if_expired(a[i], now);
    cnt += g_kv.erase(a[i]) ? 1 : 0;
  }
  if (cnt > 0 && !aof_is_replaying())
    aof_append(a);
  return resp_int(cnt);
}

static string cmd_exists(const vector<string> &a)
{
  if (a.size() < 2)
    return resp_err("usage: EXISTS key [key ...]");
  long long now = now_ms();
  long long cnt = 0;
  for (size_t i = 1; i < a.size(); ++i)
  {
    if (lazily_remove_if_expired(a[i], now))
      continue;
    cnt += g_kv.count(a[i]) ? 1 : 0;
  }
  return resp_int(cnt);
}

static string cmd_expire(const vector<string> &a)
{
  if (a.size() != 3)
    return resp_err("usage: EXPIRE key seconds");
  long long now = now_ms();
  auto it = g_kv.find(a[1]);
  if (it == g_kv.end() || is_expired(it->second, now))
  {
    g_kv.erase(a[1]);
    return resp_int(0);
  }
  long long sec = std::stoll(a[2]);
  it->second.expire_at_ms = now + sec * 1000;
  g_exp_heap.push({it->second.expire_at_ms, a[1]});
  if (!aof_is_replaying())
    aof_append(a);
  return resp_int(1);
}

static string cmd_ttl(const vector<string> &a)
{
  if (a.size() != 2)
    return resp_err("usage: TTL key");
  long long now = now_ms();
  auto it = g_kv.find(a[1]);
  if (it == g_kv.end() || is_expired(it->second, now))
  {
    if (it != g_kv.end())
    {
      g_kv.erase(it);
      metrics_add_expired(1);
    }
    return resp_int(-2); // key not found
  }
  if (it->second.expire_at_ms == 0)
    return resp_int(-1); // no expiry
  long long rem_ms = it->second.expire_at_ms - now;
  if (rem_ms < 0)
    rem_ms = 0;
  return resp_int(rem_ms / 1000);
}

static string cmd_info()
{
  std::string s;
  s += "uptime:" + std::to_string(metrics_uptime_sec()) + "\n";
  s += "connected_clients:" + std::to_string(metrics_clients()) + "\n";
  s += "keys:" + std::to_string(g_kv.size()) + "\n";
  s += "total_commands:" + std::to_string(metrics_total_commands()) + "\n";
  s += "expired_keys:" + std::to_string(metrics_expired()) + "\n";
  s += "aof_bytes:" + std::to_string(aof_bytes()) + "\n";
  return resp_bulk(s);
}

std::string handle_command(const std::vector<std::string> &a)
{
  metrics_inc_commands();
  if (a.empty())
    return resp_err("empty command");
  auto cmd = upper(a[0]);

  if (cmd == "PING")
    return cmd_ping(a);
  if (cmd == "ECHO")
    return cmd_echo(a);
  if (cmd == "SET")
    return cmd_set(a);
  if (cmd == "GET")
    return cmd_get(a);
  if (cmd == "DEL")
    return cmd_del(a);
  if (cmd == "EXISTS")
    return cmd_exists(a);
  if (cmd == "EXPIRE")
    return cmd_expire(a);
  if (cmd == "TTL")
    return cmd_ttl(a);
  if (cmd == "INFO")
    return cmd_info();

  return resp_err("unknown command '" + a[0] + "'");
}
