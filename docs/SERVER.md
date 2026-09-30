# Orchestrion integration

The Android app is a C++/Qt client. Python is required on the **server** for the
reviewed Baron workflow engine; it is not bundled into the app. The compiler
only creates a graph. Jobs are submitted through the website's existing
authenticated, billed ComfyUI proxy.

On this laptop, all website edits are in the isolated checkout
`D:\orchestrion-baron-integration`, branch `baron/krita-orchestrion`, baseline
`ca6a0fd`. Production `Z:\orchestrator` remains unchanged. This repo includes a
review bundle under `orchestrion-integration`; it does not deploy anything.

## Server setup for the website maintainer

1. Review the tracked-file patch against the stated baseline and the accompanying
   new files. Preserve unrelated website changes. Integrate on a separate branch.
2. Place this Android repository's `server/` folder on the server. Create a
   dedicated Python environment and install `server/requirements.txt`.
3. Set environment variables on the website process:

   ```text
   BARON_NATIVE_PYTHON=<absolute path to the server Python executable>
   BARON_NATIVE_COMPILER=<absolute path to server/compile_workflow.py>
   ```

4. The website fetches installed model and node metadata from the user's selected
   ComfyUI server. Client-supplied `object_info` and model metadata are ignored.
   Configure the existing user/server selection normally.
5. The route `POST /api/krita/native/prepare` requires authentication. It returns
   the compiled graph and the existing billing estimate, in bleatbucks only.
   Workflow preparation does not submit a GPU job or debit coins.
6. Run the isolated site's integration tests using an in-memory database. Then
   test consent, logout, native preparation, pricing and result ownership with a
   staging account before following the site's normal rollout procedure.

## Native flow

`device/start → browser approval → device/poll → account/models → native/prepare
→ /krita/connection/prompt → owned history/view → apply new Krita layer`.

The browser URL includes the short approval code. It is prefilled, but approval
still requires a user click. The credential is sent in Authorization headers
and sealed in Android Keystore. Logging out clears the local credential even
if the website is offline; online logout also attempts server revocation.

The compiler limits concurrent processes to two, imposes a 45-second timeout and
does not copy stderr into public logs. It uses no production DB or local ComfyUI
installer. Credentials, models and GPU workloads remain outside this repo.

## Current boundaries

- Native preparation has not been deployed on the production website.
- Compiler tests use real saved ComfyUI metadata, without executing inference.
- Exact quotes require preparing a graph, including canvas/reference data.
- Reference thumbnails currently accept HTTPS images from image.civitai.com;
  unavailable previews display a themed placeholder.
- Job recovery is available after connection errors while the app remains open.
  Recovery after Android kills the process is not implemented yet.
- Live, Animation, Custom Graph and all advanced desktop controls are subsequent
  ports; they must not be advertised as complete.
