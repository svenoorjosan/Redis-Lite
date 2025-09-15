# Build C++ binary
FROM gcc:13 as build
WORKDIR /app
COPY cpp/ /app/cpp/
RUN g++ -O2 -std=c++20 -Wall -Wextra -pedantic \
    /app/cpp/server.cpp /app/cpp/resp.cpp /app/cpp/commands.cpp /app/cpp/aof.cpp /app/cpp/metrics.cpp \
    -o /app/redis-lite

# Runtime with Node for dashboard
FROM node:20-slim
WORKDIR /srv
COPY --from=build /app/redis-lite /usr/local/bin/redis-lite
COPY dashboard/ /srv/
RUN npm i --omit=dev express
EXPOSE 6380 8080
CMD sh -c "/usr/local/bin/redis-lite 6380 & node server.js"
