# TLOU2 Frame Generation input investigation

## September 12: release integration

The user confirmed that nested-source.1 fixes the TLOU2 flicker. The approved
manager v1.0.2 integration retains the same guard and removes diagnostic probes.
See `MANAGER_1_0_2_RELEASE.md` for build paths, verification, and release status.

## September 12: nested-source.1 (candidate investigation record)

User log `b7146c86-d817-4f15-844e-67f37383a046` loads boundary-trace.2.
Three pairs of native SR admissions share thread, command, feature, parameters,
resources and millisecond tick, but the application counter advances between
returns (37164/37165, 37304/37305, 37324/37325). There are 51 NR evaluations
for 48 apparent source groups at one pass. The trace also shows real SR-to-FG
queue fence signal/wait edges, so missing synchronization cannot simply be
assumed. The burst loses 192 records and four boundary observations. Token
observation is disabled; UINT64_MAX fence samples are not completion proof.

The supplied 60 fps recording shows fluctuating scene detail while rendering
continues. Its 10:01 time does not align with the log's 09:58 trace; no individual
video frame is claimed to correspond to a particular trace event.

Confirmed local bug: the verified base's common DX12 evaluation dispatcher at
RVA 4B140 is called by nine forwarding wrappers. Nested wrappers can execute its
native-SR return handler twice for one vendor evaluation. The upstream gate
normally rejects the second return by application counter, but accepts it if
that counter changes between returns. This is not evidence of two registrations:
the two post-callback insertion sites are alternative vector insertion paths.

`framegen_nr_host` now reproduces this using an extra forwarding call through
4B140 around one real SR evaluation. Only the fixture's current-thread frame
override is advanced between returns and restored afterwards. The previous
binary fails: `frame 20 native passes 2 expected 1` in
`build/framegen-fixture-nested-source-baseline-1`. Production never writes this
override, a global frame counter, game parameters, or vendor inputs.

The candidate reuses the existing MinHook helper to wrap that one shared
dispatcher. Stack scopes in Windows TLS record completed native SR; a completed
child marks only matching command/feature/parameter ancestors. Their redundant
return is rejected before the original gate mutates state. Separate calls and
fresh sibling scopes remain independent even with identical pointers. Configured
passes still run inside the parent, failed parents are not marked complete,
owned retries are exempt, and other source types are untouched. No cross-frame
pointer cache, timeout recovery, resource replacement or feature reset is added.
Unknown aliases are not guessed equivalent. TLS/setup failures preserve original
admission. Modules and TLS remain process-owned while the pinned hook is active.

Activation now uses the existing DX12 device-init callback, with an overlay
fallback for late attachment, not DllMain/loader-lock hook installation. An early
iteration activated only at the first overlay and missed the initial frame;
the final fixture checks every SR call from frame zero. The candidate-only
`NR_NESTED_SOURCE_GUARD` flag leaves normal and boundary-trace.2 modes unchanged.
The boundary active mask adds bit 64; the separate guard summary reports actual
tracking status and the cumulative suppressed-return count.

Validation of the final binary:

- Unit checks: nested and duplicate returns, sibling/sequential calls, three
  configured passes, failed parents, distinct command/feature/parameter keys,
  retries, another thread, fail-open behavior, exact vendor arguments/results.
- `framegen-fixture-nested-source-final-1`: 390 real NR evaluations, 278 scaled
  calls, scale changes through 50/75/99/100/101/125/150%, presets, off/on, two-pass
  manual FG routing and 11,440 synthetic FG bypasses. Correct pass count from
  frame zero. 36 traced sources, zero drops, one explicitly incomplete trailing
  callback at the one-second deadline. The validator now checks this bounded
  tail separately while rejecting missing returns inside the capture.
- `framegen-fixture-nested-source-normal-1`: unmodified SR call flow, 390 NR
  evaluations, 321 scaled calls, 11,040 synthetic FG bypasses; 46 traced sources,
  zero drops. All FG callbacks preserve their parameters and perform zero NR.
- Normal UI/preset/trace/lifetime/scale tests, DX11 lifecycle tests, live WARP
  queue interception, 65,536 Vulkan ordinal checks and static addon validation
  pass. These are not game or interpolated-frame acceptance tests.
- `framegen-fixture-nested-source-mfg-1`: 1,199 synthetic FG callbacks covering
  indices 1-4 at 100% scale; zero NR evaluations and zero scaled work.

Final addon SHA-256:
`6769D8EA698571509D5740A4D3FB884F8495284D20F942E7D91C5B5AEBE85092`.
Package: `build/tlou2-nested-source-1.zip`; build ID
`1.0.3-tlou2-nested-source.1`. Rebuild with
`scripts/build/build-dx11-experimental.cmd nested-source`, then
`scripts/build/build-framegen-host.cmd`. Use the existing fixture driver with
`-Recovery -NestedCandidate -NestedSource -ScaleChurn -InputTrace -InitialScale 100`
and a fresh `build/framegen-fixture-*` directory/configuration. Omit
`-NestedSource` for ordinary routing; use `-MfgCadence -InitialScale 100` without
`-Recovery` for synthetic FG-only transparency.

No game files, manager/public payloads, NVIDIA DLLs or public releases changed.
Public upstream.3 SHA remains `6121E0F743E2092B1965DC03257DCF0A452F6405CA6AE0CE287CC36149433838`;
normal-build SHA remains `3579F7571F3130A83F3DE3FEA518D83FDD4DAE320B19065C50C63FBFBA917D7A`.
The local duplicate-admission mechanism is fixed in the candidate. Whether it
fully explains TLOU2 flicker still requires game evidence; no universal fix is
claimed, and BOTDW/KCD2 installations remain untouched.

## September 12: boundary-trace.2 (previous diagnostic)

User's 09:21 attachment (`57b22952-dc78-4f76-bf38-fcf4e3a583dd`) confirms trace.1
loaded, but logged `Streamline export prologue mismatch` at 09:22:09.534. Earlier
resource-tag hooks were already active 2/2. Its manual burst has 61 successful NR
evaluations, zero dropped records and no new tag/native-queue observations.
`missed=0` was not evidence that the disabled probe had complete coverage.

The trace.1 setup incorrectly required pristine exports after upstream hook
installation. Confirmed source flow: `streamline_v2.hpp` registers Hooked_slSetTag
and Hooked_slSetTagForFrame; they may rewrite resources and unwrap commands before
forwarding. `vtable.hpp` uses Detours. The verified base binary registers callback
RVAs A240/B920 and Real_* slots 26DE40/26DE48. The latter are read at A37F/B4CC/
B531/B7A9 and BA83/CBA5/CC0E/CE9F, respectively.

Trace.2 reuses the existing callbacks: MinHook observes their entry points, not
the already-detoured Streamline exports. The Real_* slots and original export
detours are never replaced. Tags are labeled BEFORE RenoDX rewriting, not final
vendor inputs. Fixed callback prologues and non-null registration slots are
checked against the hash-verified base; SL profile and structure guards remain.

Queue installation is independent and runs even with no Streamline module or
failed tag setup. Token setup has its own result. Each hook reports its status;
the capture-end summary reports an active mask, so absent hooks cannot hide
behind a zero missed-event count. Observers remain pinned, manual-only and do
not add GPU work or change rendering. Image readback remains disabled.

Tests passed: pre-existing export detours plus production observers on both
callback signatures, exactly-once forwarding, unchanged earlier Real_* slots,
and preservation of the older chain after observer removal. This test models the
earlier chain using MinHook; it does not launch Streamline. Actual WARP queue
tests use the production installer after rejecting an unsupported SL image.
The final addon fixture `framegen-fixture-tlou2-boundary2-1` passed 390 NR evals,
321 scaled calls, 11,040 synthetic FG bypasses and no NR in those callbacks.
Its 41 source groups/439 input-trace records had zero drops; independent native
queue/fence observations are now checked too. These are not interpolated frames.
DX11 lifecycle, 65,536 Vulkan ordinals, normal UI/lifetime/scale and static addon
validation also passed. Normal binary SHA remains
`3579F7571F3130A83F3DE3FEA518D83FDD4DAE320B19065C50C63FBFBA917D7A`.

Candidate `build/tlou2-boundary-trace-2/renodx-dlss5-super-anus.addon64`, SHA-256
`C3AA0E91ED16BC39E49E84C26C33DDF19D824B1879196D63C0DABA9BB08B9D18`.
Build with the existing `framegen-boundary-trace` mode; fixture `-BoundaryTrace`
now selects trace.2. Package README has capture instructions. No game files or
release payloads were changed and no publication was performed. Next evidence:
TLOU2 capture with individual hook activation statuses and native queue events.

## September 12: boundary-trace.1 (superseded diagnostic)

Implemented the approved boundary-observation phase, not image readback or a
flicker fix. New code is compiled only with `NR_FRAMEGEN_BOUNDARY_TRACE`; the
existing MinHook dependency and bounded FrameTrace buffer are reused.

- Observe `slSetTag`, `slSetTagForFrame` and `slGetNewFrameToken`. Decode only
  checked v1 GUID/prefix layouts; preserve arguments and results exactly once.
  Pair pre-call tags with their result via a call serial. Tokens stay opaque;
  only an explicitly supplied frame index is reported, otherwise UINT_MAX.
- Observe native D3D12 queue Execute/Signal/Wait calls, filtered to command/queue
  identities seen in the same capture. No GPU commands or resource retention
  are added. Fence completion is sampled while the caller owns the fence.
- Bound identity tracking to 128 commands/16 queues without eviction. Log
  cumulative missed observations separately from FrameTrace dropped events.
- Install only for DX12 and the inspected Streamline image profile; verify
  export prologues and queue entry ownership. Pin modules so callbacks and
  trampolines remain mapped. Setup failures disable or explicitly report partial
  observation. No user DLL is substituted to bypass these checks.
- Manual-only one-second bursts; the input-trace automatic trigger is disabled.

Inspected game `sl.interposer.dll`: version 2.13.0.0, SHA-256
`27B2190057994C0B287C2C5716953BF1586F6499AC12FBBB2092B9AAF8396570`,
PE timestamp `6a7c8ed5`, SizeOfImage `a2000`. Runtime checks use the image profile
and prologues, not this full hash. Public main headers supplied the versioned
prefix definitions; they are not claimed to be the exact installed 2.13 headers:
[structures](https://github.com/NVIDIA-RTX/Streamline/blob/main/include/sl_struct.h),
[resource types](https://github.com/NVIDIA-RTX/Streamline/blob/main/include/sl_core_types.h),
[API declarations](https://github.com/NVIDIA-RTX/Streamline/blob/main/include/sl_core_api.h).

Important limits: tags supplied through other APIs or outside a capture may be
missed; another queue implementation may bypass the observed vtable targets.
Command pointers are identities, not generation/lifetime proofs. A tag's declared
state is not necessarily the current resource state. UINT64_MAX fence samples
are unavailable/device-removed, not success. No blanket completion assertion is
made. FG feature classification, current per-resource state, full queue ordering
and pixel correctness remain unproven, so image readback is deliberately absent.

Checks: `tests/framegen_boundary.cpp` covers ABI rejection/decoding, original
forwarding/results, capture reset/capacity, and actual WARP queue hooks/fences.
The diagnostic build passed DX11 lifecycle, 40,000 scale-history records, 65,536
Vulkan ordinals, and static addon validation. `build-v66.cmd` also passed its
normal trace, lifetime, preset and UI regressions. The isolated rendering fixture
passed 390 NR evaluations, 321 scaled calls, 11,040 bypass callbacks and no NR in
synthetic FG callbacks. The first fixture attempt correctly failed the trace
assertion because its harness expected auto-capture; the harness now explicitly
requests the manual diagnostic and the trace validator passes. This fixture does
not load Streamline or generate interpolated frames; tag forwarding is tested
synthetically, queue detours separately on WARP.

Final binary SHA-256:
`6DCB52F5BCBF5B98F384EDC58BC89D09A3223DC7387EBD244D8871FD7A5B062A`.
Final fixture: `build/framegen-fixture-tlou2-boundary-trace-3`, 40 source groups,
429 input-trace events, zero dropped. ZIP: `build/tlou2-boundary-trace-1.zip`,
SHA-256 `582606A6A59891355F8B4C6FAED04BD8BB1E5820FDB32086545D8334F412DE87`.

Build/package: `build/tlou2-boundary-trace-1/` (README contains manual capture
instructions). Build using `build-dx11-experimental.cmd framegen-boundary-trace`;
run the fixture with `-Recovery -ScaleChurn -InputTrace -BoundaryTrace` in a fresh
fixture folder after rebuilding `build-framegen-host.cmd`. Preserve matching
embedded DLL/map files when resolving fixture RVAs. No game install or GitHub
publication was performed. Manager and upstream.3 payload hashes remain
`6121E0F743E2092B1965DC03257DCF0A452F6405CA6AE0CE287CC36149433838`.

Next: capture TLOU2's actual Streamline/native-queue path using the packaged
instructions. Only consider image readback once the above ownership/state gaps
are closed; do not infer a rendering fix from CPU identities alone.

## September 12: in-addon image capture feasibility

The approved first step was to establish whether existing screenshot/readback
ownership can support a paired NR/FG image diagnostic. Result: NR-side readback
is reusable, but the current FG observation boundary does not provide the safety
contract needed to implement the complete diagnostic. No new binary or rendering
change was made, and no new gameplay test is requested at this stage.

Evidence from the current code:

- `screenshot_capture.inl::before/after` records an ordered NR chain on a tracked
  command list. It requires compute-readable input and explicitly certified UAV
  output, restores the output state after copying, and charges allocations to the
  existing 512 MiB budget. `finish` maps only after backend retirement establishes
  completed GPU work. Its current pair is NR input/output, not FG input/output.
- `framegen_probe.inl::observed_framegen_callback` reads raw DLSSG resource
  pointers from a general NGX pre-evaluation callback. The keys can be stale on
  SR calls. There is no certified FG feature classification, per-resource state,
  source-frame token, or owning submission queue in that observation.
- The 00:28 gameplay trace resolves the FG command identity, but records no
  submission for it. `on_execute_recording` relies on ReShade events; resolving
  a ReShade wrapper does not establish that its submission was intercepted.
  `collect_resources_locked` explicitly refuses safe retirement when no queue
  was observed. COM retention alone would not prove GPU completion or ordering.
- `final_screen_capture.inl` has a safe immediate-list/fence path for a known
  PRESENT-state swapchain image, not arbitrary FG textures. Its existing HDR
  pair deliberately suppresses NR briefly via `g_capture_off_until`; using that
  mode unchanged would perturb this diagnostic. Its tone-mapped PNG output also
  is not an exact raw HDR resource dump.

No current `slSetTag`/`slSetTagForFrame` observer or generic resource-state tracker
was found in `src`, `tests`, or `tools`. NVIDIA's tagging contract exposes resource
state and lifecycle at that earlier boundary, but immutable tags describe state
at use time, not necessarily at tag time; volatile tags may produce a separate
snapshot. Capturing a tag alone must not be mislabeled as capturing the actual
NGX FG input. Reference:
[DLSS-G tagging recommendations](https://github.com/NVIDIA-RTX/Streamline/blob/main/docs/ProgrammingGuideDLSS_G.md#52-tagging-recommendations).

Required scope before image capture: validate the installed Streamline tagging
ABI and observe frame/resource/lifecycle metadata without modifying it; establish
the actual FG command submission and completion path and resource states at the
chosen copy boundary. Only then reuse the bounded readback/retirement machinery,
without NR suppression, and preserve raw pixels plus metadata for comparison.
Missing contracts must reject capture before recording GPU work. This is deeper
instrumentation than an extension of the existing screenshot button, not a
proven fix for flicker. Manager/release payloads and game files remain untouched.

## September 12: copy-back review after the 00:28 capture

Status: no rendering defect established, so no rendering patch or new binary was
made in this review. Input-trace.2 remains the diagnostic candidate, not a fix.

The `00:28:41` log confirms the tested input-trace.2 build. Both captures ended
with zero dropped records. All 84 recorded NR evaluations returned success.
Handle `0x20469a2e670` correlates to the native SR/NR source; the other handle
`0x2046a086710` has the 1/2/3 MFG index cadence. Both receive the same NGX parameter
object `0x2002a011760`, explaining why DLSSG keys also appeared on SR callbacks.
This shared identity alone does not establish a data race or invalid lifetime.

In the second (gameplay) capture:

- There are 37 source begin/end pairs and 37 successful NR evaluations.
- The second SR post callback is denied by the existing duplicate gate; it does
  not cause a second NR evaluation.
- Each of the 37 index-1 FG observations matches the preceding source's motion
  and depth addresses. This is address correlation, not a proof of frame pixels.
- All 37 recorded source submissions use queue `0x20028827440`. The observed FG
  native command `0x200402947c0` has NO submission record in the capture. Zero
  dropped buffer records therefore does not mean complete GPU/queue coverage.

Code/binary path inspected:

1. Native post-evaluation observer 0x9BD10 builds the SR descriptor and calls the
   original gate and parent. The log explicitly reports `replace_source=true`
   and `return_output=false` for the SR source.
2. `embedded_capture_parent` registers the command and forwards to 0x573B0.
   With no screenshot active, `capture::parent` directly calls that original.
3. At one pass/100%, the scaled evaluator bypasses private scaling and calls the
   original NR evaluator. Scaled passes resolve into the original caller's NR
   output with a UAV barrier; the ordinary build does not suppress copy-back.
4. The verified parent's copy-back branch (descriptor +0x43) copies the private
   result through the encoding/output bridge into the SR source texture and
   restores the source's incoming usage on the same command list. This branch
   is present in the installed binary; its copy operation was not instrumented
   by the CPU trace, so it is not a captured pixel-content verification.
5. The remaining path from the modified SR output through game postprocessing,
   HUD-less tagging/copying, and FG consumption is not visible in this trace.

Static addon validation passed again. The installed TLOU2 addon hash equals the
packaged input-trace.2 hash. Local file versions are 310.8.0.0 for SR/FG/NR and
2.13.0.0 for Streamline; version differences are not evidence of the cause.

NVIDIA's [DLSS-G tagging contract](https://github.com/NVIDIA-RTX/Streamline/blob/main/docs/ProgrammingGuideDLSS_G.md#52-tagging-recommendations)
requires correctly ordered producer commands and states; volatile tags may cause
Streamline to make a snapshot for later FG use. It does NOT establish that TLOU2
snapshots before NR. That remains a hypothesis, not a reason to overwrite FG
inputs, clear motion vectors, retag resources, or add an unconditional queue wait.

Required next evidence is an in-game graphics capture of the SR copy-back,
postprocessing/HUD-less image, FG inputs and queue dependencies, ideally covering
adjacent real/generated frames while the flicker occurs. Another identical CPU
log cannot supply those pixels or missing dependencies. TLOU2 was not running
during this review. Nsight Systems/Compute were found in the standard NVIDIA
installation folder; Nsight Graphics/PIX/RenderDoc were not found in the standard
locations checked. Installing a graphics capture tool and launching the game
under it requires a separately confirmed setup. Capture/replay compatibility
with this game and DLL combination must be checked, not assumed.

Only this investigation record changed in this review. Existing uncommitted
diagnostics, game files, manager payloads, BOTDW/KCD2 and public releases were
preserved. No game launch, debugger attachment, tool installation or push occurred.

## September 12: input-trace.2

The user's `00:00:30` TLOU2 log loaded input-trace.1 correctly. Its two captures
contained 298 successful NR evaluations, 1,303 observations labeled FG, and
5,465 pre-submit records. They dropped 17,851 and 16,847 events respectively.
These are CPU observations, not proof that the displayed/generated frames agree.

Inspection of the verified official image explains an ambiguity in that trace:
RVA 0x7CCCD/0x7CCFE inserts the observer into a general pre-evaluation callback
vector at 0x26E0B0. The dispatcher at 0x4A050 invokes that vector, calls the
vendor, then invokes the post-evaluation vector at 0x26E0E0, forwarding the
vendor result. Merely finding `DLSSG.*` keys does not identify an FG feature.
The second feature handle in the game log therefore cannot be labeled another
FG feature without correlation. This is a diagnostic defect, not a proven
rendering root cause.

Input-trace.2 preserves rendering and corrects the diagnostics:

- Calls those observations `NGX-pre`, with parameter identity and caller RVA.
- Wraps only the existing post-evaluation observer's two registration paths
  (0x7CD18/0x7CD49, byte-validated) in this diagnostic build. The wrapper forwards
  every argument/result to the original 0x9BD10 handler. Thread-local, scoped
  feature/parameter identity is copied into nested NR source records. Post
  records identify whether that callback actually produced an NR source.
  `nr-source=0` remains unclassified; it is not proof of FG.
- Marks only commands directly observed during this capture. Submission consumes
  the marker once; reset clears it; old capture IDs cannot carry into a new
  capture. Lifetime references and fence ownership are unchanged.
- Uses a one-second burst with the existing 4,096-record bound. Overflow and
  lock contention are still disclosed; this is not a promise of lossless logging
  at every frame rate. Ordinary builds retain the ten-second window and do not
  patch the post-evaluation observer.

The extended fixture places stale DLSSG keys on an actual SR feature. It
reproduced two pre-observations per source while proving the source feature and
parameter identities: 32 source groups, 64 SR pre-observations, 32 synthetic FG
observations, 352 total records, zero dropped. The longer run completed 390 NR
evaluations with scale changes, NR off/on, multipass and 10,000 manual FG calls;
none of the FG callbacks performed NR. These are synthetic FG inputs, not
interpolated game frames. The unit test also rejects 20,000 duplicate submission
attempts, old capture tags and expired tags. Standard UI, lifetime, DX11 and
Vulkan ordinal checks passed.

Package: `build/TLOU2-FG-input-trace-2.zip`. Tested addon SHA-256:
`CC13CEB746A1082C5FF39FA38A5D082781ADEC211A8699386A6B7330580EC70A`.
Source changes remain local. Manager/public payloads remain upstream.3, SHA-256
`6121E0F743E2092B1965DC03257DCF0A452F6405CA6AE0CE287CC36149433838`.
No game files were installed or modified.

Next evidence: manually capture during focused TLOU2 gameplay with FG/NR on,
initially one pass at 100%. Use Debug > Capture FrameGen input trace, close the
overlay immediately and play for 30 seconds to allow drainage. Send ReShade.log
containing build ID `1.0.3-tlou2-input-trace.2` and trace END. The first automatic
capture may happen in menus; let it drain before requesting the gameplay capture.
This build is not a flicker fix. Do not change BOTDW/KCD2 or publish it as a release.

## Original investigation (input-trace.1)

User reports continuous flicker only with in-game Frame Generation, across hook
methods and NR resolutions, including one pass at 100%. BOTDW's in-game Frame
Generation and KCD2's driver Smooth Motion are reported working with the released
`1.0.3-framegen-upstream.3` addon. Preserve both as regression references.

The TLOU2 log from September 11 confirms the released addon, successful NR
evaluations, and no repeated scaled/native admission fallback. The subsequent
23:38 run still reports late NGX/Streamline attachment, but that is not proof of
the flicker's cause. No game configuration has been changed by this investigation.

The current FrameGen and Upscaled selections share the native-SR route. Ordinary
success logs do not establish whether FG consumes the changed image or an earlier
snapshot, nor do they prove source-frame identity, GPU execution order, or pixel
correctness. Distinct SR/FG resource addresses alone are normal after game
postprocessing and must not be treated as proof of a mismatch.

## Diagnostic candidate

`1.0.3-tlou2-input-trace.1` keeps upstream.3 rendering and adds bounded CPU-side
observations using the existing trace buffer:

- Source submission begin/end: descriptor frame, source, native command list,
  original color, motion/depth resources, dimensions and parent result.
- FG observer: raw and resolved native command identity, feature, backbuffer,
  HUD-less color, motion/depth, MFG index and application counter. The latter is
  explicitly not labeled as a proven FG source frame.
- Pre-submit notifications for tracked NR or observed FG commands, including
  queue identity. These are not GPU completion events or cross-queue fences.
- Existing per-pass evaluation and duplicate-gate records.

The special build requests one automatic capture after successful NR and FG are
both observed. `Capture FrameGen input trace` in the Debug section requests another.
Capture lasts ten seconds, holds at most 4096 events and reports dropped records.
The overlay drains records afterwards; allow about 30 seconds after requesting a
capture. No GPU commands, NR replay, vendor parameter writes, or feature resets
are added to the FG callback. The diagnostic command flag is not a lifetime pin.
The special automatic capture is not enabled in ordinary builds.

## Checks and reproducibility

Build with `scripts\build\build-dx11-experimental.cmd framegen-input-trace`.
This includes the same DX11 and Vulkan compilation flags as upstream.3 and runs
the export-ordinal regression. The candidate uses the existing game-test output
path for the fixture's hash/map identity checks; copy it into the separate package
directory before another experimental build overwrites that generated output.

Rebuild `scripts\build\build-framegen-host.cmd`, then run the existing fixture
with `-Recovery -ScaleChurn -InputTrace` in a fresh fixture directory. The driver
checks the captured identities against resources owned by the fixture and ensures
that NR and FG resource identities remain distinguishable. The fixture also checks
that 10,000 manual-FG callbacks do not perform NR or change vendor arguments.

Final fixture: 579 NR evaluations (501 scaled calls), 269 traced source groups,
144 FG inputs, 1907 records, zero dropped. Command alias and pre-submit correlation
checks passed. Trace bounds/concurrency, command lifetime, scale history, UI,
DX11 lifecycle and Vulkan ordinal checks passed. These use synthetic FG parameters
and do not validate interpolated game frames.

Next required evidence: a TLOU2 trace while focused in gameplay with in-game FG
and NR enabled. Determine frame/command correlation before choosing a renderer
change; if resources differ due to copies or postprocessing, this CPU trace alone
cannot establish their pixel contents. No fix or new manager release is claimed.
