// Copyright 2017-2020 The Verible Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "verible/common/util/init-command-line.h"

#include <algorithm>
#include <cstdlib>
#include <string>
#include <string_view>
#include <vector>

#include "absl/base/log_severity.h"
#include "absl/debugging/failure_signal_handler.h"
#include "absl/debugging/symbolize.h"
#include "absl/flags/parse.h"
#include "absl/flags/usage.h"
#include "absl/flags/usage_config.h"
#include "absl/log/globals.h"
#include "absl/log/initialize.h"
#include "absl/strings/numbers.h"
#include "absl/time/time.h"
#include "verible/common/util/generated-verible-build-version.h"

namespace verible {

std::string GetRepositoryVersion() {
#ifdef VERIBLE_GIT_DESCRIBE
  return VERIBLE_GIT_DESCRIBE;  // from --workspace_status_command
#elif defined(VERIBLE_MODULE_VERSION)
  return VERIBLE_MODULE_VERSION;  // from MODULE.bazel via module-version.bzl
#else
  return "<unknown repository version>";
#endif
}

// Long-form of build version, might contain multiple lines
// Build a version string with as much as possible info.
static std::string GetBuildVersion() {
  std::string result;
  result.append("Version\t").append(GetRepositoryVersion()).append("\n");
#ifdef VERIBLE_COMMIT_TIMESTAMP
  result.append("Commit-Timestamp\t")
      .append(absl::FormatTime("%Y-%m-%dT%H:%M:%SZ",
                               absl::FromTimeT(VERIBLE_COMMIT_TIMESTAMP),
                               absl::UTCTimeZone()))
      .append("\n");
#elif defined(VERIBLE_GIT_DATE)  // Legacy
  result.append("Commit-Date\t").append(VERIBLE_GIT_DATE).append("\n");
#endif
#ifdef VERIBLE_BUILD_TIMESTAMP
  result.append("Built\t")
      .append(absl::FormatTime("%Y-%m-%dT%H:%M:%SZ",
                               absl::FromTimeT(VERIBLE_BUILD_TIMESTAMP),
                               absl::UTCTimeZone()))
      .append("\n");
#endif
  return result;
}

void SetLoggingLevelsFromEnvironment() {
  // To avoid confusing and rarely used flags, we just enable logging via
  // environment variables.
  const char *const stderr_log_level = getenv("VERIBLE_LOGTHRESHOLD");
  int log_level = 0;
  if (stderr_log_level && absl::SimpleAtoi(stderr_log_level, &log_level)) {
    absl::SetStderrThreshold(
        static_cast<absl::LogSeverityAtLeast>(std::clamp(log_level, 0, 3)));
  }

  // Set vlog-level with environment variable. The definition of
  // VERIBLE_INTERNAL_SET_VLOGLEVEL() might be different depending on if we
  // have an absl implementation or not.
  const char *const vlog_level_env = getenv("VERIBLE_VLOG_DETAIL");
  int vlog_level = 0;
  if (vlog_level_env && absl::SimpleAtoi(vlog_level_env, &vlog_level)) {
    absl::SetGlobalVLogLevel(vlog_level);
  }
}

// We might want to have argc edited in the future, hence non-const param.
std::vector<std::string_view> InitCommandLine(
    std::string_view usage,
    int *argc,  // NOLINT(readability-non-const-parameter)
    char ***argv) {
  absl::InitializeSymbolizer(*argv[0]);
  absl::FlagsUsageConfig usage_config;
  usage_config.version_string = GetBuildVersion;
  absl::SetFlagsUsageConfig(usage_config);
  absl::SetProgramUsageMessage(usage);  // copies usage string

  SetLoggingLevelsFromEnvironment();
  absl::InitializeLog();

  // Print stacktrace on issue, but not if --config=asan
  // which comes with its own stacktrace handling.
#if !defined(__SANITIZE_ADDRESS__)
  absl::FailureSignalHandlerOptions options;
  absl::InstallFailureSignalHandler(options);
#endif

  const auto positional_parameters = absl::ParseCommandLine(*argc, *argv);
  return {positional_parameters.cbegin(), positional_parameters.cend()};
}

}  // namespace verible
