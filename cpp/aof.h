#pragma once
#include <vector>
#include <string>

void aof_init(const char* path);
void aof_append(const std::vector<std::string>& argv);
void aof_periodic_flush();
long long aof_bytes();
void aof_replay();
bool aof_is_replaying();
