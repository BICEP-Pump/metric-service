#include <gtest/gtest.h>
#include "../src/exporter.hpp"
#include <vector>

TEST(ExporterTest, BatchingLogic) {
    int push_count = 0;
    int total_metrics_received = 0;

    Exporter exporter("http://localhost:8080", 5);
    exporter.set_push_function([&](const std::vector<ContainerMetric>& metrics) {
        push_count++;
        total_metrics_received += metrics.size();
    });

    std::vector<ContainerMetric> batch1 = {
        {"c1", 1.0, 100.0, 1000},
        {"c2", 2.0, 200.0, 1000}
    };

    // Add 2 -> size 2, no push (batch size 5)
    exporter.add_metrics(batch1);
    EXPECT_EQ(push_count, 0);

    // Add 3 -> size 5, push!
    std::vector<ContainerMetric> batch2 = {
        {"c3", 3.0, 300.0, 1001},
        {"c4", 4.0, 400.0, 1001},
        {"c5", 5.0, 500.0, 1001}
    };
    exporter.add_metrics(batch2);
    EXPECT_EQ(push_count, 1);
    EXPECT_EQ(total_metrics_received, 5);

    // Flush remaining
    std::vector<ContainerMetric> batch3 = {
        {"c6", 6.0, 600.0, 1002}
    };
    exporter.add_metrics(batch3);
    EXPECT_EQ(push_count, 1); // No new push yet
    
    exporter.flush();
    EXPECT_EQ(push_count, 2);
    EXPECT_EQ(total_metrics_received, 6);
}

TEST(ExporterTest, EmptyMetrics) {
    int push_count = 0;
    Exporter exporter("http://localhost:8080", 5);
    exporter.set_push_function([&](const std::vector<ContainerMetric>&) {
        push_count++;
    });

    exporter.add_metrics({});
    EXPECT_EQ(push_count, 0);
    exporter.flush();
    EXPECT_EQ(push_count, 0);
}

TEST(ExporterTest, BatchSizeOne) {
    int push_count = 0;
    Exporter exporter("http://localhost:8080", 1);
    exporter.set_push_function([&](const std::vector<ContainerMetric>&) {
        push_count++;
    });

    exporter.add_metrics({{"c1", 1.0, 100.0, 1000}});
    EXPECT_EQ(push_count, 1);
}
