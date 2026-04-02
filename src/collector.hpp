#pragma once

#include <string>
#include <vector>
#include <map>
#include <chrono>

struct ContainerMetric {
    std::string name;
    double cpu_percent;
    double ram_mb;
    long long timestamp;
};

class Collector {
public:
    Collector(const std::string& cgroup_root = "/sys/fs/cgroup");
    std::vector<ContainerMetric> collect();
    
    // For testing: Bypass Docker API mapping
    void set_container_mapping(const std::map<std::string, std::string>& mapping);

    // Make internal methods testable
    long long read_cpu_usage(const std::string& container_id);
    long long read_memory_usage(const std::string& container_id);
    long long read_memory_stat_key(const std::string& container_id, const std::string& key);

private:
    struct CpuSnapshot {
        long long usage_usec;
        std::chrono::steady_clock::time_point time;
    };

    std::string cgroup_root;
    std::map<std::string, std::string> id_to_name_map;
    bool bypass_docker_api = false;
    std::map<std::string, CpuSnapshot> prev_cpu_stats;

    void update_container_mapping();
    double calculate_cpu_percent(const std::string& container_id, long long current_usage_usec);
    long long read_file_long(const std::string& path);
    long long read_cpu_usage(const std::string& container_id);
    long long read_memory_usage(const std::string& container_id);
    long long read_memory_stat_key(const std::string& container_id, const std::string& key);
};
