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

Config get_config();
std::string discover_ip();
