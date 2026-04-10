# BICEP Metric Service

![Build Status](https://github.com/bicep-pump/metric-service/actions/workflows/unit-tests.yml/badge.svg)
![Coverage](https://codecov.io/gh/bicep-pump/metric-service/branch/main/graph/badge.svg)
![GHCR Version](https://ghcr-badge.egpl.dev/bicep-pump/metric-service/latest_tag?label=Latest%20Version)
![Last Commit](https://img.shields.io/github/last-commit/bicep-pump/metric-service)

High-performance C++ service to monitor containerized environments via cgroups v2.

## Key Features
- **Direct Scraper**: Parses `/sys/fs/cgroup` files for precise CPU core usage derived from `usage_usec` deltas and RAM (`memory.current`) metrics.
- **Docker Mapping**: Resolves container IDs to names using the Docker Engine API.
- **Self-Discovered IP**: Automatically detects its host/container IP for registration if not provided.
- **Flexible Exporter**: Can push JSON to a core API or Prometheus text payloads directly to a Pushgateway-compatible endpoint.
- **Internal Healthcheck**: Simple HTTP `/health` server for orchestration.

## Metric Units
- `container_cpu_usage` is exported in CPU core units, not percent.
- `1.0` means one fully utilized CPU core, `0.5` means half a core, and `2.0` means two fully utilized cores.
- The JSON exporter reports CPU with the `cpu_usage` field in the same core units.
- `container_memory_usage_bytes` is exported in bytes, and the JSON exporter reports RAM as `ram_mb`.

## Configuration
All settings are managed via environment variables.

| Variable | Description | Default |
|----------|-------------|---------|
| `METRIC_ENDPOINT` | HTTP URL for metrics submission | (required) |
| `METRIC_EXPORT_MODE` | `core` for JSON-to-backend, `prometheus` for Pushgateway text export | `core` |
| `REGISTRATION_ENDPOINT` | HTTP URL for service discovery, for example `http://core:8000/metric-services/register` or `http://core:8000/metric-services/register/1` | (optional) |
| `SCRAPE_INTERVAL` | Seconds between scrapes; each scrape is pushed immediately | `10` |
| `SERVICE_NAME` | Name reported to registry | `bicep-metric-service` |
| `SERVICE_PORT` | Healthcheck port | `8080` |
| `SERVICE_IP` | Service IP address | (auto-detected) |

When `METRIC_EXPORT_MODE=prometheus`, `METRIC_ENDPOINT` should point at a Pushgateway-compatible URL such as `http://host:9091/metrics/job/metric_service_host_1`.

## Development
Build requirements: `CMake 3.15+`, `gcc/g++ (C++20)`, `libcurl`, `docker-compose`.

### Build & Test
```bash
mkdir -p build && cd build
cmake -DBUILD_TESTS=ON ..
make -j$(nproc)
./bicep_tests
```

### Local Stack (with Mock Backend)
To verify everything works end-to-end:
```bash
./tests/integration_test.sh
```

## Deployment
Mount the Docker socket and cgroup filesystem:
```yaml
volumes:
  - /var/run/docker.sock:/var/run/docker.sock:ro
  - /sys/fs/cgroup:/sys/fs/cgroup:ro
```
Requires `SYS_ADMIN` capabilities or `privileged: true` depending on the host OS security.

## CI/CD
- **Unit Tests**: GitHub Action running GTest and uploading coverage to Codecov.
- **Integration**: Automated E2E verification via `docker-compose`.
- **Registry**: Pre-built images pushed to `ghcr.io`.
