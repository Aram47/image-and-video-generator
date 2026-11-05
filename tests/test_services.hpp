#pragma once

#include "common/logger.hpp"
#include "common/metrics.hpp"
#include "storage/memory_job_store.hpp"
#include "storage/output_quota.hpp"

#include <cstdint>
#include <filesystem>
#include <limits>

struct TestServices {
  gen::MemoryJobStore store;
  gen::OutputQuota quota;
  gen::Metrics metrics;

  explicit TestServices(gen::Logger& logger)
      : quota(std::filesystem::temp_directory_path() / "gen-quota-unused", std::numeric_limits<std::uint64_t>::max() / 4),
        metrics(logger, {}) {}

  TestServices(gen::Logger& logger, std::filesystem::path output, std::uint64_t limit)
      : quota(std::move(output), limit), metrics(logger, {}) {}
};
