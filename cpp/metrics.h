#pragma once
#include <cstdint>

void        metrics_init();
void        metrics_inc_commands();
void        metrics_add_expired(std::int64_t n);
void        metrics_set_clients(int n);

std::int64_t metrics_total_commands();
std::int64_t metrics_expired();
int          metrics_clients();
std::int64_t metrics_uptime_sec();
