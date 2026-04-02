#include <gtest/gtest.h>
#include "../src/utils.hpp"
#include <cstdlib>

TEST(UtilsTest, ConfigParsingDefaults) {
    // Clear env to test defaults
    unsetenv("METRIC_ENDPOINT");
    unsetenv("REGISTRATION_ENDPOINT");
    unsetenv("SCRAPE_INTERVAL");
    unsetenv("BATCH_SIZE");

    Config cfg = get_config();
    EXPECT_EQ(cfg.metric_endpoint, "");
    EXPECT_EQ(cfg.scrape_interval, 10);
    EXPECT_EQ(cfg.batch_size, 10);
    EXPECT_EQ(cfg.service_name, "bicep-metric-service");
}

TEST(UtilsTest, ConfigParsingEnv) {
    setenv("METRIC_ENDPOINT", "http://test:9000", 1);
    setenv("SCRAPE_INTERVAL", "5", 1);
    setenv("BATCH_SIZE", "20", 1);

    Config cfg = get_config();
    EXPECT_EQ(cfg.metric_endpoint, "http://test:9000");
    EXPECT_EQ(cfg.scrape_interval, 5);
    EXPECT_EQ(cfg.batch_size, 20);

    unsetenv("METRIC_ENDPOINT");
}

TEST(UtilsTest, IpDiscovery) {
    std::string ip = discover_ip();
    EXPECT_FALSE(ip.empty());
    // Should at least return loopback if nothing else
}
