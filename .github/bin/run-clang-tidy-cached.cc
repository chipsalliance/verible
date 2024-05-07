#if 0  // Invoke with /bin/sh or simply add executable bit on this file on Unix.
B=${0%%.cc}; [ "$B" -nt "$0" ] || c++ -std=c++17 -o"$B" "$0" && exec "$B" "$@";
#endif
// Copyright 2023 Henner Zeller <h.zeller@acm.org>
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
//
// Note: usefule environment variables to configure are
//  CLANG_TIDY = binary to run; default would just be clang-tidy.
//  CACHE_DIR  = where to put the cached content; default ~/.cache

// This file shall be self-contained, so we don't use any re2 or absl niceties
#include <algorithm>
#include <cerrno>
#include <cinttypes>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <list>
#include <map>
#include <mutex>
#include <regex>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>

// Some configuration for this project.
static const std::string kProjectCachePrefix = "verible_";
static constexpr std::string_view kWorkspaceFile = "MODULE.bazel";

static constexpr std::string_view kSearchDir = ".";
static const std::string kFileExcludeRe =
    ".git/|.github/"         // stuff we're not interested in
    "|vscode/"               // some generated code in there
    "|tree_operations_test"  // very slow
    "|symbol_table_test";

static constexpr std::string_view kClangConfigFile = ".clang-tidy";
static constexpr std::string_view kExtraArgs[] = {"-Wno-unknown-pragmas"};

namespace fs = std::filesystem;
using file_time = std::filesystem::file_time_type;
using ReIt = std::sregex_iterator;
using filepath_contenthash_t = std::pair<fs::path, uint64_t>;

// Some helpers
std::string GetContent(FILE *f) {
  std::string result;
  if (!f) return result;  // ¯\_(ツ)_/¯ best effort.
  char buf[4096];
  while (const size_t r = fread(buf, 1, sizeof(buf), f)) {
    result.append(buf, r);
  }
  fclose(f);
  return result;
}

std::string GetContent(const fs::path &f) {
  FILE *file_to_read = fopen(f.string().c_str(), "r");
  if (!file_to_read) {
    fprintf(stderr, "%s: can't open: %s\n", f.string().c_str(),
            strerror(errno));
    return {};  // ¯\_(ツ)_/¯ best effort.
  }
  return GetContent(file_to_read);
}

std::string GetCommandOutput(const std::string &prog) {
  return GetContent(popen(prog.c_str(), "r"));  // NOLINT
}

using hash_t = uint64_t;
hash_t hash(const std::string &s) { return std::hash<std::string>()(s); }
std::string toHex(uint64_t value, int show_lower_nibbles = 16) {
  char out[16 + 1];
  snprintf(out, sizeof(out), "%016" PRIx64, value);
  return out + (16 - show_lower_nibbles);
}

// Mapping filepath_contenthash_t to an actual location in the file system.
class ContentAddressedStore {
 public:
  explicit ContentAddressedStore(const fs::path &project_base_dir)
      : content_dir(project_base_dir / "contents") {
    fs::create_directories(content_dir);
  }

  // Given filepath contenthash, return the path to read/write from.
  fs::path PathFor(const filepath_contenthash_t &c) const {
    // Name is human readable, the content hash makes it unique.
    std::string name_with_contenthash = c.first.filename().string();
    name_with_contenthash.append("-").append(toHex(c.second));
    return content_dir / name_with_contenthash;
  }

  std::string GetContentFor(const filepath_contenthash_t &c) const {
    return GetContent(PathFor(c));
  }

  // Check if this needs to be recreated, either because it is not there,
  // or is not empty and does not fit freshness requirements.
  bool NeedsRefresh(const filepath_contenthash_t &c,
                    file_time min_freshness) const {
    const fs::path content_hash_file = PathFor(c);
    // Recreate if we don't have it yet or if it contains messages but is
    // older than WORKSPACE or compilation db. Maybe something got fixed.
    return (!fs::exists(content_hash_file) ||
            (fs::file_size(content_hash_file) > 0 &&
             fs::last_write_time(content_hash_file) < min_freshness));
  }

 private:
  const fs::path content_dir;
};

class ClangTidyRunner {
 public:
  ClangTidyRunner(int argc, char **argv)
      : clang_tidy_(getenv("CLANG_TIDY") ?: "clang-tidy"),
        clang_tidy_args_(AssembleArgs(argc, argv)) {
    project_cache_dir_ = AssembleProjectCacheDir();
  }

  const fs::path &project_cache_dir() const { return project_cache_dir_; }

  // Given a work-queue in/out-file, process it. Using system() for portability.
  void RunClangTidyOn(ContentAddressedStore &output_store,
                      std::list<filepath_contenthash_t> work_queue) {
    if (work_queue.empty()) return;
    const int kJobs = std::thread::hardware_concurrency();
    std::cerr << work_queue.size() << " files to process...";

    std::mutex queue_access_lock;
    auto clang_tidy_runner = [&]() {
      for (;;) {
        filepath_contenthash_t work;
        {
          const std::lock_guard<std::mutex> lock(queue_access_lock);
          if (work_queue.empty()) return;
          fprintf(stderr, "%5d\b\b\b\b\b", static_cast<int>(work_queue.size()));
          work = work_queue.front();
          work_queue.pop_front();
        }
        const fs::path final_out = output_store.PathFor(work);
        const std::string tmp_out = final_out.string() + ".tmp";
        // Putting the file to clang-tidy early in the command line so that
        // it is easy to find with `ps` or `top`.
        const std::string command = clang_tidy_ + " '" + work.first.string() +
                                    "'" + clang_tidy_args_ + "> '" + tmp_out +
                                    "' 2>/dev/null";
        const int r = system(command.c_str());
#ifdef WIFSIGNALED
        // NOLINTBEGIN
        if (WIFSIGNALED(r) &&
            (WTERMSIG(r) == SIGINT || WTERMSIG(r) == SIGQUIT)) {
          break;  // got Ctrl-C
        }
        // NOLINTEND
#endif
        RepairFilenameOccurences(tmp_out, tmp_out);
        fs::rename(tmp_out, final_out);  // atomic replacement
      }
    };

    std::vector<std::thread> workers;
    for (auto i = 0; i < kJobs; ++i) {
      workers.emplace_back(clang_tidy_runner);  // NOLINT
    }
    for (auto &t : workers) t.join();
    fprintf(stderr, "     \n");  // Clean out progress counter.
  }

 private:
  static fs::path GetCacheBaseDir() {
    if (const char *from_env = getenv("CACHE_DIR")) return fs::path{from_env};
    if (const char *home = getenv("HOME")) {
      if (auto cdir = fs::path(home) / ".cache/"; fs::exists(cdir)) return cdir;
    }
    return fs::path{getenv("TMPDIR") ?: "/tmp"};
  }

  static std::string AssembleArgs(int argc, char **argv) {
    std::string result = " --quiet";
    result.append(" '--config-file=").append(kClangConfigFile).append("'");
    for (const std::string_view arg : kExtraArgs) {
      result.append(" --extra-arg='").append(arg).append("'");
    }
    for (int i = 1; i < argc; ++i) {
      result.append(" '").append(argv[i]).append("'");
    }
    return result;
  }

  fs::path AssembleProjectCacheDir() const {
    const fs::path cache_dir = GetCacheBaseDir() / "clang-tidy";

    // Use major version as part of name of our configuration specific dir.
    auto version = GetCommandOutput(clang_tidy_ + " --version");
    std::smatch version_match;
    const std::string major_version =
        std::regex_search(version, version_match,
                          std::regex{"version ([0-9]+)"})
            ? version_match[1].str()
            : "UNKNOWN";
    // Make sure directory filename depends on .clang-tidy content.
    return cache_dir /
           fs::path(kProjectCachePrefix + "v" + major_version + "_" +
                    toHex(hash(version + clang_tidy_ + clang_tidy_args_) ^
                              hash(GetContent(kClangConfigFile)),
                          8));
  }

  // Fix filename paths found in logfiles that are not emitted relative to
  // project root in the log (bazel has its own)
  static void RepairFilenameOccurences(const fs::path &infile,
                                       const fs::path &outfile) {
    static const std::regex sFixPathsRe = []() {
      std::string canonicalize_expr = "(^|\\n)(";  // fix names at start of line
      auto root = GetCommandOutput("bazel info execution_root 2>/dev/null");
      if (!root.empty()) {
        root.pop_back();  // remove newline.
        canonicalize_expr += root + "/|";
      }
      canonicalize_expr += fs::current_path().string() + "/";  // $(pwd)/
      canonicalize_expr +=
          ")?(\\./)?";  // Some start with, or have a trailing ./
      return std::regex{canonicalize_expr};
    }();

    const auto in_content = GetContent(infile);
    std::fstream out_stream(outfile, std::ios::out);
    out_stream << std::regex_replace(in_content, sFixPathsRe, "$1");
  }

  const std::string clang_tidy_;
  const std::string clang_tidy_args_;
  fs::path project_cache_dir_;
};

class FileGatherer {
 public:
  FileGatherer(ContentAddressedStore &store, std::string_view search_dir)
      : store_(store), root_dir_(search_dir) {}

  // Find all the files we're interested in, and assemble a list of
  // paths that need refreshing.
  std::list<filepath_contenthash_t> BuildWorkList(file_time min_freshness) {
    // Gather all *.cc and *.h files; remember content hashes of includes.
    const std::regex exclude_re(kFileExcludeRe);
    std::map<std::string, hash_t> header_hashes;
    for (const auto &dir_entry : fs::recursive_directory_iterator(root_dir_)) {
      const fs::path &p = dir_entry.path().lexically_normal();
      if (!fs::is_regular_file(p)) continue;
      if (!kFileExcludeRe.empty() &&
          std::regex_search(p.string(), exclude_re)) {
        continue;
      }
      if (auto ext = p.extension(); ext == ".cc" || ext == ".h") {
        files_of_interest_.emplace_back(p, 0);  // <- hash to be filled later.
        if (ext == ".h") header_hashes[p.string()] = hash(GetContent(p));
      }
    }
    std::cerr << files_of_interest_.size() << " files of interest.\n";

    // Create content hash address. If any header a file depends on changes, we
    // want to reprocess. So we make the hash dependent on header content as
    // well.
    std::list<filepath_contenthash_t> work_queue;
    const std::regex inc_re("\"([0-9a-zA-Z_/-]+\\.h)\"");  // match include
    for (filepath_contenthash_t &f : files_of_interest_) {
      const auto content = GetContent(f.first);
      f.second = hash(content);
      for (ReIt it(content.begin(), content.end(), inc_re); it != ReIt();
           ++it) {
        const std::string &header_path = (*it)[1].str();
        f.second ^= header_hashes[header_path];
      }
      // Recreate if we don't have it yet or if it contains messages but is
      // older than WORKSPACE or compilation db. Maybe something got fixed.
      if (store_.NeedsRefresh(f, min_freshness)) {
        work_queue.emplace_back(f);
      }
    }
    return work_queue;
  }

  // Tally up findings for files of interest and assemble in one file.
  // (BuildWorkList() needs to be called first).
  std::map<std::string, int> CreateReport(const fs::path &project_dir,
                                          std::string_view symlink_to) {
    const fs::path tidy_outfile = project_dir / "tidy.out";
    // Assemble the separate outputs into a single file. Tally up per-check
    const std::regex check_re("(\\[[a-zA-Z.-]+\\])\n");
    std::map<std::string, int> checks_seen;
    std::ofstream tidy_collect(tidy_outfile);
    for (const filepath_contenthash_t &f : files_of_interest_) {
      const auto tidy = store_.GetContentFor(f);
      if (!tidy.empty()) tidy_collect << f.first.string() << ":\n" << tidy;
      for (ReIt it(tidy.begin(), tidy.end(), check_re); it != ReIt(); ++it) {
        checks_seen[(*it)[1].str()]++;
      }
    }

    std::error_code ignored_error;
    fs::remove(symlink_to, ignored_error);
    fs::create_symlink(tidy_outfile, symlink_to, ignored_error);
    return checks_seen;
  }

 private:
  ContentAddressedStore &store_;
  const std::string root_dir_;
  std::vector<filepath_contenthash_t> files_of_interest_;
};

int main(int argc, char *argv[]) {
  const std::string kTidySymlink = kProjectCachePrefix + "clang-tidy.out";

  // Test that key files exist and remember their last change.
  std::error_code ec;
  const auto workspace_ts = fs::last_write_time(kWorkspaceFile, ec);
  if (ec.value() != 0) {
    std::cerr << "Script needs to be executed in toplevel bazel project dir\n";
    return EXIT_FAILURE;
  }
  const auto compdb_ts = fs::last_write_time("compile_commands.json", ec);
  if (ec.value() != 0) {
    std::cerr << "No compilation db found. First, run make-compilation-db.sh\n";
    return EXIT_FAILURE;
  }
  const auto build_env_latest_change = std::max(workspace_ts, compdb_ts);

  ClangTidyRunner runner(argc, argv);
  ContentAddressedStore store(runner.project_cache_dir());
  std::cerr << "Cache dir " << runner.project_cache_dir() << "\n";

  FileGatherer cc_file_gatherer(store, kSearchDir);
  auto work_list = cc_file_gatherer.BuildWorkList(build_env_latest_change);
  runner.RunClangTidyOn(store, work_list);
  auto checks_seen =
      cc_file_gatherer.CreateReport(runner.project_cache_dir(), kTidySymlink);

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
