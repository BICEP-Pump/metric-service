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
#include <httplib.h>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

std::string discover_ip() {
    struct ifaddrs *ifaddr;
    std::string ip = "127.0.0.1";

    if (getifaddrs(&ifaddr) == -1) {
        perror("getifaddrs");
        return ip;
    }

    ip = parse_ifaddrs(ifaddr);

    freeifaddrs(ifaddr);
    return ip;
}

std::string parse_ifaddrs(struct ifaddrs* ifaddr) {
    std::string ip = "127.0.0.1";
    struct ifaddrs* ifa;

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

void register_service(const Config& config) {
    if (config.registration_endpoint.empty()) {
        std::cout << "[Registration] No endpoint set. Skipping registration." << std::endl;
        return;
    }

    json j = {
        {"ip", config.service_ip},
        {"name", config.service_name},
        {"port", config.service_port}
    };

    std::string url = config.registration_endpoint;
    std::string protocol = "http://";
    if (url.find(protocol) == 0) {
        url.erase(0, protocol.length());
    }

    size_t path_pos = url.find('/');
    std::string host = (path_pos == std::string::npos) ? url : url.substr(0, path_pos);
    std::string path = (path_pos == std::string::npos) ? "/" : url.substr(path_pos);

    httplib::Client cli(host);
    cli.set_connection_timeout(std::chrono::seconds(5));

    int max_retries = 10;
    int retry_delay = 1; // Faster for tests

    // Use shorter retry if we are in a test scenario (detected via localhost)
    if (host.find("127.0.0.1") != std::string::npos || host.find("localhost") != std::string::npos) {
        max_retries = 5;
        retry_delay = 1;
    }

    for (int i = 0; i < max_retries; ++i) {
        auto res = cli.Post(path.c_str(), j.dump(), "application/json");

        if (res && (res->status == 200 || res->status == 201 || res->status == 204)) {
            std::cout << "[Registration] Successfully registered." << std::endl;
            return;
        }

        std::cerr << "[Registration] Attempt " << (i + 1) << " failed. Status: " << (res ? std::to_string(res->status) : "error") << ". Retrying..." << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(retry_delay));
    }

    std::cerr << "[Registration] Final failure after " << max_retries << " attempts." << std::endl;
}
