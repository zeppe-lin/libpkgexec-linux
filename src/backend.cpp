// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later
#include <libpkgexec-linux/backend.h>

#include <libpkgexec-linux/error.h>

#include "mount_isolation.h"
#include "network_isolation.h"
#include "process_control.h"
#include "support.h"

#include <algorithm>
#include <array>
#include <cerrno>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <optional>
#include <sstream>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include <fcntl.h>
#include <grp.h>
#include <poll.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

namespace pkgexec_linux {
namespace {

class unique_fd final {
public:
  unique_fd() noexcept = default;
  explicit unique_fd(int value) noexcept : value_(value) {}
  ~unique_fd() { reset(); }
  unique_fd(const unique_fd&) = delete;
  unique_fd& operator=(const unique_fd&) = delete;
  unique_fd(unique_fd&& other) noexcept : value_(other.release()) {}
  unique_fd& operator=(unique_fd&& other) noexcept
  {
    if (this != &other) {
      reset(other.release());
    }
    return *this;
  }
  [[nodiscard]] int get() const noexcept { return value_; }
  [[nodiscard]] explicit operator bool() const noexcept { return value_ >= 0; }
  int release() noexcept
  {
    const int value = value_;
    value_ = -1;
    return value;
  }
  void reset(int value = -1) noexcept
  {
    if (value_ >= 0) {
      ::close(value_);
    }
    value_ = value;
  }
private:
  int value_ = -1;
};

struct pipe_pair final {
  unique_fd read;
  unique_fd write;
};

pipe_pair make_pipe()
{
  int values[2] = {-1, -1};
  if (::pipe2(values, O_CLOEXEC) != 0) {
    throw error(error_code::invalid_value,
                detail::errno_message("pipe2", errno));
  }
  return {unique_fd(values[0]), unique_fd(values[1])};
}

void set_nonblocking(int fd)
{
  const int flags = ::fcntl(fd, F_GETFL, 0);
  if (flags < 0 || ::fcntl(fd, F_SETFL, flags | O_NONBLOCK) != 0) {
    throw error(error_code::invalid_value,
                detail::errno_message("fcntl", errno));
  }
}

std::string join_path(const std::vector<pkgexec::logical_path>& values)
{
  std::string result;
  for (const auto& value : values) {
    if (!result.empty()) {
      result.push_back(':');
    }
    result += value.string();
  }
  return result;
}

std::vector<std::string> make_environment(const pkgexec::environment_policy& policy)
{
  std::vector<std::string> values;
  values.reserve(8U + policy.additional_variables().size());
  values.push_back("PATH=" + join_path(policy.executable_search_path()));
  values.push_back("HOME=" + policy.home_directory().string());
  values.push_back("LANG=C.UTF-8");
  values.push_back("LC_ALL=C.UTF-8");
  values.push_back("TZ=UTC");
  values.push_back("TMPDIR=" + policy.temporary_directory().string());
  values.push_back("PKGEXEC_JOBS=" + std::to_string(policy.parallelism()));
  if (policy.source_date_epoch()) {
    values.push_back("SOURCE_DATE_EPOCH=" +
                     std::to_string(*policy.source_date_epoch()));
  }
  for (const auto& variable : policy.additional_variables()) {
    values.push_back(variable.name() + "=" + variable.value());
  }
  return values;
}

std::vector<char*> pointers(std::vector<std::string>& values)
{
  std::vector<char*> result;
  result.reserve(values.size() + 1U);
  for (auto& value : values) {
    result.push_back(value.data());
  }
  result.push_back(nullptr);
  return result;
}

const interpreter_binding* find_interpreter(
    const std::vector<interpreter_binding>& values,
    const pkgexec::interpreter_identity& identity) noexcept
{
  const auto found = std::lower_bound(
      values.begin(), values.end(), identity,
      [](const interpreter_binding& binding,
         const pkgexec::interpreter_identity& key) {
        return binding.identity() < key;
      });
  return found != values.end() && found->identity() == identity ? &*found
                                                                : nullptr;
}

std::string validate_host_request_shape(
    const pkgexec::execution_request& request,
    const pkgexec::execution_resources& resources)
{
  if (resources.root_view_path() != std::filesystem::path("/")) {
    return "host supervisor admits only the current / root view";
  }
  if (request.environment().network() != pkgexec::network_policy::allowed) {
    return "host supervisor does not isolate networking";
  }
  if (!request.limits().empty()) {
    return "host supervisor does not classify resource limits";
  }
  if (!request.credentials().no_new_privileges()) {
    return "host supervisor requires no-new-privileges containment";
  }
  if (request.credentials().user_id() != static_cast<std::uint64_t>(::getuid()) ||
      request.credentials().group_id() != static_cast<std::uint64_t>(::getgid()) ||
      request.credentials().supplementary_groups() != detail::current_groups()) {
    return "host supervisor admits only its current credentials";
  }
  for (const auto& binding : request.resources().bindings()) {
    if (binding.access() != pkgexec::resource_access::writable) {
      return "host supervisor does not realize read-only resources";
    }
    const auto& materialization = resources.materialization(binding.resource());
    if (!materialization.host_path().is_absolute() ||
        materialization.host_path().lexically_normal() !=
            std::filesystem::path(binding.mount_point().string()).lexically_normal()) {
      return "resource is not already present at its logical path";
    }
    if (detail::path_has_symlink_component(materialization.host_path())) {
      return "resource path contains a symlink component";
    }
    struct stat info {};
    if (::stat(materialization.host_path().c_str(), &info) != 0 ||
        !S_ISDIR(info.st_mode)) {
      return "resource path is not an existing directory";
    }
    if (::access(materialization.host_path().c_str(), W_OK | X_OK) != 0) {
      return "resource path is not writable by the requested credentials";
    }
  }
  const auto& working = request.resources().binding(
      request.resources().working_directory());
  const auto& materialization = resources.materialization(working.resource());
  if (::access(materialization.host_path().c_str(), X_OK) != 0) {
    return "working directory is not searchable";
  }
  return {};
}

std::string validate_isolated_request_shape(
    const pkgexec::execution_request& request)
{
  if (!request.limits().empty()) {
    return "isolated filesystem backend does not classify resource limits";
  }
  if (!request.credentials().no_new_privileges()) {
    return "isolated filesystem backend requires no-new-privileges containment";
  }
  if (request.credentials().user_id() != static_cast<std::uint64_t>(::getuid()) ||
      request.credentials().group_id() != static_cast<std::uint64_t>(::getgid()) ||
      request.credentials().supplementary_groups() != detail::current_groups()) {
    return "isolated filesystem backend admits only its current credentials";
  }
  return {};
}

enum class child_stage : std::uint32_t {
  process_group,
  standard_streams,
  mount_isolation,
  network_isolation,
  working_directory,
  capability_drop,
  containment,
  execute,
};

struct child_error final {
  child_stage stage;
  std::int32_t value;
  std::uint32_t detail;
};

void report_child_error(int fd, child_stage stage, int value,
                        std::uint32_t detail = 0) noexcept
{
  const child_error failure{stage, value, detail};
  const auto* bytes = reinterpret_cast<const unsigned char*>(&failure);
  std::size_t offset = 0;
  while (offset < sizeof(failure)) {
    const ssize_t count = ::write(fd, bytes + offset, sizeof(failure) - offset);
    if (count > 0) {
      offset += static_cast<std::size_t>(count);
      continue;
    }
    if (count < 0 && errno == EINTR) {
      continue;
    }
    break;
  }
}

struct child_configuration final {
  pkgexec::stdin_policy standard_input;
  pkgexec::stream_policy standard_output;
  pkgexec::stream_policy standard_error;
  std::uint32_t file_creation_mask;
  const char* working_directory;
  const detail::isolated_admission* isolation;
  pkgexec::network_policy network;
  int interpreter_fd;
  char* const* arguments;
  char* const* environment;
};

[[noreturn]] void child_main(
    const child_configuration& configuration,
    pipe_pair& standard_output,
    pipe_pair& standard_error,
    pipe_pair& control,
    pipe_pair& ready,
    pipe_pair& start)
{
  const int control_fd = control.write.get();
  if (::setsid() < 0) {
    report_child_error(control_fd, child_stage::process_group, errno);
    _exit(125);
  }

  auto redirect = [&](int target, pipe_pair& pipe,
                      pkgexec::stream_policy policy) -> bool {
    if (policy == pkgexec::stream_policy::capture_complete) {
      return ::dup2(pipe.write.get(), target) >= 0;
    }
    const int null_fd = ::open("/dev/null", O_WRONLY | O_CLOEXEC);
    if (null_fd < 0) {
      return false;
    }
    const bool okay = ::dup2(null_fd, target) >= 0;
    ::close(null_fd);
    return okay;
  };

  if (configuration.standard_input == pkgexec::stdin_policy::closed) {
    ::close(STDIN_FILENO);
  } else {
    const int null_fd = ::open("/dev/null", O_RDONLY | O_CLOEXEC);
    if (null_fd < 0 || ::dup2(null_fd, STDIN_FILENO) < 0) {
      const int saved = errno;
      if (null_fd >= 0) {
        ::close(null_fd);
      }
      report_child_error(control_fd, child_stage::standard_streams, saved);
      _exit(125);
    }
    ::close(null_fd);
  }
  if (!redirect(STDOUT_FILENO, standard_output,
                configuration.standard_output) ||
      !redirect(STDERR_FILENO, standard_error,
                configuration.standard_error)) {
    report_child_error(control_fd, child_stage::standard_streams, errno);
    _exit(125);
  }

  if (configuration.isolation) {
    detail::mount_setup_failure failure{};
    if (!detail::setup_isolated_filesystem(*configuration.isolation, failure)) {
      report_child_error(control_fd, child_stage::mount_isolation,
                         failure.error,
                         static_cast<std::uint32_t>(failure.stage));
      _exit(125);
    }
    detail::network_setup_failure network_failure{};
    if (!detail::setup_network_policy(configuration.network, network_failure)) {
      report_child_error(control_fd, child_stage::network_isolation,
                         network_failure.error,
                         static_cast<std::uint32_t>(network_failure.stage));
      _exit(125);
    }
  }

  ::umask(static_cast<mode_t>(configuration.file_creation_mask));
  if (::chdir(configuration.working_directory) != 0) {
    report_child_error(control_fd, child_stage::working_directory, errno);
    _exit(125);
  }
  if (configuration.isolation && !detail::drop_process_capabilities()) {
    report_child_error(control_fd, child_stage::capability_drop, errno);
    _exit(125);
  }
  if (!detail::install_process_group_containment()) {
    report_child_error(control_fd, child_stage::containment, errno);
    _exit(125);
  }

  const unsigned char ready_byte = 1;
  ssize_t ready_count = -1;
  do {
    ready_count = ::write(ready.write.get(), &ready_byte, 1);
  } while (ready_count < 0 && errno == EINTR);
  ready.write.reset();
  if (ready_count != 1) {
    _exit(125);
  }

  unsigned char start_byte = 0;
  ssize_t start_count = -1;
  do {
    start_count = ::read(start.read.get(), &start_byte, 1);
  } while (start_count < 0 && errno == EINTR);
  start.read.reset();
  if (start_count != 1 || start_byte != 1) {
    _exit(125);
  }

  detail::close_fds_except(control_fd, configuration.interpreter_fd);
#ifdef __NR_execveat
  (void)::syscall(__NR_execveat, configuration.interpreter_fd, "",
                  configuration.arguments, configuration.environment,
                  AT_EMPTY_PATH);
#else
  errno = ENOSYS;
#endif
  report_child_error(control_fd, child_stage::execute, errno);
  _exit(127);
}

void drain_fd(unique_fd& fd, std::string& output, bool& failed)
{
  std::array<char, 16 * 1024> buffer{};
  while (fd) {
    const ssize_t count = ::read(fd.get(), buffer.data(), buffer.size());
    if (count > 0) {
      output.append(buffer.data(), static_cast<std::size_t>(count));
      continue;
    }
    if (count == 0) {
      fd.reset();
      return;
    }
    if (errno == EINTR) {
      continue;
    }
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
      return;
    }
    failed = true;
    fd.reset();
  }
}

bool cleanup_group(pid_t group) noexcept
{
  if (::kill(-group, 0) != 0 && errno == ESRCH) {
    return true;
  }
  (void)::kill(-group, SIGTERM);
  for (int i = 0; i < 20; ++i) {
    if (::kill(-group, 0) != 0 && errno == ESRCH) {
      return true;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  (void)::kill(-group, SIGKILL);
  for (int i = 0; i < 100; ++i) {
    if (::kill(-group, 0) != 0 && errno == ESRCH) {
      return true;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  return false;
}

pkgexec::execution_failure_kind child_failure_kind(const child_error& failure)
{
  switch (failure.stage) {
    case child_stage::working_directory:
      return pkgexec::execution_failure_kind::resource_admission_failed;
    case child_stage::mount_isolation:
    case child_stage::network_isolation:
    case child_stage::capability_drop:
    case child_stage::containment:
    case child_stage::process_group:
      return pkgexec::execution_failure_kind::isolation_setup_failed;
    case child_stage::standard_streams:
      return pkgexec::execution_failure_kind::process_start_failed;
    case child_stage::execute:
      return failure.value == ENOENT
          ? pkgexec::execution_failure_kind::interpreter_unavailable
          : pkgexec::execution_failure_kind::process_start_failed;
  }
  return pkgexec::execution_failure_kind::process_start_failed;
}

std::string child_failure_diagnostic(const child_error& failure)
{
  const char* operation = "child setup";
  switch (failure.stage) {
    case child_stage::process_group: operation = "setsid"; break;
    case child_stage::standard_streams: operation = "standard stream setup"; break;
    case child_stage::mount_isolation:
      operation = detail::mount_stage_name(
          static_cast<detail::mount_setup_stage>(failure.detail)).data();
      break;
    case child_stage::network_isolation:
      operation = detail::network_stage_name(
          static_cast<detail::network_setup_stage>(failure.detail)).data();
      break;
    case child_stage::working_directory: operation = "chdir"; break;
    case child_stage::capability_drop: operation = "capability drop"; break;
    case child_stage::containment: operation = "seccomp containment"; break;
    case child_stage::execute: operation = "execve"; break;
  }
  return detail::errno_message(operation, failure.value);
}

std::vector<pkgexec::execution_guarantee> without_guarantee(
    const std::vector<pkgexec::execution_guarantee>& input,
    pkgexec::execution_guarantee removed)
{
  std::vector<pkgexec::execution_guarantee> result;
  std::copy_if(input.begin(), input.end(), std::back_inserter(result),
               [removed](pkgexec::execution_guarantee value) {
                 return value != removed;
               });
  return result;
}


bool observe_child_exit(pid_t child, int pidfd, siginfo_t& information) noexcept
{
  information = {};
  const idtype_t type = pidfd >= 0 ? static_cast<idtype_t>(3) : P_PID;
  const id_t identifier = pidfd >= 0 ? static_cast<id_t>(pidfd)
                                     : static_cast<id_t>(child);
  for (;;) {
    if (::waitid(type, identifier, &information,
                 WEXITED | WNOHANG | WNOWAIT) == 0) {
      return information.si_pid != 0;
    }
    if (errno != EINTR) {
      return false;
    }
  }
}

pkgexec::process_termination termination_from_siginfo(
    const siginfo_t& information)
{
  if (information.si_code == CLD_EXITED) {
    return pkgexec::process_termination::exited(
        static_cast<std::uint32_t>(information.si_status));
  }
  return pkgexec::process_termination::signaled(
      static_cast<std::uint32_t>(information.si_status));
}

bool write_start(unique_fd& fd) noexcept
{
  const unsigned char value = 1;
  ssize_t count = -1;
  do {
    count = ::write(fd.get(), &value, 1);
  } while (count < 0 && errno == EINTR);
  fd.reset();
  return count == 1;
}

} // namespace

host_supervisor_backend::host_supervisor_backend(
    capability_report report, std::vector<interpreter_binding> interpreters)
    : report_(std::move(report)), interpreters_(std::move(interpreters))
{
}

host_supervisor_backend host_supervisor_backend::make(
    std::vector<interpreter_binding> interpreters)
{
  if (interpreters.empty()) {
    throw error(error_code::invalid_value,
                "at least one exact interpreter binding is required");
  }
  std::sort(interpreters.begin(), interpreters.end());
  for (std::size_t i = 1; i < interpreters.size(); ++i) {
    if (interpreters[i - 1].identity() == interpreters[i].identity()) {
      throw error(error_code::duplicate_interpreter,
                  "duplicate interpreter identity");
    }
  }
  return host_supervisor_backend(capability_report::probe(),
                                 std::move(interpreters));
}

const capability_report& host_supervisor_backend::report() const noexcept
{ return report_; }

pkgexec::backend_capability_profile host_supervisor_backend::capabilities() const
{ return report_.profile(); }

pkgexec::execution_result execute_backend(
    const capability_report& report,
    const std::vector<interpreter_binding>& interpreters,
    const pkgexec::execution_request& request,
    const pkgexec::execution_resources& resources,
    bool isolated,
    const pkgexec::cancellation_token* cancellation)
{
  const auto profile = report.profile();
  if (!profile.supports(request)) {
    return pkgexec::execution_result::failed_before_start(
        request, profile, pkgexec::execution_failure_kind::backend_unsupported,
        {}, "Linux backend cannot establish all requested guarantees");
  }
  if (cancellation && cancellation->cancellation_requested()) {
    return pkgexec::execution_result::cancelled_before_start(
        request, profile, *cancellation,
        {pkgexec::execution_guarantee::cancellation},
        "cancellation was requested before resource admission");
  }

  std::optional<detail::isolated_admission> isolation;
  try {
    const auto shape_error = isolated
        ? validate_isolated_request_shape(request)
        : validate_host_request_shape(request, resources);
    if (!shape_error.empty()) {
      return pkgexec::execution_result::failed_before_start(
          request, profile, pkgexec::execution_failure_kind::request_rejected,
          {}, shape_error);
    }
    if (isolated) {
      isolation.emplace(detail::admit_isolated_resources(request, resources));
    }
  } catch (const std::exception& value) {
    return pkgexec::execution_result::failed_before_start(
        request, profile,
        pkgexec::execution_failure_kind::resource_admission_failed,
        {}, value.what());
  }
  if (cancellation && cancellation->cancellation_requested()) {
    return pkgexec::execution_result::cancelled_before_start(
        request, profile, *cancellation,
        {pkgexec::execution_guarantee::cancellation},
        "cancellation was requested before interpreter admission");
  }

  const auto* configured = find_interpreter(interpreters, request.interpreter());
  if (!configured) {
    return pkgexec::execution_result::failed_before_start(
        request, profile,
        pkgexec::execution_failure_kind::interpreter_unavailable,
        {}, "no exact interpreter binding matches the request");
  }
  unique_fd interpreter_fd(::open(configured->executable().c_str(),
                                  O_RDONLY | O_CLOEXEC | O_NOFOLLOW));
  if (!interpreter_fd) {
    return pkgexec::execution_result::failed_before_start(
        request, profile,
        pkgexec::execution_failure_kind::interpreter_unavailable,
        {}, detail::errno_message("open interpreter", errno));
  }
  try {
    struct stat interpreter_info {};
    if (::fstat(interpreter_fd.get(), &interpreter_info) != 0 ||
        !S_ISREG(interpreter_info.st_mode)) {
      return pkgexec::execution_result::failed_before_start(
          request, profile,
          pkgexec::execution_failure_kind::interpreter_unavailable,
          {}, "interpreter descriptor is not a regular file");
    }
    const auto observed_digest = detail::digest_fd(interpreter_fd.get());
    if (observed_digest != configured->content_digest() ||
        detail::interpreter_identity(observed_digest) != configured->identity()) {
      return pkgexec::execution_result::failed_before_start(
          request, profile,
          pkgexec::execution_failure_kind::interpreter_unavailable,
          {}, "interpreter bytes changed after admission");
    }
  } catch (const std::exception& value) {
    return pkgexec::execution_result::failed_before_start(
        request, profile,
        pkgexec::execution_failure_kind::interpreter_unavailable,
        {}, value.what());
  }
  if (cancellation && cancellation->cancellation_requested()) {
    return pkgexec::execution_result::cancelled_before_start(
        request, profile, *cancellation,
        {pkgexec::execution_guarantee::cancellation},
        "cancellation was requested before process creation");
  }

  pipe_pair standard_output;
  pipe_pair standard_error;
  pipe_pair control;
  pipe_pair ready;
  pipe_pair start;
  try {
    standard_output = make_pipe();
    standard_error = make_pipe();
    control = make_pipe();
    ready = make_pipe();
    start = make_pipe();
    set_nonblocking(standard_output.read.get());
    set_nonblocking(standard_error.read.get());
    set_nonblocking(control.read.get());
    set_nonblocking(ready.read.get());
  } catch (const std::exception& value) {
    return pkgexec::execution_result::failed_before_start(
        request, profile, pkgexec::execution_failure_kind::process_start_failed,
        {}, value.what());
  }

  auto environment = make_environment(request.environment());
  auto environment_pointers = pointers(environment);
  const auto& working_binding = request.resources().binding(
      request.resources().working_directory());
  const std::string working_directory = isolated
      ? working_binding.mount_point().string()
      : resources.materialization(working_binding.resource()).host_path().string();
  std::string option = "-c";
  std::string material = request.program().material();
  std::string argument_zero = "pkgexec";
  std::array<char*, 5> arguments{
      const_cast<char*>(configured->executable().c_str()),
      option.data(), material.data(), argument_zero.data(), nullptr,
  };
  const child_configuration child_configuration_value{
      request.environment().standard_input(),
      request.environment().standard_output(),
      request.environment().standard_error(),
      request.environment().file_creation_mask(),
      working_directory.c_str(), isolation ? &*isolation : nullptr,
      request.environment().network(), interpreter_fd.get(),
      arguments.data(), environment_pointers.data(),
  };

  const pid_t child = ::fork();
  if (child < 0) {
    return pkgexec::execution_result::failed_before_start(
        request, profile, pkgexec::execution_failure_kind::process_start_failed,
        {}, detail::errno_message("fork", errno));
  }
  if (child == 0) {
    standard_output.read.reset();
    standard_error.read.reset();
    control.read.reset();
    ready.read.reset();
    start.write.reset();
    child_main(child_configuration_value, standard_output, standard_error,
               control, ready, start);
  }

  standard_output.write.reset();
  standard_error.write.reset();
  control.write.reset();
  ready.write.reset();
  start.read.reset();
  interpreter_fd.reset();
  unique_fd pidfd;
  if (cancellation) {
    pidfd.reset(detail::open_pidfd(child));
    if (!pidfd) {
      const int saved = errno;
      start.write.reset();
      (void)::kill(child, SIGKILL);
      while (::waitpid(child, nullptr, 0) < 0 && errno == EINTR) {
      }
      const bool mount_cleaned = !isolation || isolation->verify_parent_cleanup();
      std::string diagnostic = detail::errno_message("pidfd_open", saved);
      if (!mount_cleaned) {
        diagnostic += "; parent isolation scratch cleanup failed";
      }
      return pkgexec::execution_result::failed_before_start(
          request, profile,
          pkgexec::execution_failure_kind::process_start_failed,
          {}, std::move(diagnostic));
    }
  }

  std::string output_material;
  std::string error_material;
  std::array<unsigned char, sizeof(child_error)> control_bytes{};
  std::size_t control_size = 0;
  bool capture_failed = false;
  bool ready_seen = false;
  bool start_released = false;
  bool exec_confirmed = false;
  bool child_exited = false;
  bool child_reaped = false;
  siginfo_t child_information{};

  bool cancellation_observed = false;
  bool cancellation_before_start = false;
  bool cancellation_signal_attempted = false;
  bool cancellation_signal_delivered = false;
  bool forced_signal_sent = false;
  auto cancellation_deadline = std::chrono::steady_clock::time_point::max();

  bool cleanup_started = false;
  bool cleanup_complete = false;
  bool cleanup_failed = false;
  auto cleanup_deadline = std::chrono::steady_clock::time_point::max();

  const auto observe_cancellation = [&]() {
    if (!cancellation || cancellation_observed || child_exited ||
        !cancellation->cancellation_requested()) {
      return;
    }
    cancellation_observed = true;
    cancellation_before_start = !start_released;
    if (cancellation_before_start) {
      start.write.reset();
    }
  };

  const auto send_cancellation_signal = [&]() {
    if (!cancellation_observed || cancellation_signal_attempted ||
        (start_released && !exec_confirmed)) {
      return;
    }
    const auto signal_result = detail::signal_process_group_members(
        child, child, pidfd.get(), SIGTERM);
    cancellation_signal_attempted = true;
    cancellation_signal_delivered = signal_result.leader_signaled;
    if (!signal_result.complete) {
      cleanup_failed = true;
    }
    cancellation_deadline = std::chrono::steady_clock::now() +
        std::chrono::milliseconds(
            *request.cancellation().grace_period_milliseconds());
  };

  while (!child_reaped || standard_output.read || standard_error.read ||
         control.read || ready.read) {
    if (!child_exited) {
      child_exited = observe_child_exit(child, pidfd.get(), child_information);
    }
    observe_cancellation();

    if (ready_seen && !start_released && !cancellation_observed) {
      observe_cancellation();
      if (!cancellation_observed) {
        if (!write_start(start.write)) {
          start.write.reset();
        } else {
          start_released = true;
        }
      }
    }
    send_cancellation_signal();

    const auto now = std::chrono::steady_clock::now();
    if (cancellation_signal_attempted && !forced_signal_sent &&
        now >= cancellation_deadline && !cleanup_complete) {
      const auto signal_result = detail::signal_process_group_members(
          child, child, pidfd.get(), SIGKILL);
      if (!signal_result.complete) {
        cleanup_failed = true;
      }
      forced_signal_sent = true;
      cleanup_deadline = now + std::chrono::seconds(1);
    }

    if (child_exited && cancellation && !cleanup_started) {
      cleanup_started = true;
      if (!cancellation_signal_attempted) {
        const auto signal_result = detail::signal_process_group_members(
            child, child, pidfd.get(), SIGTERM);
        if (!signal_result.complete) {
          cleanup_failed = true;
        }
        cleanup_deadline = now + std::chrono::milliseconds(200);
      } else if (!forced_signal_sent) {
        cleanup_deadline = cancellation_deadline;
      }
    }

    if (child_exited && cancellation && cleanup_started && !cleanup_complete) {
      if (forced_signal_sent) {
        const auto signal_result = detail::signal_process_group_members(
            child, child, pidfd.get(), SIGKILL);
        if (!signal_result.complete) {
          cleanup_failed = true;
        }
      }
      if (detail::wait_process_group_members_gone(
              child, child, std::chrono::milliseconds(0))) {
        cleanup_complete = true;
      } else if (now >= cleanup_deadline) {
        if (!forced_signal_sent) {
          const auto signal_result = detail::signal_process_group_members(
              child, child, pidfd.get(), SIGKILL);
          if (!signal_result.complete) {
            cleanup_failed = true;
          }
          forced_signal_sent = true;
          cleanup_deadline = now + std::chrono::seconds(1);
        } else {
          const auto signal_result = detail::signal_process_group_members(
              child, child, pidfd.get(), SIGKILL);
          if (!signal_result.complete) {
            cleanup_failed = true;
          }
          cleanup_failed = true;
          cleanup_complete = true;
        }
      }
    }

    if (child_exited && cancellation && cleanup_complete && !child_reaped) {
      int ignored = 0;
      while (::waitpid(child, &ignored, 0) < 0 && errno == EINTR) {
      }
      child_reaped = true;
    }
    if (child_exited && !cancellation && !child_reaped) {
      int ignored = 0;
      while (::waitpid(child, &ignored, 0) < 0 && errno == EINTR) {
      }
      child_reaped = true;
      cleanup_complete = cleanup_group(child);
      cleanup_failed = !cleanup_complete;
    }

    std::array<pollfd, 5> descriptors{{
        {standard_output.read.get(), POLLIN | POLLHUP | POLLERR, 0},
        {standard_error.read.get(), POLLIN | POLLHUP | POLLERR, 0},
        {control.read.get(), POLLIN | POLLHUP | POLLERR, 0},
        {ready.read.get(), POLLIN | POLLHUP | POLLERR, 0},
        {pidfd.get(), POLLIN | POLLHUP | POLLERR, 0},
    }};
    (void)::poll(descriptors.data(), descriptors.size(), 10);
    drain_fd(standard_output.read, output_material, capture_failed);
    drain_fd(standard_error.read, error_material, capture_failed);

    while (control.read) {
      const ssize_t count = ::read(
          control.read.get(), control_bytes.data() + control_size,
          control_bytes.size() - control_size);
      if (count > 0) {
        control_size += static_cast<std::size_t>(count);
        if (control_size == control_bytes.size()) {
          control.read.reset();
        }
        continue;
      }
      if (count == 0) {
        control.read.reset();
        if (control_size == 0U && start_released) {
          exec_confirmed = true;
        }
      } else if (errno == EINTR) {
        continue;
      } else if (errno != EAGAIN && errno != EWOULDBLOCK) {
        control.read.reset();
      }
      break;
    }

    while (ready.read) {
      unsigned char byte = 0;
      const ssize_t count = ::read(ready.read.get(), &byte, 1);
      if (count > 0) {
        if (byte == 1) {
          ready_seen = true;
        }
        continue;
      }
      if (count == 0) {
        ready.read.reset();
      } else if (errno == EINTR) {
        continue;
      } else if (errno != EAGAIN && errno != EWOULDBLOCK) {
        ready.read.reset();
      }
      break;
    }

    if (child_reaped && !standard_output.read && !standard_error.read &&
        !control.read && !ready.read) {
      break;
    }
  }

  const bool mount_cleaned = !isolation || isolation->verify_parent_cleanup();
  const bool cleanup_verified = cleanup_complete && !cleanup_failed &&
                                mount_cleaned;

  if (control_size != 0U) {
    child_error failure{};
    if (control_size == sizeof(failure)) {
      std::memcpy(&failure, control_bytes.data(), sizeof(failure));
      std::string diagnostic = child_failure_diagnostic(failure);
      if (!mount_cleaned) {
        diagnostic += "; parent isolation scratch cleanup failed";
      }
      return pkgexec::execution_result::failed_before_start(
          request, profile, child_failure_kind(failure), {},
          std::move(diagnostic));
    }
    return pkgexec::execution_result::failed_before_start(
        request, profile, pkgexec::execution_failure_kind::process_start_failed,
        {}, "truncated child setup failure record");
  }

  if (cancellation_observed && cancellation_before_start) {
    if (!cleanup_verified) {
      return pkgexec::execution_result::failed_before_start(
          request, profile,
          pkgexec::execution_failure_kind::isolation_setup_failed,
          {}, "pre-start cancellation cleanup could not be verified");
    }
    return pkgexec::execution_result::cancelled_before_start(
        request, profile, *cancellation,
        {pkgexec::execution_guarantee::cancellation},
        "cancellation was requested before the final program-start gate");
  }
  if (!exec_confirmed) {
    return pkgexec::execution_result::failed_before_start(
        request, profile,
        cleanup_verified
            ? pkgexec::execution_failure_kind::process_start_failed
            : pkgexec::execution_failure_kind::isolation_setup_failed,
        {}, cleanup_verified
            ? "child terminated before descriptor execution was confirmed"
            : "pre-start termination cleanup could not be verified");
  }

  std::optional<pkgexec::stream_capture> output_capture;
  std::optional<pkgexec::stream_capture> error_capture;
  if (request.environment().standard_output() ==
      pkgexec::stream_policy::capture_complete && !capture_failed) {
    output_capture = pkgexec::stream_capture::retained(std::move(output_material));
  }
  if (request.environment().standard_error() ==
      pkgexec::stream_policy::capture_complete && !capture_failed) {
    error_capture = pkgexec::stream_capture::retained(std::move(error_material));
  }

  auto established = request.required_guarantees();
  if (capture_failed) {
    established = without_guarantee(
        established, pkgexec::execution_guarantee::complete_stdout_capture);
    established = without_guarantee(
        established, pkgexec::execution_guarantee::complete_stderr_capture);
  }
  if (!cleanup_verified) {
    established = without_guarantee(
        established, pkgexec::execution_guarantee::cleanup_verified);
  }

  const auto observed_termination = termination_from_siginfo(child_information);
  if (capture_failed) {
    return pkgexec::execution_result::failed_after_start(
        request, profile, configured->identity(), observed_termination,
        std::move(output_capture), std::move(error_capture),
        std::move(established),
        cleanup_verified ? pkgexec::cleanup_outcome::verified
                         : pkgexec::cleanup_outcome::failed,
        pkgexec::execution_failure_kind::log_capture_failed,
        "complete stream capture failed");
  }
  if (!cleanup_verified) {
    return pkgexec::execution_result::failed_after_start(
        request, profile, configured->identity(), observed_termination,
        std::move(output_capture), std::move(error_capture),
        std::move(established), pkgexec::cleanup_outcome::failed,
        pkgexec::execution_failure_kind::cleanup_failed,
        "process-group or mount cleanup could not be verified");
  }
  if (cancellation_signal_delivered) {
    return pkgexec::execution_result::cancelled_after_start(
        request, profile, *cancellation, configured->identity(),
        std::move(output_capture), std::move(error_capture),
        std::move(established), pkgexec::cleanup_outcome::verified,
        forced_signal_sent
            ? "cancellation required forced process-group termination"
            : "cancellation completed during the graceful period");
  }
  if (child_information.si_code == CLD_EXITED &&
      child_information.si_status == 0) {
    return pkgexec::execution_result::succeeded(
        request, profile, configured->identity(), std::move(output_capture),
        std::move(error_capture), std::move(established));
  }
  if (child_information.si_code == CLD_EXITED) {
    return pkgexec::execution_result::failed_after_start(
        request, profile, configured->identity(), observed_termination,
        std::move(output_capture), std::move(error_capture),
        std::move(established), pkgexec::cleanup_outcome::verified,
        pkgexec::execution_failure_kind::program_exited_nonzero);
  }
  return pkgexec::execution_result::failed_after_start(
      request, profile, configured->identity(), observed_termination,
      std::move(output_capture), std::move(error_capture),
      std::move(established), pkgexec::cleanup_outcome::verified,
      pkgexec::execution_failure_kind::program_terminated_by_signal);
}

pkgexec::execution_result host_supervisor_backend::execute_uncontrolled(
    const pkgexec::execution_request& request,
    const pkgexec::execution_resources& resources)
{
  return execute_backend(report_, interpreters_, request, resources, false,
                         nullptr);
}

pkgexec::execution_result host_supervisor_backend::execute_controlled(
    const pkgexec::execution_request& request,
    const pkgexec::execution_resources& resources,
    const pkgexec::cancellation_token& cancellation)
{
  return execute_backend(report_, interpreters_, request, resources, false,
                         &cancellation);
}

isolated_backend::isolated_backend(
    capability_report report, std::vector<interpreter_binding> interpreters)
    : report_(std::move(report)), interpreters_(std::move(interpreters))
{
}

isolated_backend isolated_backend::make(
    std::vector<interpreter_binding> interpreters)
{
  if (interpreters.empty()) {
    throw error(error_code::invalid_value,
                "at least one exact interpreter binding is required");
  }
  std::sort(interpreters.begin(), interpreters.end());
  for (std::size_t i = 1; i < interpreters.size(); ++i) {
    if (interpreters[i - 1].identity() == interpreters[i].identity()) {
      throw error(error_code::duplicate_interpreter,
                  "duplicate interpreter identity");
    }
  }
  return isolated_backend(capability_report::probe_isolated(),
                          std::move(interpreters));
}

const capability_report& isolated_backend::report() const noexcept
{ return report_; }

pkgexec::backend_capability_profile isolated_backend::capabilities() const
{ return report_.profile(); }

pkgexec::execution_result isolated_backend::execute_uncontrolled(
    const pkgexec::execution_request& request,
    const pkgexec::execution_resources& resources)
{
  return execute_backend(report_, interpreters_, request, resources, true,
                         nullptr);
}

pkgexec::execution_result isolated_backend::execute_controlled(
    const pkgexec::execution_request& request,
    const pkgexec::execution_resources& resources,
    const pkgexec::cancellation_token& cancellation)
{
  return execute_backend(report_, interpreters_, request, resources, true,
                         &cancellation);
}

} // namespace pkgexec_linux
