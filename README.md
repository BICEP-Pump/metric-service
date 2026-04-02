# BICEP Metric Service

![Build Status](https://github.com/bicep-pump/metric-service/actions/workflows/unit-tests.yml/badge.svg)
![Coverage](https://codecov.io/gh/bicep-pump/metric-service/branch/main/graph/badge.svg)
![GHCR Version](https://ghcr-badge.egpl.dev/bicep-pump/metric-service/latest_tag?label=Latest%20Version)
![Last Commit](https://img.shields.io/github/last-commit/bicep-pump/metric-service)

High-performance C++ service to monitor containerized environments via cgroups v2.

## Key Features
- **Direct Scraper**: Parses `/sys/fs/cgroup` files for precision CPU (`usage_usec`) and RAM (`memory.current`) metrics.
- **Docker Mapping**: Resolves container IDs to names using the Docker Engine API.
- **Self-Discovered IP**: Automatically detects its host/container IP for registration if not provided.
- **Batched Exporter**: Efficiently buffers and pushes JSON metrics to a configurable endpoint.
- **Internal Healthcheck**: Simple HTTP `/health` server for orchestration.

## Configuration
All settings are managed via environment variables.

| Variable | Description | Default |
|----------|-------------|---------|
| `METRIC_ENDPOINT` | HTTP URL for metrics submission | (required) |
| `REGISTRATION_ENDPOINT` | HTTP URL for service discovery | (optional) |
| `SCRAPE_INTERVAL` | Seconds between scrapes | `10` |
| `BATCH_SIZE` | Metric count before pushing | `10` |
| `SERVICE_NAME` | Name reported to registry | `bicep-metric-service` |
| `SERVICE_PORT` | Healthcheck port | `8080` |
| `SERVICE_IP` | Service IP address | (auto-detected) |

## Development
Build requirements: `CMake 3.15+`, `gcc/g++ (C++20)`, `libcurl`, `docker-compose`.

### Build & Test
```bash
mkdir build && cd build
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
