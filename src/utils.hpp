#pragma once

#include <string>

struct Config {
    std::string metric_endpoint;
    std::string registration_endpoint;
    int scrape_interval = 10;
    int batch_size = 10;
    std::string service_name = "bicep-metric-service";
    int service_port = 8080;
    std::string service_ip = "";
};

std::string discover_ip();
std::string parse_ifaddrs(struct ifaddrs* ifaddr);
Config get_config();
