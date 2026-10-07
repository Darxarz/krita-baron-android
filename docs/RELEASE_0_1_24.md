# Android 0.1.24 — prompt selection and hang diagnostics

The user reported an Android ANR while selecting prompt text on Samsung
SM-X806B (Android 16). The screenshot shows Android's text selection handles
and Cut/Copy/Select All toolbar while the embedded tag suggestions remain open.
No ANR thread dump from that incident was available during this fix. The device
hang itself has not been reproduced on the desktop test platforms.

Two deterministic faults were reproduced before editing PromptEditor:

* Selecting text did not close suggestions, which remained visible after the
  completion timer fired.
* One hundred identical selection-only IME events increased the document
  revision from 1 to 101 despite leaving the text unchanged.

Selection-only events now bypass Qt's empty text-edit transaction when there
is no actual commit, replacement, or active preedit to cancel. Identical cursor
and anchor values do not call setTextCursor again. Actual text commits and
preedit cancellation still use the 0.1.22 safeguard: finish the text transaction
and block layout before applying Selection attributes. Reverse selections,
stale positions, composition, and undo remain covered by the existing tests.

Suggestions close when text is selected and cannot reopen while a selection
exists. Typing over selected text restores normal suggestions. Per-query
synchronous diagnostic file writes were removed from the high-frequency IME
query path; meaningful input, paste and lifecycle stages remain recorded.

Crash diagnostics now include an ANR thread stack when Android retains one,
in addition to the latest native crash stack. The ANR formatter extracts thread
states, Java/native frames and lock information, with input/output limits;
arbitrary trace logs are excluded. Reading remains on a background thread.
Android may evict traces, so a report can legitimately lack a retained stack.
An ANR trace attached to an exit with another reason is also inspected.

Validation: the two new cases fail on the pre-fix source and pass on the fixed
source. The full native suite passes 100 cases with no failures on Windows
Qt 5.15.2 and Linux Qt 5.15.13 under AddressSanitizer (two optional cases skipped
per platform). The Java ANR and native-tombstone formatter tests also pass.

Relevant API semantics:
[ApplicationExitInfo.getTraceInputStream](https://developer.android.com/reference/android/app/ApplicationExitInfo#getTraceInputStream()).

Acceptance on the physical tablet remains pending: open a saved document with
a long prompt, long-press a word with suggestions visible, move both selection
handles, select all, copy, switch documents, paste and type over a selection.
If it still hangs, the new report should be collected after restarting; it
will distinguish UI overload from a platform thread/lock problem.
