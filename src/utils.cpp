#include "utils.hpp"

#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <cstdlib>
#include <ifaddrs.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/socket.h>

std::string discover_ip() {
    struct ifaddrs *ifaddr, *ifa;
    std::string ip = "127.0.0.1";

    if (getifaddrs(&ifaddr) == -1) {
        perror("getifaddrs");
        return ip;
    }

    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == NULL) continue;

        if (ifa->ifa_addr->sa_family == AF_INET) { // IPv4
            char host[NI_MAXHOST];
            int s = getnameinfo(ifa->ifa_addr, sizeof(struct sockaddr_in), host, NI_MAXHOST, NULL, 0, NI_NUMERICHOST);
            if (s == 0) {
                std::string current_ip = host;
                if (current_ip != "127.0.0.1") {
                    ip = current_ip;
                    // If we find an IP that looks like a docker internal IP (172.x), we might prefer it
                    if (ip.find("172.") == 0) {
                        break; 
                    }
                }
            }
        }
    }

    freeifaddrs(ifaddr);
    return ip;
}

Config get_config() {
    Config config;
    const char* me = std::getenv("METRIC_ENDPOINT");
    if (me) config.metric_endpoint = me;

    const char* re = std::getenv("REGISTRATION_ENDPOINT");
    if (re) config.registration_endpoint = re;

    const char* si = std::getenv("SCRAPE_INTERVAL");
    if (si) config.scrape_interval = std::stoi(si);

    const char* bs = std::getenv("BATCH_SIZE");
    if (bs) config.batch_size = std::stoi(bs);

    const char* sn = std::getenv("SERVICE_NAME");
    if (sn) config.service_name = sn;

    const char* sp = std::getenv("SERVICE_PORT");
    if (sp) config.service_port = std::stoi(sp);

    const char* sip = std::getenv("SERVICE_IP");
    if (sip) {
        config.service_ip = sip;
    } else {
        config.service_ip = discover_ip();
        std::cout << "[Config] Auto-discovered IP: " << config.service_ip << std::endl;
    }

    return config;
}
