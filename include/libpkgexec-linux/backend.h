// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

/*! \file backend.h
 *  \brief Restricted Linux host-supervisor backend.
 */
#pragma once

#include <vector>

#include <libpkgexec/backend.h>
#include <libpkgexec-linux/capability.h>
#include <libpkgexec-linux/interpreter.h>

namespace pkgexec_linux {

/*! \brief Linux v0 host supervisor for already-visible writable resources.
 *
 *  The backend does not create namespaces or mounts. It accepts only the
 *  current root view, current credentials, allowed networking, writable
 *  resources already present at their logical paths, empty resource limits,
 *  and disabled cancellation.
 */
class host_supervisor_backend final : public pkgexec::execution_backend {
public:
  [[nodiscard]] static host_supervisor_backend make(
      std::vector<interpreter_binding> interpreters);
  [[nodiscard]] const capability_report& report() const noexcept;
  [[nodiscard]] pkgexec::backend_capability_profile capabilities() const override;
  [[nodiscard]] pkgexec::execution_result execute(
      const pkgexec::execution_request& request,
      const pkgexec::execution_resources& resources) override;
private:
  host_supervisor_backend(capability_report report,
                          std::vector<interpreter_binding> interpreters);
  capability_report report_;
  std::vector<interpreter_binding> interpreters_;
};

} // namespace pkgexec_linux
