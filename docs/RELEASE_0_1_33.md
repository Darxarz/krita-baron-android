# Krita Baron Edition 0.1.33

- Persistent thumbnail cache in app data, shared concurrent downloads, background
  disk IO and decoding, atomic files, corrupt-cache recovery. Approximate 256 MiB
  disk budget (pruned periodically), 8 MiB RAM cache. Existing HTTPS image and
  redirect restrictions remain; no credentials sent to image hosts.
- Fixed-position touch gallery, folders and multi-tag filtering, sidebar toggle,
  names/folders/tags/trigger search. Information buttons open author metadata
  without applying a model. Normal card taps preserve model/LoRA selection.
- Detail view includes trigger insertion, plain-text author/model/version notes,
  explicit recommended settings and separately labelled example parameters.
  Optional metadata errors never emit generation errors. JSON is parsed in a
  background worker and cached per site/device connection for 24 hours.
- Browser device tokens require the accompanying reviewed Orchestrion API patch
  in `server/orchestrion-catalog`. Production site source is not modified by the
  patch preparation script. Author data only appears where metadata exists.
- New labels translated into the 14 shipped language catalogues.

Validation: Qt Windows and Linux ASAN tests, cache cross-instance recovery,
in-flight deduplication, info tap without selection, tags/trigger normalization,
authenticated metadata route and existing device scope restrictions. Physical
Samsung tablet validation remains separate from these checks.
