// cpp/server.cpp
#include <sys/epoll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>
#include <unordered_map>
#include <string>
#include <vector>
#include <iostream>
#include <chrono>
#include <signal.h>
#include <atomic>
#include <cstdlib> // getenv, atoi

#include "resp.h"
#include "commands.h"
#include "metrics.h"
#include "aof.h"

static std::atomic<bool> g_stop{false};
static void on_sig(int) { g_stop = true; }

static int set_nonblock(int fd)
{
  int f = fcntl(fd, F_GETFL, 0);
  return fcntl(fd, F_SETFL, f | O_NONBLOCK);
}

struct Conn
{
  int fd;
  std::string inbuf;
  std::string outbuf;
};

static void mod_epoll(int ep, int fd, uint32_t events, void *ptr)
{
  epoll_event ev{.events = events, .data = {.ptr = ptr}};
  if (epoll_ctl(ep, EPOLL_CTL_MOD, fd, &ev) < 0)
  {
    epoll_ctl(ep, EPOLL_CTL_ADD, fd, &ev); // first time
  }
}

static long long now_ms()
{
  using Steady = std::chrono::steady_clock;
  return std::chrono::duration_cast<std::chrono::milliseconds>(Steady::now().time_since_epoch()).count();
}

int main(int argc, char **argv)
{
  int port = 6380;
  if (const char *p = std::getenv("PORT"))
    port = std::atoi(p);
  if (argc == 2)
    port = std::stoi(argv[1]);

  signal(SIGINT, on_sig);
  signal(SIGTERM, on_sig);

  metrics_init();
  const char *aof_path = std::getenv("AOF_PATH");
  aof_init(aof_path ? aof_path : "data.aof");
  aof_replay();

  int lfd = socket(AF_INET, SOCK_STREAM, 0);
  int yes = 1;
  setsockopt(lfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = INADDR_ANY;
  addr.sin_port = htons(port);
  if (bind(lfd, (sockaddr *)&addr, sizeof(addr)) < 0)
  {
    perror("bind");
    return 1;
  }
  if (listen(lfd, 128) < 0)
  {
    perror("listen");
    return 1;
  }
  set_nonblock(lfd);

  int ep = epoll_create1(0);
  epoll_event ev{.events = EPOLLIN, .data = {.fd = lfd}};
  epoll_ctl(ep, EPOLL_CTL_ADD, lfd, &ev);

  std::unordered_map<int, Conn> conns;

  std::cout << "Server listening on :" << port << std::endl;

  constexpr int MAXE = 128;
  epoll_event events[MAXE];

  while (true)
  {
    // check stop flag before waiting too long
    if (g_stop.load())
    {
      aof_periodic_flush();
      break;
    }

    int n = epoll_wait(ep, events, MAXE, /*timeout ms*/ 50);

    // epoll_wait can be interrupted by signals (EINTR)
    if (n < 0 && errno == EINTR)
      continue;

    for (int i = 0; i < n; ++i)
    {
      if (events[i].data.fd == lfd)
      {
        // accept all pending
        while (true)
        {
          int cfd = accept(lfd, nullptr, nullptr);
          if (cfd < 0)
          {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
              break;
            perror("accept");
            break;
          }
          set_nonblock(cfd);
          Conn c{cfd, "", ""};
          conns.emplace(cfd, std::move(c));
          epoll_event cev{.events = EPOLLIN | EPOLLET, .data = {.ptr = &conns[cfd]}};
          epoll_ctl(ep, EPOLL_CTL_ADD, cfd, &cev);
          metrics_set_clients((int)conns.size());
        }
        continue;
      }

      Conn *c = reinterpret_cast<Conn *>(events[i].data.ptr);
      int fd = c->fd;

      if (events[i].events & EPOLLIN)
      {
        char buf[4096];
        while (true)
        {
          ssize_t r = recv(fd, buf, sizeof(buf), 0);
          if (r > 0)
          {
            c->inbuf.append(buf, r);
            while (true)
            {
              auto maybe = resp_parse(c->inbuf);
              if (!maybe.has_value())
                break;
              std::string resp = handle_command(maybe->args);
              c->outbuf.append(resp);
            }
          }
          else if (r == 0)
          {
            close(fd);
            conns.erase(fd);
            metrics_set_clients((int)conns.size());
            break;
          }
          else
          {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
              break;
            close(fd);
            conns.erase(fd);
            metrics_set_clients((int)conns.size());
            break;
          }
        }
      }

      if (events[i].events & (EPOLLOUT | EPOLLIN))
      {
        while (!c->outbuf.empty())
        {
          ssize_t w = send(fd, c->outbuf.data(), c->outbuf.size(), 0);
          if (w > 0)
          {
            c->outbuf.erase(0, w);
          }
          else
          {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
              break;
            close(fd);
            conns.erase(fd);
            metrics_set_clients((int)conns.size());
            break;
          }
        }
        uint32_t want = EPOLLIN | EPOLLET;
        if (!c->outbuf.empty())
          want |= EPOLLOUT;
        mod_epoll(ep, fd, want, c);
      }
    }

    // periodic tasks
    ttl_sweep(now_ms(), /*budget*/ 200);
    aof_periodic_flush();
  }

  // Clean shutdown: close sockets once, print once
  for (auto &[fd, _] : conns)
    close(fd);
  close(lfd);
  std::cout << "Shutting down." << std::endl;
  return 0;
}
