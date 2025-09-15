#pragma once
#include <string>
#include <vector>
#include <optional>

struct RespCmd { std::vector<std::string> args; };

// Parses one RESP command from inbuf (if complete). Consumes parsed bytes.
std::optional<RespCmd> resp_parse(std::string& inbuf);

// Encoders
std::string resp_simple(const std::string& s);  // +OK\r\n
std::string resp_bulk(const std::string& s);    // $len\r\n...\r\n
std::string resp_int(long long v);              // :123\r\n
std::string resp_err(const std::string& s);     // -ERR ...\r\n
