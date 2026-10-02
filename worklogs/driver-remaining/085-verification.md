# Gate 085 Verification Evidence: CSF Event Memory Layout & Synchronization

## 1. Source and Patch Provenance
- **Clean Base Commit**: `1451e2d99ccb144a35fe958b24eaf121fad4288b` (`work/mesa`, clean git commit)
- **Patch Path**: `patches/csf-v11/085-keep-descriptor-ring-and-vkevent-syncs-in-csf-event-memory.patch`
- **Patch SHA256**: `d78782db42e4befc98e9285a452c4f72b928c3932c89700be246b47986aad1b0`
- **Target Worktree**: `/tmp/mesa-clean-085` (extracted from `git archive 1451e2d99cc` with patch 085 applied cleanly)

---

## 2. Test Enhancements and False Negative Fixes (`tests/csf-event-memory-layout.c`)

1. **Production Definition Extraction (No Replicas)**:
   - `struct panvk_cs_sync32` and `struct panvk_cs_sync64` extracted directly from `src/panfrost/vulkan/csf/panvk_cmd_buffer.h`.
   - `enum panvk_subqueue_id` with `PANVK_SUBQUEUE_COUNT` extracted directly from `src/panfrost/vulkan/csf/panvk_queue.h`.
   - All sizes and offsets dynamically derived from `sizeof(...)` and extracted enum values.
2. **All 3 Sync Words Verified (Set & Reset)**:
   - SetEvent: verifies all 3 subqueues updated to `seqno = 1`.
   - ResetEvent: seeds all 3 subqueues with non-zero inputs (`0xbeef00 + i`, error `0x55`), executes ResetEvent, asserts all 3 subqueues zeroed (`seqno == 0 && error == 0`).
3. **Flushed Pointer and Derived Size**:
   - Asserts flushed address strictly equals `event->syncobjs.host_addr`.
   - Asserts flushed size equals `sizeof(struct panvk_cs_sync32) * PANVK_SUBQUEUE_COUNT` (derived, not hardcoded 24).
4. **GetEventStatus Cache Invalidation**:
   - Extracts `panvk_per_arch(GetEventStatus)`.
   - Asserts `panvk_event_cache_op` called with `invalidate == true`.
   - Verifies `VK_EVENT_SET` on all non-zero, `VK_EVENT_RESET` on any zero.
5. **Complete Negative Control Suite**:
   - Mutant 1: SetEvent missing publish -> caught (exit 201).
   - Mutant 2: ResetEvent missing publish -> caught (exit 202).
   - Mutant 3: Cache op executed after signal -> caught (exit 203).
   - Mutant 4: Signal failure error swallowed -> caught (exit 204).
   - Mutant 5: Partial SetEvent (word 0 only) -> caught by 3-word assertion (exit 205).
   - Mutant 6: Partial ResetEvent (word 0 only on seeded non-zero) -> caught by 3-word assertion (exit 206).
   - Mutant 7: Truncated flush (1 word instead of 3) -> caught by derived size check (exit 207).
   - Mutant 8: Struct/count layout overflow (simulated count=5 > 64) -> caught (exit 208).

---

## 3. Independent Android Object Build Verification
- **Toolchain**: NDK r30 (`30.0.14904198`), `aarch64-linux-android35-clang`
- **Build Output Directory**: `/tmp/panvk-085-clean-objs`
- **Source**: `/tmp/mesa-clean-085`

| Object File | Exit Code | SHA256 Hash | Symbol Evidence |
|---|---|---|---|
| `pan_kmod.c.o` | 0 | `cda23ee1ad26dbb85bf764d16980223367c76bd8fc278db7b6d9a0567a7e17a4` | `T pan_kmod_cs_event_signal` |
| `kbase_kmod.c.o` | 0 | `e09d5b00f098f90413eb2c4af13ff0c0b922671d6964fbe91133bf88c1bd8c6a` | `D kbase_kmod_ops` (.cs_event_signal wired) |
| `csf_panvk_vX_event.c.o` | 0 | `d6ee575a7c38ceec9e3366233fccd2ca9e5d4617c5dd6df62bd0ba20379d35be` | `U pan_kmod_cs_event_signal` |
| `csf_panvk_vX_cmd_draw.c.o` | 0 | `4d284af56d8dab6a8ff7e481038cbd1abb301b4f14317d2ba79844a92ad959f1` | Render desc ringbuf reserve & signal scope |
| `csf_panvk_vX_gpu_queue.c.o` | 0 | `9a4ac51159cba94731074bf586ef724731e90f896827348c732e6df3c1730523` | Queue syncobjs BO sizing & ringbuf sync init |

---

## 4. Device Inspection and Blockers
- **Hardware**: NOT_RUN
- **Device**: `192.168.1.34:40501` (`2311DRK48I`, Dimensity 8300 Ultra `mt6897`, Mali-G615 MC6)
- **Active Processes**: `dev.zenithblue.panvklauncher` (PID 11731), `dev.zenithblue.panvktest` (PID 15719), `termux-x11` (PID 19323), `com.termux.x11` (PID 28471)
- **CTS Runner Status**: Inactive.
- **Blocker**: Active launcher and X11 container sessions running. Nondisruptive inspection preserved installation.

---

## 5. Verification Execution Log
```
$ gcc -Wall -Werror -o /tmp/panvk-csf-event-memory-test tests/csf-event-memory-layout.c
$ /tmp/panvk-csf-event-memory-test
=== Gate 085 Verification: CSF Event Memory Layout ===
Source directory: /tmp/mesa-clean-085
PASS: Negative control: isolated SetEvent checker successfully rejected mutant missing publish
PASS: Negative control: isolated ResetEvent checker successfully rejected mutant missing publish
PASS: Mock execution verified SetEvent, ResetEvent (all3 words, seeded nonzero), and GetEventStatus (cache invalidate)
PASS: Negative control: mutant SetEvent without publish caught (exit code 201)
PASS: Negative control: mutant ResetEvent without publish caught (exit code 202)
PASS: Negative control: mutant cache op after signal caught (exit code 203)
PASS: Negative control: mutant error swallowed caught (exit code 204)
PASS: Negative control: mutant partial SetEvent caught (exit code 205)
PASS: Negative control: mutant partial ResetEvent caught (exit code 206)
PASS: Negative control: mutant truncated flush caught (exit code 207)
PASS: Negative control: changed struct/count layout check caught (exit code 208)
PASS: Verified built Android objects present in /tmp/panvk-085-clean-objs
ALL CHECKS PASSED: ringbuf @192 in 256-byte BO, ioctl nr 44, all 3 sync words and cache invalidate verified
Exit code: 0
```
