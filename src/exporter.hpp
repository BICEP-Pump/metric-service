#pragma once

#include "collector.hpp"
#include "utils.hpp"
#include <string>
#include <vector>
#include <mutex>
#include <functional>

class Exporter {
public:
    using PushFunc = std::function<void(const std::vector<ContainerMetric>&)>;

    Exporter(
        const std::string& endpoint,
        MetricExportMode export_mode = MetricExportMode::CORE
    );
    void add_metrics(const std::vector<ContainerMetric>& metrics);

    // For testing
    void set_push_function(PushFunc func);

private:
    std::string endpoint;
    MetricExportMode export_mode;
    std::mutex mtx;
    PushFunc push_func;

    void push_to_endpoint(const std::vector<ContainerMetric>& metrics);
    void push_to_core_endpoint(const std::vector<ContainerMetric>& metrics);
    void push_to_prometheus_endpoint(const std::vector<ContainerMetric>& metrics);
    std::string build_prometheus_payload(
        const std::vector<ContainerMetric>& metrics
    ) const;
    std::string escape_prometheus_label_value(const std::string& value) const;
};
