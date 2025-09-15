#pragma once
#include <string>
#include <vector>

// Returns a RESP-encoded response for the parsed args.
std::string handle_command(const std::vector<std::string>& args);

// Active expiration: pop expired keys from the heap.
// Run this periodically from the event loop.
void ttl_sweep(long long now_ms, int budget);
