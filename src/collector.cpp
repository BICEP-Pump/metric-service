#include "collector.hpp"

#include <httplib.h>
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>
#include <filesystem>
#include <thread>
#include <chrono>

using json = nlohmann::json;
namespace fs = std::filesystem;

Collector::Collector(const std::string& root) : cgroup_root(root) {
    update_container_mapping();
}

void Collector::update_container_mapping() {
    httplib::Client cli("/var/run/docker.sock");
    cli.set_address_family(AF_UNIX);
    
    httplib::Headers headers = {
        {"Host", "localhost"}
    };
    
    auto res = cli.Get("/containers/json", headers);

    if (res && res->status == 200) {
        try {
            auto j = json::parse(res->body);
            id_to_name_map.clear();
            for (const auto& container : j) {
                std::string id = container["Id"];
                std::string name = container["Names"][0];
                if (name.front() == '/') name.erase(0, 1);
                id_to_name_map[id] = name;
            }
        } catch (const std::exception& e) {
            std::cerr << "Failed to parse Docker containers JSON: " << e.what() << std::endl;
        }
    } else {
        std::cerr << "Failed to query Docker API status code: " << (res ? std::to_string(res->status) : "error") << std::endl;
    }
}

long long Collector::read_file_long(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) return -1;
    long long value;
    file >> value;
    return value;
}

long long Collector::read_cpu_usage(const std::string& container_id) {
    // Try systemd path first
    std::string path = cgroup_root + "/system.slice/docker-" + container_id + ".scope/cpu.stat";
    if (!fs::exists(path)) {
        // Fallback to cgroupfs path
        path = cgroup_root + "/docker/" + container_id + "/cpu.stat";
    }

    std::ifstream file(path);
    if (!file.is_open()) return -1;

    std::string line, key;
    long long value;
    while (std::getline(file, line)) {
        std::stringstream ss(line);
        ss >> key >> value;
        if (key == "usage_usec") return value;
    }
    return -1;
}

long long Collector::read_memory_usage(const std::string& container_id) {
    std::string path = cgroup_root + "/system.slice/docker-" + container_id + ".scope/memory.current";
    if (!fs::exists(path)) {
        path = cgroup_root + "/docker/" + container_id + "/memory.current";
    }
    return read_file_long(path);
}

double Collector::calculate_cpu_percent(const std::string& container_id, long long current_usage_usec) {
    auto now = std::chrono::steady_clock::now();
    if (prev_cpu_stats.find(container_id) == prev_cpu_stats.end()) {
        prev_cpu_stats[container_id] = {current_usage_usec, now};
        return 0.0;
    }

    auto prev = prev_cpu_stats[container_id];
    long long delta_usec = current_usage_usec - prev.usage_usec;
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(now - prev.time).count();

    prev_cpu_stats[container_id] = {current_usage_usec, now};

    if (duration <= 0) return 0.0;
    return (static_cast<double>(delta_usec) / duration) * 100.0;
}

std::vector<ContainerMetric> Collector::collect() {
    update_container_mapping(); // Update names in case containers changed

    std::vector<ContainerMetric> metrics;
    long long timestamp = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());

    for (const auto& [id, name] : id_to_name_map) {
        long long cpu_usec = read_cpu_usage(id);
        long long mem_bytes = read_memory_usage(id);

        if (cpu_usec != -1 && mem_bytes != -1) {
            double cpu_pct = calculate_cpu_percent(id, cpu_usec);
            double ram_mb = static_cast<double>(mem_bytes) / 1024.0 / 1024.0;
            
            metrics.push_back({name, cpu_pct, ram_mb, timestamp});
        }
    }
    return metrics;
}
