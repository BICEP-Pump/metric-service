#include "exporter.hpp"
#include <nlohmann/json.hpp>
#include <httplib.h>
#include <iostream>

using json = nlohmann::json;

Exporter::Exporter(const std::string& ep, int bs) : endpoint(ep), batch_size(bs) {
    // Default push function: the real HTTP push
    push_func = [this](const std::vector<ContainerMetric>& m) {
        this->push_to_endpoint(m);
    };
}

void Exporter::set_push_function(PushFunc func) {
    std::lock_guard<std::mutex> lock(mtx);
    push_func = func;
}

void Exporter::add_metrics(const std::vector<ContainerMetric>& metrics) {
    std::lock_guard<std::mutex> lock(mtx);
    for (const auto& m : metrics) {
        buffer.push_back(m);
    }

    if (buffer.size() >= static_cast<size_t>(batch_size)) {
        push_func(buffer);
        buffer.clear();
    }
}

void Exporter::flush() {
    std::lock_guard<std::mutex> lock(mtx);
    if (!buffer.empty()) {
        push_func(buffer);
        buffer.clear();
    }
}

void Exporter::push_to_endpoint(const std::vector<ContainerMetric>& metrics) {
    if (endpoint.empty()) {
        std::cout << "[Exporter] Dry run: Endpoint not set. Would have pushed " << metrics.size() << " metrics." << std::endl;
        return;
    }

    json j = json::array();
    for (const auto& m : metrics) {
        j.push_back({
            {"container_name", m.name},
            {"cpu_percent", m.cpu_percent},
            {"ram_mb", m.ram_mb},
            {"timestamp", m.timestamp}
        });
    }

    std::string body = j.dump();

    // Parse endpoint into host and path
    // Assuming http://host[:port]/path
    std::string protocol = "http://";
    std::string url = endpoint;
    if (url.find(protocol) == 0) {
        url.erase(0, protocol.length());
    }

    size_t path_pos = url.find('/');
    std::string host = (path_pos == std::string::npos) ? url : url.substr(0, path_pos);
    std::string path = (path_pos == std::string::npos) ? "/" : url.substr(path_pos);

    httplib::Client cli(host);
    auto res = cli.Post(path.c_str(), body, "application/json");

    if (res) {
        if (res->status == 200 || res->status == 201 || res->status == 204) {
            std::cout << "[Exporter] Successfully pushed " << metrics.size() << " metrics." << std::endl;
        } else {
            std::cerr << "[Exporter] Failed to push metrics. Status: " << res->status << std::endl;
            std::cerr << "[Exporter] Response: " << res->body << std::endl;
        }
    } else {
        std::cerr << "[Exporter] Failed to push metrics. Connection error." << std::endl;
    }
}
