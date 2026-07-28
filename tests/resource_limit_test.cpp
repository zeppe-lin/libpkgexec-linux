// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later
#include "fixture.h"
#include "test.h"

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>

#include <signal.h>
#include <sys/resource.h>
#include <unistd.h>

namespace {

class temporary_directory final {
public:
  temporary_directory()
  {
    std::string pattern = "/tmp/pkgexec-linux-limits.XXXXXX";
    char* value = ::mkdtemp(pattern.data());
    if (!value) {
      throw std::runtime_error("mkdtemp failed");
    }
    path_ = value;
  }
  ~temporary_directory()
  {
    std::error_code ignored;
    std::filesystem::remove_all(path_, ignored);
  }
  const std::filesystem::path& path() const noexcept { return path_; }
private:
  std::filesystem::path path_;
};

bool has(const std::vector<pkgexec::execution_guarantee>& values,
         pkgexec::execution_guarantee value)
{
  return std::binary_search(values.begin(), values.end(), value);
}

std::string material(const std::optional<pkgexec::stream_capture>& capture)
{
  CHECK(capture.has_value());
  CHECK(capture->material().has_value());
  return *capture->material();
}

pkgexec::execution_result run(
    pkgexec_linux::host_supervisor_backend& backend,
    const pkgexec_linux::interpreter_binding& shell,
    const std::filesystem::path& workspace,
    const std::filesystem::path& probe,
    std::string arguments,
    pkgexec::resource_limits limits)
{
  auto request = fixture::request(
      shell, workspace, "exec '" + probe.string() + "' " + arguments,
      pkgexec::network_policy::allowed, pkgexec::resource_access::writable,
      true, pkgexec::stream_policy::capture_complete,
      pkgexec::stream_policy::capture_complete,
      static_cast<std::uint64_t>(::getuid()),
      pkgexec::cancellation_policy::disabled(), std::move(limits));
  return backend.execute(request, fixture::resources(request, workspace));
}

int test(const std::filesystem::path& probe)
{
  using namespace pkgexec;
  using namespace pkgexec_linux;

  temporary_directory temporary;
  const auto shell = interpreter_binding::inspect("/bin/sh");
  auto backend = host_supervisor_backend::make({shell});
  if (backend.report().state(capability_kind::address_space_limit) !=
          capability_state::available ||
      backend.report().state(capability_kind::file_size_limit) !=
          capability_state::available ||
      backend.report().state(capability_kind::open_files_limit) !=
          capability_state::available) {
    return 77;
  }

  const auto profile = backend.capabilities();
  const auto& guarantees = profile.guarantees();
  CHECK(has(guarantees, execution_guarantee::resource_limits));
  CHECK(has(guarantees, execution_guarantee::address_space_limit));
  CHECK(has(guarantees, execution_guarantee::file_size_limit));
  CHECK(has(guarantees, execution_guarantee::open_files_limit));
  CHECK(!has(guarantees, execution_guarantee::cpu_time_limit));
  CHECK(!has(guarantees, execution_guarantee::process_count_limit));

  constexpr std::uint64_t address_space = 256U * 1024U * 1024U;
  constexpr std::uint64_t file_size = 4096U;
  constexpr std::uint64_t open_files = 64U;
  auto exact = run(
      backend, shell, temporary.path(), probe, "show",
      resource_limits::make(std::nullopt, address_space, file_size,
                            open_files));
  CHECK(exact.status() == execution_status::succeeded);
  CHECK(material(exact.standard_output()) ==
        "as=268435456/268435456\n"
        "fsize=4096/4096\n"
        "nofile=64/64\n");
  CHECK(has(exact.established_guarantees(),
            execution_guarantee::resource_limits));
  CHECK(has(exact.established_guarantees(),
            execution_guarantee::address_space_limit));
  CHECK(has(exact.established_guarantees(),
            execution_guarantee::file_size_limit));
  CHECK(has(exact.established_guarantees(),
            execution_guarantee::open_files_limit));

  auto sealed = run(
      backend, shell, temporary.path(), probe, "raise nofile 65",
      resource_limits::make(std::nullopt, std::nullopt, std::nullopt, 64U));
  CHECK(sealed.status() == execution_status::succeeded);
  CHECK(material(sealed.standard_output()) ==
        "result=-1 errno=" + std::to_string(EPERM) + "\n");

  auto prlimit_observed = run(
      backend, shell, temporary.path(), probe, "prlimit-read nofile",
      resource_limits::make(std::nullopt, std::nullopt, std::nullopt, 64U));
  CHECK(prlimit_observed.status() == execution_status::succeeded);
  CHECK(material(prlimit_observed.standard_output()) ==
        "result=0 errno=0 value=64/64\n");

  auto prlimit_sealed = run(
      backend, shell, temporary.path(), probe, "prlimit-raise nofile 65",
      resource_limits::make(std::nullopt, std::nullopt, std::nullopt, 64U));
  CHECK(prlimit_sealed.status() == execution_status::succeeded);
  CHECK(material(prlimit_sealed.standard_output()) ==
        "result=-1 errno=" + std::to_string(EPERM) + "\n");

  auto descriptors = run(
      backend, shell, temporary.path(), probe, "open",
      resource_limits::make(std::nullopt, std::nullopt, std::nullopt, 16U));
  CHECK(descriptors.status() == execution_status::succeeded);
  const auto descriptor_output = material(descriptors.standard_output());
  CHECK(descriptor_output.find("errno=" + std::to_string(EMFILE)) !=
        std::string::npos);

  auto mapped = run(
      backend, shell, temporary.path(), probe,
      "mmap " + std::to_string(128U * 1024U * 1024U),
      resource_limits::make(std::nullopt, 64U * 1024U * 1024U));
  CHECK(mapped.status() == execution_status::succeeded);
  CHECK(material(mapped.standard_output()) ==
        "errno=" + std::to_string(ENOMEM) + "\n");

  const auto output_path = temporary.path() / "limited-output";
  auto written = run(
      backend, shell, temporary.path(), probe,
      "write '" + output_path.string() + "' 4096",
      resource_limits::make(std::nullopt, std::nullopt, 1024U));
  CHECK(written.status() == execution_status::failed);
  CHECK(written.failure() ==
        execution_failure_kind::program_terminated_by_signal);
  CHECK(written.termination()->kind() == process_termination_kind::signaled);
  CHECK(written.termination()->value() ==
        static_cast<std::uint32_t>(SIGXFSZ));
  CHECK(std::filesystem::file_size(output_path) == 1024U);
  CHECK(has(written.established_guarantees(),
            execution_guarantee::file_size_limit));

  auto cpu_request = fixture::request(
      shell, temporary.path(), "true", network_policy::allowed,
      resource_access::writable, true, stream_policy::capture_complete,
      stream_policy::capture_complete, static_cast<std::uint64_t>(::getuid()),
      cancellation_policy::disabled(), resource_limits::make(1000U));
  auto cpu = backend.execute(
      cpu_request, fixture::resources(cpu_request, temporary.path()));
  CHECK(cpu.failure() == execution_failure_kind::backend_unsupported);

  auto process_request = fixture::request(
      shell, temporary.path(), "true", network_policy::allowed,
      resource_access::writable, true, stream_policy::capture_complete,
      stream_policy::capture_complete, static_cast<std::uint64_t>(::getuid()),
      cancellation_policy::disabled(),
      resource_limits::make(std::nullopt, std::nullopt, std::nullopt,
                            std::nullopt, 4U));
  auto process = backend.execute(
      process_request, fixture::resources(process_request, temporary.path()));
  CHECK(process.failure() == execution_failure_kind::backend_unsupported);

  auto infinite = run(
      backend, shell, temporary.path(), probe, "show",
      resource_limits::make(
          std::nullopt, std::nullopt, std::nullopt,
          static_cast<std::uint64_t>(RLIM_INFINITY)));
  CHECK(infinite.start_state() == execution_start_state::not_started);
  CHECK(infinite.failure() == execution_failure_kind::resource_admission_failed);
  CHECK(infinite.diagnostic().find("open-files resource limit") !=
        std::string::npos);

  return 0;
}

} // namespace

int main(int argc, char** argv)
{
  if (argc != 2) {
    return 2;
  }
  try {
    return test(argv[1]);
  } catch (const std::exception& value) {
    std::fprintf(stderr, "%s\n", value.what());
    return 1;
  }
}
