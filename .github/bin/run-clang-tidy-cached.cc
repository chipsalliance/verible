#if 0  // Invoke with /bin/sh or simply add executable bit on this file on Unix.
B=${0%%.cc}; [ "$B" -nt "$0" ] || c++ -std=c++17 -o"$B" "$0" && exec "$B" "$@";
#endif
// Copyright 2023 The Verible Authors.
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

// Script to run clang-tidy on files in a bazel project while caching the
// results as clang-tidy can be pretty slow. The clang-tidy output messages
// are content-addressed in a hash(cc-file-content) cache file.
// Should run on any system with a shell that provides 2>/dev/null redirect.
//
// Invocation without parameters simply uses the .clang-tidy config to run on
// all *.{cc,h} files. Additional parameters passed to this script are passed
// to clang-tidy as-is. Typical use could be for instance
//   run-clang-tidy-cached.cc --checks="-*,modernize-use-override" --fix

#include <algorithm>
#include <cinttypes>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <list>
#include <map>
#include <mutex>
#include <optional>
#include <regex>
#include <thread>

namespace fs = std::filesystem;
using filepath_contenthash_t = std::pair<fs::path, uint64_t>;
using ReIt = std::sregex_iterator;

// Some helpers
std::string GetContent(FILE *f) {
  std::string result;
  char buf[4096];
  while (size_t r = fread(buf, 1, sizeof(buf), f)) result.append(buf, r);
  fclose(f);
  return result;
}

std::string GetContent(const fs::path &f) {
  return GetContent(fopen(f.string().c_str(), "r"));
}

using hash_t = uint64_t;
hash_t hash(const std::string &s) { return std::hash<std::string>()(s); }
std::string toHex(uint64_t value, int show_lower_nibbles = 16) {
  char out[16 + 1];
  snprintf(out, sizeof(out), "%016" PRIx64, value);
  return out + (16 - show_lower_nibbles);
}

std::optional<std::string> ReadAndVerifyTidyConfig(const fs::path &config) {
  auto content = GetContent(config);
  auto start_config = content.find("\nChecks:");
  if (start_config == std::string::npos) {
    std::cerr << "Not seen 'Checks:' in config " << config << "\n";
    return std::nullopt;
  }
  if (content.find('#', start_config) != std::string::npos) {
    std::cerr << "Comment found in check section of " << config << "\n";
    return std::nullopt;
  }
  return content.substr(start_config);
}

fs::path GetCacheDir() {
  if (const char *from_env = getenv("CACHE_DIR")) return fs::path(from_env);
  if (const char *home = getenv("HOME")) {
    if (auto cdir = fs::path(home) / ".cache/"; fs::exists(cdir)) return cdir;
  }
  return fs::path(getenv("TMPDIR") ?: "/tmp");
}

// Fix filename paths that are not emitted relative to project root.
void CanonicalizeSourcePaths(const fs::path &infile, const fs::path &outfile) {
  static const std::regex sFixPathsRe = []() {
    std::string canonicalize_expr = "(^|\\n)(";  // fix names at start of line
    auto root = GetContent(popen("bazel info execution_root 2>/dev/null", "r"));
    if (!root.empty()) {
      root.pop_back();  // remove newline.
      canonicalize_expr += root + "/|";
    }
    canonicalize_expr += fs::current_path().string() + "/";  // $(pwd)/
    canonicalize_expr += ")?(\\./)?";  // Some start with, or have a trailing ./
    return std::regex{canonicalize_expr};
  }();
  const auto in_content = GetContent(infile);
  std::fstream out_stream(outfile, std::ios::out);
  out_stream << std::regex_replace(in_content, sFixPathsRe, "$1");
}

// Given a work-queue in/out-file, process it. Using system() for portability.
void ClangTidyProcessFiles(const fs::path &content_dir, const std::string &cmd,
                           std::list<filepath_contenthash_t> *work_queue) {
  if (work_queue->empty()) return;
  const int kJobs = std::thread::hardware_concurrency();
  std::cerr << work_queue->size() << " files to process...";

  std::mutex queue_access_lock;
  auto clang_tidy_runner = [&]() {
    for (;;) {
      filepath_contenthash_t work;
      {
        const std::lock_guard<std::mutex> lock(queue_access_lock);
        if (work_queue->empty()) return;
        fprintf(stderr, "%5d\b\b\b\b\b", static_cast<int>(work_queue->size()));
        work = work_queue->front();
        work_queue->pop_front();
      }
      const fs::path final_out = content_dir / toHex(work.second);
      const std::string tmp_out = final_out.string() + ".tmp";
      const std::string command = cmd + " '" + work.first.string() + "'" +
                                  "> '" + tmp_out + "' 2>/dev/null";
      const int r = system(command.c_str());
#ifdef WIFSIGNALED
      if (WIFSIGNALED(r) && (WTERMSIG(r) == SIGINT || WTERMSIG(r) == SIGQUIT)) {
        break;  // got Ctrl-C
      }
#endif
      CanonicalizeSourcePaths(tmp_out, tmp_out);
      fs::rename(tmp_out, final_out);  // atomic replacement
    }
  };
  std::vector<std::thread> workers;
  for (auto i = 0; i < kJobs; ++i) workers.emplace_back(clang_tidy_runner);
  for (auto &t : workers) t.join();
  fprintf(stderr, "     \n");  // Clean out progress counter.
}

int main(int argc, char *argv[]) {
  const std::string kProjectPrefix = "verible_";
  const std::string kSearchDir = ".";
  const std::string kFileExcludeRe = "vscode/|external_libs/|.github/";

  const std::string kTidySymlink = kProjectPrefix + "clang-tidy.out";
  const fs::path cache_dir = GetCacheDir() / "clang-tidy";

  if (!fs::exists("compile_commands.json")) {
    std::cerr << "No compilation db found. First, run make-compilation-db.sh\n";
    return EXIT_FAILURE;
  }
  const auto config = ReadAndVerifyTidyConfig(".clang-tidy");
  if (!config) return EXIT_FAILURE;

  // We'll invoke clang-tidy with all the additional flags user provides.
  const std::string clang_tidy = getenv("CLANG_TIDY") ?: "clang-tidy";
  std::string clang_tidy_invocation = clang_tidy + " --quiet";
  clang_tidy_invocation.append(" \"--config=").append(*config).append("\"");
  for (int i = 1; i < argc; ++i) {
    clang_tidy_invocation.append(" \"").append(argv[i]).append("\"");
  }

  // Use major version as part of name of our configuration specific dir.
  auto version = GetContent(popen((clang_tidy + " --version").c_str(), "r"));
  std::smatch version_match;
  const std::string major_version =
      std::regex_search(version, version_match, std::regex{"version ([0-9]+)"})
          ? version_match[1].str()
          : "UNKNOWN";

  // Cache directory name based on configuration.
  const fs::path project_base_dir =
      cache_dir / fs::path(kProjectPrefix + "v" + major_version + "_" +
                           toHex(hash(version + clang_tidy_invocation), 8));
  const fs::path tidy_outfile = project_base_dir / "tidy.out";
  const fs::path content_dir = project_base_dir / "contents";
  fs::create_directories(content_dir);
  std::cerr << "Cache dir " << project_base_dir << "\n";

  // Gather all *.cc and *.h files; remember content hashes of includes.
  std::vector<filepath_contenthash_t> files_of_interest;
  std::map<std::string, hash_t> header_hashes;
  const std::regex exclude_re(kFileExcludeRe);
  for (const auto &dir_entry : fs::recursive_directory_iterator(kSearchDir)) {
    const fs::path &p = dir_entry.path().lexically_normal();
    if (!fs::is_regular_file(p)) continue;
    if (!kFileExcludeRe.empty() && std::regex_search(p.string(), exclude_re)) {
      continue;
    }
    if (auto ext = p.extension(); ext == ".cc" || ext == ".h") {
      files_of_interest.emplace_back(p, 0);
      if (ext == ".h") header_hashes[p.string()] = hash(GetContent(p));
    }
  }
  std::cerr << files_of_interest.size() << " files of interest.\n";

  // Create content hash address. If any header a file depends on changes, we
  // want to reprocess. So we make the hash dependent on header content as well.
  std::list<filepath_contenthash_t> work_queue;
  const std::regex inc_re("\"([0-9a-zA-Z_/-]+\\.h)\"");  // match include
  for (filepath_contenthash_t &f : files_of_interest) {
    const auto content = GetContent(f.first);
    f.second = hash(content);
    for (ReIt it(content.begin(), content.end(), inc_re); it != ReIt(); ++it) {
      const std::string &header_path = (*it)[1].str();
      f.second ^= header_hashes[header_path];
    }
    const fs::path content_hash_file = content_dir / toHex(f.second);
    if (!exists(content_hash_file)) work_queue.emplace_back(f);
  }

  // Run clang tidy in parallel on the files to process.
  ClangTidyProcessFiles(content_dir, clang_tidy_invocation, &work_queue);

  // Assemble the separate outputs into a single file. Tally up per-check stats.
  const std::regex check_re("(\\[[a-zA-Z.-]+\\])\n");
  std::map<std::string, int> checks_seen;
  std::ofstream tidy_collect(tidy_outfile);
  for (const filepath_contenthash_t &f : files_of_interest) {
    const auto tidy = GetContent(content_dir / toHex(f.second));
    if (!tidy.empty()) tidy_collect << f.first.string() << ":\n" << tidy;
    for (ReIt it(tidy.begin(), tidy.end(), check_re); it != ReIt(); ++it) {
      checks_seen[(*it)[1].str()]++;
    }
  }
  std::error_code ignored_error;
  fs::remove(kTidySymlink, ignored_error);
  fs::create_symlink(tidy_outfile, kTidySymlink, ignored_error);

  if (checks_seen.empty()) {
    std::cerr << "No clang-tidy complaints. 😎\n";
  } else {
    std::cerr << "--- Summary --- (details in " << kTidySymlink << ")\n";
    using check_count_t = std::pair<std::string, int>;
    std::vector<check_count_t> by_count(checks_seen.begin(), checks_seen.end());
    std::stable_sort(by_count.begin(), by_count.end(),
                     [](const check_count_t &a, const check_count_t &b) {
                       return b.second < a.second;  // reverse count
                     });
    for (const auto &counts : by_count) {
      fprintf(stdout, "%5d %s\n", counts.second, counts.first.c_str());
    }
  }
  return checks_seen.empty() ? EXIT_SUCCESS : EXIT_FAILURE;
}
