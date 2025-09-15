#include "resp.h"
#include <stdexcept>

static bool take_line(std::string& buf, std::string& out) {
  size_t p = buf.find("\r\n");
  if (p == std::string::npos) return false;
  out = buf.substr(0, p);
  buf.erase(0, p + 2);
  return true;
}

std::optional<RespCmd> resp_parse(std::string& b) {
  if (b.empty() || b[0] != '*') return std::nullopt;
  std::string line;
  if (!take_line(b, line)) return std::nullopt;        // *N
  int n = std::stoi(line.substr(1));
  std::vector<std::string> out; out.reserve(n);

  for (int i = 0; i < n; ++i) {
    if (b.empty() || b[0] != '$') return std::nullopt;
    if (!take_line(b, line)) return std::nullopt;      // $len
    int len = std::stoi(line.substr(1));
    if (b.size() < static_cast<size_t>(len + 2)) return std::nullopt;
    out.push_back(b.substr(0, len));
    b.erase(0, len + 2);                                // payload + \r\n
  }
  return RespCmd{ out };
}

std::string resp_simple(const std::string& s) { return "+" + s + "\r\n"; }
std::string resp_err(const std::string& s)    { return "-ERR " + s + "\r\n"; }
std::string resp_int(long long v)             { return ":" + std::to_string(v) + "\r\n"; }
std::string resp_bulk(const std::string& s) {
  return "$" + std::to_string(s.size()) + "\r\n" + s + "\r\n";
}
