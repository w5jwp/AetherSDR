# TGXL and PGXL remote authentication: hardware evidence

Captured on 2026-09-27 for issue #2313 using
[`tools/probe_4o3a_auth.py`](../tools/probe_4o3a_auth.py). The operator supplied
the device addresses and authorization code. The code was entered through a
non-echoing prompt; it is not stored in the probe or shown below. The probe
opened one TCP connection per device, read the greeting, sent one auth command,
read one reply, and closed the connection. It sent no control or transmit
commands.

Command:

```text
python3 tools/probe_4o3a_auth.py --tgxl-host <TGXL_HOST> --pgxl-host <PGXL_HOST>
```

Output (authorization code and private addresses redacted):

```text
TGXL <TGXL_HOST>:9010
  greeting: V1.2.17 AUTH
  sending: C1|auth <code>
  reply: R0|0|auth OK
  auth accepted: yes
PGXL <PGXL_HOST>:9008
  greeting: V3.9.1 AUTH
  sending: C1|auth code=<code>
  reply: R1|0|Authorized
  auth accepted: yes
```

The probe exited with status 0. A separate single-attempt PGXL probe using
`C1|auth <code>` received `R1|50000016|Incorrect parameter`; the successful
PGXL form includes `code=`. TGXL's success response uses sequence `0` despite
the command using sequence `1`, so a TGXL implementation must not require an
`R1` auth response. This observation establishes the handshake on these two
devices at the firmware versions shown; it does not establish behavior for
other firmware versions or Antenna Genius.

The [vendor TGXL API document](https://github.com/user-attachments/files/27611828/TG.XL.API.Commands.pdf)
lists `R1|0|Unauthorized` for a rejected code. Because its numeric result is
also zero, the client must check the reply body. This rejection was documented
by the vendor; it was not produced by the hardware probe above.

The [4O3A Antenna Genius TCP/IP API](https://github.com/4o3a/genius-api-docs/wiki/Antenna-Genius-TCPIP-API)
documents the optional WAN `AUTH` greeting, and its
[auth command](https://github.com/4o3a/genius-api-docs/wiki/Antenna-Genius-TCPIP-auth)
documents `auth code=<code>`. The command format specifies a carriage-return
terminator; the client sends CRLF, consistent with its other AG commands and
the contributor capture summarized below. The published response format echoes the sequence number and uses
a zero hexadecimal result for success; an optional message may follow it.
This is protocol documentation, not a live AG validation.

### Contributor-reported Antenna Genius capture

[TnxQSO-Admin's 2026-05-30 report on #2313](https://github.com/aethersdr/AetherSDR/issues/2313#issuecomment-4583904317)
describes a capture from the working 4O3A AG Utility on Windows:

- The challenged greeting was `V<version> AG AUTH\n`, confirming that `AG`
  remains in the WAN greeting.
- The utility sent `C<seq>|auth code=<code>\n`. AG emitted LF-terminated
  replies with a trailing pipe: `R<seq>|0|` or `R<seq>|FF|`.
- AetherSDR's ordinary CRLF-terminated auth attempts reached the auth parser
  and received `R<seq>|FF|`. This demonstrates accepted framing, not a
  successful authorization with those attempts. Bare CR has no equivalent
  captured evidence, so the AUTH builder also uses CRLF.
- The utility silently truncated the configured password to eight characters
  on that device. Operators troubleshooting a code set through that utility
  should verify the stored value; AetherSDR does not silently truncate codes.

These are the contributor's reported observations, not a new hardware run by
this branch. The report does not identify an AG firmware version. Successful
in-app authorization and Keychain restore still need live validation.


## AetherSDR application run

On 2026-09-27, the QtKeychain-enabled `AetherSDR.app` built from this branch
was launched with the operator's existing settings. The app established TCP
connections from the operator's client host to both configured devices. Two `nettop`
snapshots of the app process showed continuing bidirectional traffic:

| Device | Endpoint | First in/out | Second in/out |
|---|---|---:|---:|
| TGXL | `<TGXL_HOST>:9010` | 42,420 / 1,741 bytes | 67,899 / 2,667 bytes |
| PGXL | `<PGXL_HOST>:9008` | 39,674 / 1,546 bytes | 54,985 / 2,146 bytes |

These counts are per TCP connection. Both links remained established while
the counters increased. Polling starts after a successful AUTH reply **or** an
unchallenged version greeting. The traffic is consistent with working direct
connections but does not independently establish which greeting the app saw.
Packet capture was unavailable because macOS denied access to the BPF device;
no packet payload or authorization code was recorded during this app run.

## Restart with accessory credentials removed

Both `AetherSDR` Keychain items (`tgxl_auth_code` and `pgxl_auth_code`) were
deleted and confirmed absent, then the app was restarted. The restarted app
kept its TCP session to the FlexRadio at `<RADIO_HOST>:4992`, but had no
established direct connection to TGXL `:9010` or PGXL `:9008` in the observed
socket snapshot. Its log reported a TGXL direct-socket closure at 13:23:13.
The tuner still appeared active because the FlexRadio relays TGXL status on
its own connection and supports the fallback tuner commands. This observation
does not indicate that the direct TGXL authentication succeeded without a
code.

## Earlier branch build

An earlier branch build was restarted on 2026-09-27. A socket snapshot showed
established TCP connections to the
FlexRadio (`<RADIO_HOST>:4992`), TGXL (`<TGXL_HOST>:9010`), and PGXL
(`<PGXL_HOST>:9008`). This confirms that build connected to both
accessories; the snapshot alone does not prove the contents of the AUTH reply.

## Earlier Tune presses

The pre-change app log from 2026-09-26 shows
`TgxlConnection: TGXL version "1.2.17 AUTH"` at 22:57:45, immediately
followed by an unauthenticated `C1|info`. At 22:57:48–22:57:58 it records repeated
`TunerModel::autoTune: using direct TGXL path` entries followed by
`TgxlConnection: sent "C…|autotune"`. Those presses did not use the FlexRadio
relay. Before this change, `TgxlConnection` marked an `AUTH`-challenged socket
connected as soon as it read the version greeting. `TunerModel::autoTune()`
prefers that direct socket whenever `isConnected()` is true, so the app sent
Tune over an unauthenticated path. The log establishes the route choice and
outbound commands; the device's reason for not tuning was not captured.

The new handshake only marks the direct path connected after authorization.
With no saved code, the direct path stays unavailable and Tune can use the
existing FlexRadio relay. No relay command implementation was changed in this
branch. During an operator-initiated Tune action in the later 2026-09-27 run,
the radio accepted a relayed
`tgxl autotune` command (`R69|0|`) and reported `tuning=1`.

## Later application run

The app was restarted on 2026-09-27.
`lsof` showed established sockets to the FlexRadio, TGXL `:9010`, and PGXL
`:9008`. Two `nettop` samples one second apart showed TGXL traffic increasing
from 27,538 / 918 to 28,658 / 962 bytes (in / out), and PGXL traffic
increasing from 13,668 / 698 to 14,864 / 742 bytes. These are socket
counters, not captured payloads. They establish live bidirectional traffic
from that build; they do not independently show the AUTH reply text. The means
by which direct connections resumed after the Keychain-deletion experiment
was not recorded, so this later traffic is not evidence that a saved code was
restored or that either device challenged this particular app session.

The endpoint-bound Keychain change made after these captures has been checked
with an in-memory Keychain job test. Its full in-app restore and AUTH path has
not yet been observed on hardware.

## Credential scope and remaining live checks

Each accessory type has one saved Keychain record, bound to the IP address and
port of the peer that accepted the code. Connecting the same type at another
address replaces that record after the new peer accepts a code. If a device's
address changes, the operator must enter its code again; the old record is not
sent to the new address. This is the current credential scope, including for a
hostname whose resolved address changes.

A failed Keychain deletion leaves the saved code in the operating system's
vault. The setup dialog reports that Clear code must be retried; it cannot
claim the secret was removed until the vault confirms deletion.

The saved-code three-failure limit is pinned by socket-free tests for TGXL,
PGXL and Antenna Genius, including timeout and socket-close paths. The full
in-app Keychain restore, Antenna Genius firmware handshake, and docked and
floating applet layout still require operator hardware/UI verification before
the PR claims them as live-proven.

The dialog test uses an in-memory credential-store adapter, and the separate
credential-store test uses an in-memory QtKeychain job double. Their saved and
cleared codes never reach an OS vault. The handshake test uses
an isolated settings profile. The automation grab test was mutation-checked:
removing the masking call made it fail, and restoring the call made it pass.
The TGXL and PGXL host-switch assertions were also mutation-checked: removing
their typed-code target guards made both assertions fail, and restoring the
guards made the handshake test pass.
The setup-dialog recovery and rejected-code flags, and the Antenna Genius
target-specific block lookup, were mutation-checked separately: each removed
guard failed its focused test, and restoring the guard made both focused tests
pass.

## Follow-up review checks (2026-09-28)

The focused socket-free dialog and handshake tests now cover a code discarded
before verification by TGXL/PGXL disconnect or host switch, AG target switch,
and the warning shown in the setup dialog. They also cover per-target AG and
ShackSwitch retry-budget resets, AG Clear code leaving ShackSwitch's block
intact, and clearing a TGXL/PGXL blocked-state notice when the operator retries.
`peripheral_auth_dialog_test`, `peripheral_auth_handshake_test`, and
`peripheral_auth_keychain_test` passed locally. These checks inject protocol
frames or use an in-memory credential store; they do not claim a new live
hardware observation.

The next adversarial review identified a Setup-dialog ordering bug when the
operator corrected a device address and entered a replacement code. A new
dialog regression first failed against the original ordering, then passed
after the dialog retired the previous attempt before marking the replacement
code pending. It exercises the TGXL, PGXL, and AG Connect buttons and checks
that a later unchallenged greeting says the replacement code was not saved.
The test injects the production connect-ordering callback and feeds greetings
directly into the connection handlers. It opens no outbound connection and
uses no listener or simulated firmware peer. This is UI-state evidence, not
a live device handshake.


## PR #6008 review follow-up

AG authorization now requires sequence 1, a zero result, and either an omitted,
empty, or exactly `OK` message. Unknown bodies, extra fields, and zero-result
`Unauthorized` / `Denied` replies are rejected before connected state or
credential acceptance is signaled. The `OK` allowlist is a defensive client
policy tested with injected frames, not a claim that AG hardware emitted it.
The vendor API explicitly documents `V<a.b.c> AG[ AUTH]`, so the AG identifier
requirement is retained. The contributor-reported capture above confirms the WAN `AG AUTH` greeting.
A fresh AG check should record the firmware version and accepted/rejected
replies for this client; it need not treat retention of `AG` as an unknown.

Auth failure counters include both timeouts and mid-auth socket closes. AG
tracks at most 128 failed targets, ignores empty targets, and does not evict
old blocks to admit new targets. At capacity, untracked targets are blocked
until a tracked target's budget is reset or the model is restarted. This
bounds discovery-churn memory without silently granting old targets retries.

A failed Keychain Clear now retains the cached credential as well as the
vault record. Cache removal waits for successful deletion, and an older
queued delete cannot erase a newer session save. The credential tests cover
denied deletes, delete/save ordering, and consecutive queued clears.

The six CodeGuard path-traversal advisories identify a protocol comment and
unchanged MainWindow label/bundle-path code, not new path operations in this
PR. No code change is warranted for those findings. Applet-indicator design
and central settings placement remain subject to the maintainer's decision.

The initial PR #6008 review validation rebuilt and passed the socket-free
handshake and Keychain tests. Its committed dialog sources were compiled
separately against the updated libraries and passed. That patch excluded the
then-uncommitted Peripherals redesign; the current `peripherals-settings-update`
branch includes it, as described below. Mutation checks for
the AG body guard, empty-target guard, retry-history cap, and failed-delete
cache handling each failed with the respective fix removed, then passed after
restoration. Static registration, manifest, engine-boundary, capability-record,
command-plane, and whitespace checks found no new blocker.


## Maintainer review follow-up (2026-09-29)

TGXL and PGXL reconnect timers are named for socket-free inspection. The
handshake regression enables automatic reconnect, injects a saved-code
rejection or three consecutive timeouts/handshake disconnects at a nonempty
target, and checks that retry scheduling stops when authentication blocks.
It also injects a late disconnect and socket error after the block and checks
that neither re-arms the timer. No timer is allowed to perform a TCP connect.
The AG test writer is installed through a private test-access friend rather
than a public constructor parameter.

PR #6008 reflowed the existing table so each device's credentials and wrapped
status had their own lines, avoiding clipped Clear code controls and errors
at the default dialog size. That PR did not include the list/detail redesign.
The current `peripherals-settings-update` branch combines those auth fixes
with the redesign described in the next section.

The existing branch also changes automatic TGXL/PGXL connection selection:
a saved manual host/port takes precedence over the radio-reported endpoint,
and loss of radio-reported accessory presence does not disconnect a manually
configured direct connection. This supports WAN connections and also affects
LAN users who retain a manual override. Clearing only the manual override
allows the radio-reported address to be used when discovery has not been
explicitly dismissed. On the current redesign branch, Remove additionally
dismisses TGXL/PGXL discovery; clearing settings alone does not undo that
choice. Add or an explicit Connect re-enables discovery.

The stored credential remains bound to the resolved peer IP and port. A DDNS
address change therefore requires entering the code again. This preserves
the reviewed security boundary pending a maintainer choice between peer-IP,
configured-host, or combined binding; it is not a claim that DDNS reuse is
implemented. The follow-up requests that ruling explicitly, along with the
existing indicator and central-tab placement decisions.

Local validation passed all eight focused auth, automation-redaction, TGXL,
and amp applet tests. Removing TGXL/PGXL auth-block reconnect guards failed
on retry-timer assertions; reverting AG AUTH to bare CR failed on emitted
command assertions; restoring the wide Setup table failed on viewport
clipping. After restoring the fixes and rebuilding, all eight tests passed
again. Registration, manifest, engine-boundary, capability-record,
command-plane, colour-ratchet, and whitespace checks found no new blocker.
No additional live-hardware, OS-vault, or TX verification was performed.


## Current Peripherals redesign branch (2026-09-29)

`peripherals-settings-update` includes a device list with per-device detail
pages, Add/Remove controls, and a persisted visible-device list. Connection,
authorization, and wrapped status controls live in each detail page. A blocked
radio-discovered TGXL or PGXL can surface a temporary recovery row without
persisting that row or its address as manual configuration. Retrying an
unchanged radio-reported address and default port preserves discovery;
changing the address or port saves a manual override.

Remove first acquires an in-memory reconnect guard, then disconnects the
selected device, retiring any pending AUTH attempt before credential deletion.
TGXL, PGXL, and AG connection entry points enforce the guard, covering
manual targets, radio discovery, retry timers, and the shared ShackSwitch model.
The gate is in the engine rather than optional wrappers at individual callers.
Setup reports that removal is pending and prevents editing or closing until
the vault completes. Only after
successful deletion (or a reported session-only clear) does it clear connection
settings and persist discovery dismissal. Add or explicit Connect clears that
dismissal. Failed deletion retains the row, settings, and discovery policy for
retry. The canceled connection is not explicitly resumed by the removal handler;
subsequent normal auto-connect events are allowed again. A separate removal
notice states that deletion failed and the connection was stopped; it survives
later Connected/status updates and explains that normal reconnect events may
connect the device again. The operator can retry Remove.

An unrelated ShackSwitch using the shared AG model retains its retry schedule
while AG credential removal is pending. An expiry during the guard restarts
the existing five-second timer without connecting; after the guard is released,
the next expiry reconnects normally. Explicit Disconnect still cancels that
timer. The socket-free dialog test covers repeated expirations, successful and
failed deletion, and an operator cancellation during the pending window.
Removing the retry deferral made the regression fail with “AG removal consumed
ShackSwitch retry”; the guard was restored before the final build and tests.

One-shot ShackSwitch requests from initial discovery or radio connect are also
retained while the AG guard is held, even if auto-reconnect is disabled. The
model keeps at most one deferred target, leaving current device metadata alone;
the next timer expiry after guard release initiates that target. Explicit
Disconnect cancels it, and a newer connection after release supersedes it.
Successful AG removal does not repeat the initial disconnect and inadvertently
cancel this unrelated request. Tests inject both discovered IP and manual DNS
targets, success/failure of deletion, cancellation, and supersession. Mutation
checks failed when request capture or deferred dispatch was removed, and when
the redundant completion-time disconnect was restored. All three mutations
were reverted before the final build and focused test sweep.

The vault completion owns the transient guard independently of Setup. Parent
window destruction bypasses dialog close guards, but cannot persist a pending
dismissal or release reconnect suppression before the vault finishes. If Setup
has been destroyed, the completion releases the guard without touching its UI
or changing configuration. On a subsequent run the original configuration and
discovery policy remain; credential deletion may have completed, so a new auth
challenge can require entering the code again. No cross-store atomic shutdown
guarantee is claimed. Discovery dismissal applies to TGXL/PGXL, not AG discovery.

AG targets configured through the applet are reconciled into Setup both when
it opens and while it remains open, including rows already listed. Untouched
prefilled fields follow the stored endpoint; edits in progress are preserved.
Removal resets those field baselines. Save-on-close clears a target only when
the operator edited its address to empty and the stored host and port still
match the field's baseline. It preserves a newer externally saved target,
even when Setup closes before its refresh timer runs.

Latest local validation built the desktop application and passed all eight
focused tests: peripheral auth dialog, handshake, and Keychain; both automation
redaction tests; TGXL docked parity and ports; and the amp applet. The dialog
tests use injected state and an in-memory credential store, not a device peer
or an OS-vault prompt. They cover discovery dismissal and re-add, authentic
radio-reported recovery versus manual targets, removal during unanswered
credential reads, late AUTH acceptance while deletion is pending, deletion
failure and success, and blocked close/accept/reject paths during deletion.
They also destroy Setup’s parent during a held deletion, reload persisted
settings, and verify reconnect suppression survives until vault completion.
The fake store rechecks pending deletion when delivering credential reads,
matching the production refusal behavior. They also cover AG configuration
before and during Setup, and close-time
preservation of externally updated settings and connected state.

The pending-removal regressions were mutation-checked for all three devices:
removing each early disconnect admitted a late AUTH success. Removing the
close-event guard allowed unsaved edits to be committed during deletion;
removing the done guard allowed Setup to close before deletion finished.
Each mutation failed the test, and the production guards were restored.
The subsequent transient-guard regressions also failed when reconnect gating
was bypassed, vault completion stopped retaining the guard, or discovery
dismissal was persisted at the start of removal. The latest tests call the
production connect methods with injected transport initiation, rather than
calling a guard helper. They verify requests reach transport before/after the
lease, and never during it, including AG discovery, manual address overloads,
and ShackSwitch. Each of the three entry-point gates and the persistent failure
notice were independently mutation-checked. All mutations were restored before
the final build and test run.

Mutation checks caught removal-policy, recovery-persistence, initial/live AG
visibility, each of the three device teardown orders, discovery-provenance,
and stale-prefill regressions. The latest four mutations also failed when
reconciliation was restricted to unlisted AG rows, Remove stopped resetting
the port baseline, save-on-close ignored edit intent, or save-on-close ignored
a newer saved endpoint. All mutations were restored before the final build
and eight-test pass. Registration, manifest, engine-boundary,
capability-record, command-plane, and whitespace checks passed. Relative to authentication PR #6008, the UI follow-up adds one AppSettings
includer (104 to 105): the connection-persistence helper, which reuses the
existing manual endpoint keys. The manifest grows from 231 to 232 headers
(core: 194 to 195) through `PeripheralRemovalGuard.h`, tagged
`peripheral(4o3a)`, with two GUI consumers. ThemeManager remains at 157
includers. The authentication implementation's earlier validation header,
AppSettings store includer and ThemeManager helper are part of #6008,
not additions in this UI follow-up. See
[Peripherals settings UI](peripherals-settings-ui.md) for the proposed workflow
and its dependency on #6008.

The latest read-only Claude review reported no surviving code defects; it did
not execute the tests. These checks add no live-hardware, OS-vault, or TX
evidence. Maintainer review of the redesigned UI and the existing credential
binding decision remains outstanding.


### Issue #6026 follow-up: bounded removal wait

The pending deletion UI now allows closing after a 15-second deadline with an
explicit unconfirmed-deletion message. It retains configuration and the
vault-owned reconnect lease; a timeout cannot safely cancel the OS job. Late
completion never finishes the abandoned Remove or commits stale field edits.
Socket-free dialog coverage injects timeout for TGXL/PGXL/AG with both successful
and failed late completion, with the dialog retained or destroyed. Additional
coverage removes AG while ShackSwitch is live and ShackSwitch while AG is live.
Legacy endpoint keys remain unchanged; this UI follow-up is not their migration.

The macOS desktop build and focused dialog test passed after this follow-up.
Mutation checks failed when the timeout kept close blocked and when a late
completion was allowed to apply Remove; the restored implementation passed.
The test injects the single-shot timer expiry, rather than sleeping or opening
sockets. Real backend cancellation is deliberately not claimed.
