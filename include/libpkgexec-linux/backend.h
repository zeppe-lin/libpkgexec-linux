// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

/*! \file backend.h
 *  \brief Linux host and isolated execution backends.
 */
#pragma once

#include <vector>

#include <libpkgexec/backend.h>
#include <libpkgexec-linux/capability.h>
#include <libpkgexec-linux/interpreter.h>

namespace pkgexec_linux {

/*! \brief Linux host supervisor for already-visible writable resources.
 *
 *  The backend does not create namespaces or mounts. It accepts only the
 *  current root view, current credentials, allowed networking, and writable
 *  resources already present at their logical paths. Exact address-space,
 *  file-size, and open-files limits are admitted when their realization probes
 *  succeed. Request-bound graceful-then-forced cancellation is admitted when
 *  the pidfd cancellation capability is available.
 */
class host_supervisor_backend final : public pkgexec::controlled_execution_backend {
public:
  [[nodiscard]] static host_supervisor_backend make(
      std::vector<interpreter_binding> interpreters);
  [[nodiscard]] const capability_report& report() const noexcept;
  [[nodiscard]] pkgexec::backend_capability_profile capabilities() const override;
protected:
  [[nodiscard]] pkgexec::execution_result execute_uncontrolled(
      const pkgexec::execution_request& request,
      const pkgexec::execution_resources& resources) override;
  [[nodiscard]] pkgexec::execution_result execute_controlled(
      const pkgexec::execution_request& request,
      const pkgexec::execution_resources& resources,
      const pkgexec::cancellation_token& cancellation) override;
private:
  host_supervisor_backend(capability_report report,
                          std::vector<interpreter_binding> interpreters);
  capability_report report_;
  std::vector<interpreter_binding> interpreters_;
};


/*! \brief Linux v0.5 backend with private filesystem and network views.
 *
 *  The backend realizes the exact supplied root view in a private mount
 *  namespace, attaches each admitted directory at its logical path, and
 *  enforces read-only or writable access at the mount layer. Allowed networking
 *  preserves the caller's network namespace. Denied and loopback-only policy
 *  create private network namespaces. The backend currently admits only the
 *  supervisor's numeric credentials. Exact address-space, file-size, and
 *  open-files limits are admitted when their realization probes succeed.
 *  Request-bound graceful-then-forced cancellation is admitted when pidfd
 *  cancellation is available.
 */
class isolated_backend final : public pkgexec::controlled_execution_backend {
public:
  [[nodiscard]] static isolated_backend make(
      std::vector<interpreter_binding> interpreters);
  [[nodiscard]] const capability_report& report() const noexcept;
  [[nodiscard]] pkgexec::backend_capability_profile capabilities() const override;
protected:
  [[nodiscard]] pkgexec::execution_result execute_uncontrolled(
      const pkgexec::execution_request& request,
      const pkgexec::execution_resources& resources) override;
  [[nodiscard]] pkgexec::execution_result execute_controlled(
      const pkgexec::execution_request& request,
      const pkgexec::execution_resources& resources,
      const pkgexec::cancellation_token& cancellation) override;
private:
  isolated_backend(capability_report report,
                   std::vector<interpreter_binding> interpreters);
  capability_report report_;
  std::vector<interpreter_binding> interpreters_;
};

} // namespace pkgexec_linux
