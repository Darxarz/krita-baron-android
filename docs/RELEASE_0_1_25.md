# Android 0.1.25 — queue capacity and cancellation

The previous queue limited all retained rows to eight, including failed jobs.
The panel preflight also counted failures. Seven failed rows therefore left
only one slot for another generation. This was a client-side restriction;
the website application and its shared server queue were not modified.

The native client now permits 128 active jobs and batches of 64. Failed and
cancelled jobs do not count as active. Preparing/uploading/submitting is bounded
to four jobs at a time; waiting requests retain their captured inputs and are
started in local queue order, including the existing front/replace choices.
The 256 MiB queued-image memory guard remains and now has a distinct error
message. Large image requests can reach that guard before the task-count cap.

Cancellation releases the local slot and captured data immediately. Waiting
jobs that have not begun preparation never reach the server. A submission
already in flight retains its client until its response arrives, so a late
accepted prompt is cancelled remotely rather than revived locally. Progress,
prepared and result callbacks cannot reactivate a cancelled row. Failed
remote cancellation is reported explicitly and can be retried with Cancel;
it does not occupy an active queue slot. Replace Queue waits for remote
cancellation confirmation and does not submit a replacement if confirmation
fails. No global shared-queue clearing operation is used.

Remote cancellation targets the specific prompt ID. After deleting a queued
prompt, the client checks whether it began running during the delete and sends
a targeted interruption if needed. Unconfirmed deletion is reported. HTTP
requests also have absolute deadlines, and obsolete preparation/download
requests are aborted on cancellation.

Successful history/queue checks that cannot find a job on three successive
polls stop monitoring it with an explicit error. Jobs present in either the
pending or running server queue reset the missing counter. No prompt is
automatically resubmitted or paid for again. Retry of a failed result still
resumes its original prompt ID rather than submitting another generation.

New regression cases cover capacity with eight prior failures, 128 active
jobs, four preparation slots, cancellation of local waiting jobs, late
submission/progress callbacks, cancellation failure/retry, a real client with
a delayed submit response, the pending-to-running cancellation race, and
missing server jobs. Existing result, cloud backend, canvas, history, prompt
selection and clipboard tests remain in the full suite.

The new queue errors are translated in all 14 native interface languages.
Physical Samsung testing remains pending. The APK keeps the same package and
signing certificate, and includes the previous 0.1.24 selection/ANR fixes and
96 public font families.
