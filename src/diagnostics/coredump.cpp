// SPDX-License-Identifier: MIT

#include "coredump.h"

#include <algorithm>
#include <iterator>

namespace heliograph::diag {

/// Xtensa EXCCAUSE values, from xtensa/corebits.h. Only the ones a firmware
/// fault plausibly produces: the TLB and PIF causes exist on the architecture
/// but not on this part in any way a driver bug reaches, and listing them would
/// suggest a precision this table does not have.
namespace {

/// One row per cause: what it is called, and whether a faulting ADDRESS means
/// anything for it.
///
/// Both questions live here because they were two switch statements that had
/// already drifted -- one knew about LoadStoreRing and the other did not, with
/// no stated reason. A cause is one thing; asking two questions of it should
/// not need two lists that can disagree.
struct CauseSpec {
  uint32_t code;
  const char *name;
  /// exc_vaddr is the address a load or a store reached for. On any other cause
  /// the field is leftover, and a 0 there reads exactly like a null-pointer
  /// dereference without being one.
  bool hasFaultAddress;
};

constexpr CauseSpec kCauses[] = {
    {0, "IllegalInstruction", false},
    {1, "Syscall", false},
    {2, "InstructionFetchError", false},
    {3, "LoadStoreError", true},
    {5, "Alloca", false},
    {6, "DivideByZero", false},
    {7, "IllegalPC", false},
    {8, "PrivilegedInstruction", false},
    {9, "UnalignedLoadStore", true},
    {20, "InstructionProhibited", false},
    {26, "LoadStorePrivilege", true},
    {28, "LoadProhibited", true},
    {29, "StoreProhibited", true},
};

const CauseSpec *findCause(uint32_t cause) {
  const auto *found = std::find_if(
      std::begin(kCauses), std::end(kCauses),
      [cause](const CauseSpec &spec) { return spec.code == cause; });
  return found == std::end(kCauses) ? nullptr : found;
}

} // namespace

const char *exceptionCauseName(uint32_t cause) {
  const CauseSpec *spec = findCause(cause);
  return spec != nullptr ? spec->name : nullptr;
}

bool causeHasFaultAddress(uint32_t cause) {
  const CauseSpec *spec = findCause(cause);
  return spec != nullptr && spec->hasFaultAddress;
}

} // namespace heliograph::diag

#if defined(ESP32)

#include <esp_core_dump.h>

#include <cstring>

namespace heliograph::diag {

CoredumpSummary readCoredumpSummary() {
  CoredumpSummary out;
  // image_check verifies the stored checksum, so a partition holding a
  // half-written dump -- the plausible outcome of a panic during the panic
  // handler -- reports absent rather than handing back a garbage task name.
  if (esp_core_dump_image_check() != ESP_OK) {
    return out;
  }
  esp_core_dump_summary_t summary{};
  if (esp_core_dump_get_summary(&summary) != ESP_OK) {
    // A dump that passes its checksum but cannot be summarised is still a dump:
    // say it is there. The ELF is retrievable with the host tool even when this
    // call cannot parse it.
    out.present = true;
    return out;
  }
  out.present = true;
  out.programCounter = summary.exc_pc;
  out.version = summary.core_dump_version;
  // exc_task is a fixed 16-byte field and is NOT guaranteed to be
  // NUL-terminated when the name uses all of it. strnlen bounds it; taking
  // strlen would run into whatever follows.
  out.taskName.assign(summary.exc_task,
                      strnlen(summary.exc_task, sizeof(summary.exc_task)));

#if defined(CONFIG_IDF_TARGET_ARCH_RISCV) || defined(__riscv)
  out.causeKnown = false;
  out.faultAddressKnown = false;
  out.backtraceDepth = 0;
  out.backtraceCorrupted = false;
  return out;
#else
  // The two fields that answer "why" without an ELF. exc_cause is an Xtensa
  // EXCCAUSE and exc_vaddr is the address the faulting access reached for --
  // together they turn a reboot into "a null dereference on a load" before
  // anything is decoded.
  out.exceptionCause = summary.ex_info.exc_cause;
  out.faultAddress = summary.ex_info.exc_vaddr;

  // A CAUSE OF 0 IS TREATED AS NO CAUSE, and that is a deliberate trade.
  out.causeKnown = summary.ex_info.exc_cause != 0;
  out.faultAddressKnown = causeHasFaultAddress(summary.ex_info.exc_cause);

  static_assert(sizeof(summary.exc_bt_info.bt) /
                        sizeof(summary.exc_bt_info.bt[0]) ==
                    kMaxBacktrace,
                "kMaxBacktrace no longer matches esp_core_dump_bt_info_t::bt");
  const size_t depth = summary.exc_bt_info.depth < kMaxBacktrace
                           ? summary.exc_bt_info.depth
                           : kMaxBacktrace;
  for (size_t i = 0; i < depth; ++i) {
    out.backtrace[i] = summary.exc_bt_info.bt[i];
  }
  out.backtraceDepth = depth;
  out.backtraceCorrupted = summary.exc_bt_info.corrupted;
  return out;
#endif
}

bool eraseCoredump() { return esp_core_dump_image_erase() == ESP_OK; }

} // namespace heliograph::diag

#else

namespace heliograph::diag {

// No partition off-device. Returning an empty summary keeps main.cpp free of
// #if blocks, and the payload builder that formats this is host-tested against
// constructed structs anyway.
CoredumpSummary readCoredumpSummary() { return {}; }
bool eraseCoredump() { return false; }

} // namespace heliograph::diag

#endif
