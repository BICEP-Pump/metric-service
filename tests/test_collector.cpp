#include <gtest/gtest.h>
#include "../src/collector.hpp"
#include <filesystem>
#include <fstream>
#include <thread>
#include <chrono>

namespace fs = std::filesystem;

class CollectorTest : public ::testing::Test {
protected:
    std::string test_root = "/tmp/bicep_test_cgroup";

    void SetUp() override {
        fs::create_directories(test_root + "/system.slice/docker-123.scope");
        fs::create_directories(test_root + "/docker/456");
    }

    void TearDown() override {
        fs::remove_all(test_root);
    }

    void write_file(const std::string& path, const std::string& content) {
        std::ofstream ofs(path);
        ofs << content;
    }
};

TEST_F(CollectorTest, FullCollectionLoop) {
    Collector collector(test_root);
    std::map<std::string, std::string> mapping = {{"123", "test-container"}};
    collector.set_container_mapping(mapping);

    // Setup mock cgroup files
    write_file(test_root + "/system.slice/docker-123.scope/cpu.stat", "usage_usec 1000\n");
    write_file(test_root + "/system.slice/docker-123.scope/memory.current", "1048576\n"); // 1MB
    write_file(test_root + "/system.slice/docker-123.scope/memory.stat", "inactive_file 0\n");

    auto metrics = collector.collect();
    ASSERT_EQ(metrics.size(), 1);
    EXPECT_EQ(metrics[0].name, "test-container");
    EXPECT_EQ(metrics[0].ram_mb, 1.0);
}

TEST_F(CollectorTest, CpuPercentCalculation) {
    Collector collector(test_root);
    std::string id = "123";

    // First sample
    double p1 = collector.calculate_cpu_percent(id, 1000000); // 1s usage
    EXPECT_EQ(p1, 0.0);

    // Wait a bit
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    // Second sample (0.1s usage in the elapsed time)
    double p2 = collector.calculate_cpu_percent(id, 1100000); 
    EXPECT_GT(p2, 0.0);
}

TEST_F(CollectorTest, MemoryRefinement) {
    Collector collector(test_root);
    
    write_file(test_root + "/docker/456/memory.current", "2097152\n"); // 2MB
    write_file(test_root + "/docker/456/memory.stat", "inactive_file 1048576\n"); // 1MB cache
    
    long long usage = collector.read_memory_usage("456");
    EXPECT_EQ(usage, 1048576); // Should return 1MB (2MB - 1MB)
}

TEST_F(CollectorTest, MissingFilesHandling) {
    Collector collector(test_root);
    
    EXPECT_EQ(collector.read_cpu_usage("999"), -1);
    EXPECT_EQ(collector.read_memory_usage("999"), -1);
}

TEST_F(CollectorTest, PermissionDenied) {
    std::string path = test_root + "/docker/456/memory.current";
    write_file(path, "12345\n");
    
    // Remove read permissions
    fs::permissions(path, fs::perms::none);
    
    Collector collector(test_root);
    EXPECT_EQ(collector.read_file_long(path), -1);
    
    // Restore for cleanup
    fs::permissions(path, fs::perms::owner_read | fs::perms::owner_write);
}

TEST_F(CollectorTest, SymbolicLinkSupport) {
    std::string real_path = test_root + "/real_cpu_stat";
    write_file(real_path, "usage_usec 5000\n");
    
    std::string link_path = test_root + "/docker/456/cpu.stat";
    fs::create_symlink(real_path, link_path);
    
    Collector collector(test_root);
    EXPECT_EQ(collector.read_cpu_usage("456"), 5000);
}

TEST_F(CollectorTest, MalformedStatFile) {
    std::string path = test_root + "/docker/456/memory.stat";
    write_file(path, "only_key_no_value\nwrong_value_type string\ninactive_file 999\n");
    
    Collector collector(test_root);
    EXPECT_EQ(collector.read_memory_stat_key("456", "inactive_file"), 999);
    EXPECT_EQ(collector.read_memory_stat_key("456", "only_key_no_value"), -1);
}
