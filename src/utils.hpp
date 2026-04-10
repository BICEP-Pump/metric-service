#pragma once

#include <string>

enum class MetricExportMode {
    CORE,
    PROMETHEUS,
};

struct Config {
    std::string metric_endpoint;
    std::string registration_endpoint;
    int scrape_interval = 10;
    MetricExportMode metric_export_mode = MetricExportMode::CORE;
    std::string service_name = "bicep-metric-service";
    int service_port = 8080;
    std::string service_ip = "";
};

struct ParsedHttpUrl {
    std::string scheme = "http";
    std::string host;
    int port = 80;
    std::string path = "/";
};

struct HttpPostResult {
    int http_status = 0;
    int transport_error_code = 0;
    std::string transport_error_message;
    std::string response_body;
};

std::string discover_ip();
std::string parse_ifaddrs(struct ifaddrs* ifaddr);
Config get_config();
bool register_service(const Config& config);
void register_service_until_success(const Config& config);
std::string metric_export_mode_to_string(MetricExportMode mode);
bool parse_http_url(
    const std::string& url,
    ParsedHttpUrl& parsed_url,
    std::string& error_message
);
bool perform_http_post(
    const ParsedHttpUrl& parsed_url,
    const std::string& request_body,
    const std::string& content_type,
    HttpPostResult& result
);
