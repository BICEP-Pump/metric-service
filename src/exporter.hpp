#pragma once

#include "collector.hpp"
#include <string>
#include <vector>
#include <mutex>
#include <functional>

class Exporter {
public:
    using PushFunc = std::function<void(const std::vector<ContainerMetric>&)>;

    Exporter(const std::string& endpoint, int batch_size);
    void add_metrics(const std::vector<ContainerMetric>& metrics);
    void flush();

    // For testing
    void set_push_function(PushFunc func);

private:
    std::string endpoint;
    int batch_size;
    std::vector<ContainerMetric> buffer;
    std::mutex mtx;
    PushFunc push_func;

    void push_to_endpoint(const std::vector<ContainerMetric>& metrics);
};
