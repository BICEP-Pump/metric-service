#include "collector.hpp"
#include "exporter.hpp"
#include "utils.hpp"

#include <httplib.h>
#include <nlohmann/json.hpp>
#include <iostream>
#include <string>
#include <thread>
#include <chrono>

using json = nlohmann::json;

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
    int retry_delay = 2; // seconds

    for (int i = 0; i < max_retries; ++i) {
        std::cout << "[Registration] Attempt " << (i + 1) << "/" << max_retries << " at " << config.registration_endpoint << std::endl;
        auto res = cli.Post(path.c_str(), j.dump(), "application/json");

        if (res && (res->status == 200 || res->status == 201 || res->status == 204)) {
            std::cout << "[Registration] Successfully registered." << std::endl;
            return;
        }

        std::cerr << "[Registration] Failed. Status: " << (res ? std::to_string(res->status) : "error") << ". Retrying in " << retry_delay << "s..." << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(retry_delay));
    }

    std::cerr << "[Registration] Final failure after " << max_retries << " attempts." << std::endl;
}

int main() {
    Config config = get_config();

    std::cout << "--- BICEP Metric Service Starting ---" << std::endl;
    std::cout << "Metric Endpoint: " << config.metric_endpoint << std::endl;
    std::cout << "Scrape Interval: " << config.scrape_interval << "s" << std::endl;
    std::cout << "Batch Size: " << config.batch_size << std::endl;

    // Healthcheck server
    httplib::Server svr;
    svr.Get("/health", [](const httplib::Request&, httplib::Response& res) {
        res.set_content("OK", "text/plain");
    });

    std::thread health_thread([&svr, &config]() {
        std::cout << "Healthcheck server listening on 0.0.0.0:" << config.service_port << std::endl;
        if (!svr.listen("0.0.0.0", config.service_port)) {
            std::cerr << "Failed to start healthcheck server" << std::endl;
        }
    });
    health_thread.detach();

    // Register
    register_service(config);

    Collector collector;
    Exporter exporter(config.metric_endpoint, config.batch_size);

    std::cout << "Scraping loop started." << std::endl;

    while (true) {
        auto start = std::chrono::steady_clock::now();
        
        auto metrics = collector.collect();
        exporter.add_metrics(metrics);

        auto end = std::chrono::steady_clock::now();
        auto diff = std::chrono::duration_cast<std::chrono::seconds>(end - start).count();

        int sleep_time = config.scrape_interval - static_cast<int>(diff);
        if (sleep_time > 0) {
            std::this_thread::sleep_for(std::chrono::seconds(sleep_time));
        }
    }

    return 0;
}
