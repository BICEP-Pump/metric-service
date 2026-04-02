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
