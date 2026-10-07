# Current parity: Python plugin → Android

Updated 2026-10-08. Reference: desktop `1.53.0-baron.15`, Android `0.1.36`.
This report compares current source, rather than reusing the 0.1.12 audit.
**Implemented is not a claim that every GPU/backend/device combination was tested.**

## What is already implemented

Generation/refine/edit and denoise; custom inpaint modes and context; ControlNet
and reference rows with preprocessors; regional prompts; tiled diffusion upscale;
foreground extraction; result preview/apply/empty-space hiding; grouped history
and document persistence; queue/cancellation/progress; prompt completion/actions;
separate prompt/style banks; model galleries, metadata and thumbnail caching;
orientation profiles; original credits and connection options.

The old audit's statements that tiled upscale, custom context and touch history
were absent are now obsolete. Source: `BaronPanel`, `InpaintWidget`, `UpscaleWidget`,
`GuidancePanel`, `HistoryStore`, `OrientationLayouts` and `krita/BaronDocker.cpp`.

## Missing or partial

| Mechanism in the Python plugin | Android 0.1.36 | Remaining work and source evidence |
| --- | --- | --- |
| Live workspace | Missing | Continuous regeneration after drawing, live preview/application and live-specific settings. `BaronPanel` only creates Generation, Upscale and Background workspaces; `InterfaceSettings` explicitly disables Live controls. |
| Animation workspace | Missing | Frame/keyframe/timeline generation and batch processing. Desktop: `ui/animation.py`, `model/model.py`; no native counterpart. |
| Custom / Graph workspace | Missing | Import arbitrary ComfyUI workflows, expose custom graph parameters and run them from their own workspace. Desktop: `ui/custom_workflow.py`, `model/custom_workflow.py`; no native counterpart. |
| Vector OpenPose | Missing | Add Skeleton, multiple vector characters, joint/limb editing and Python pose-layer logic. Raster Pose references and pose preprocessing already work. Desktop: `pose.py`, `ui/control.py`; native guidance contains no skeleton editor. |
| ControlNet resource-aware availability | Partial | Native rows use architecture rules and a fixed 16-row limit. The original negotiates individual resources/range support and GPU-dependent limits, with detailed missing-model warnings. |
| User ControlNet preset overrides | Missing | Native guidance reads bundled `:/baron/presets/control.json`; it does not load the user's override file. Built-in presets, Custom Values, strength and start/end sliders are present. |
| Complete region model | Partial | Linked layers, regional prompts/masks and applying region layers exist. Full automatic background/coverage, layer hierarchy and region-only behavior need comparison and porting. Desktop: `model/region.py`; native: `GuidancePanel`, compiler adapters and `BaronDocker`. |
| Bulk history interactions | Partial | Single-result context actions, stars, preview and persistence exist. The Python extended-selection/range gestures, bulk operations and complete shortcut set are not reproduced by native `HistoryList`. |
| All automatic mask-transition settings | Partial / deliberate default difference | Custom context and fill now use the original functions. Native requests leave extra grow/feather/blend at zero by default to preserve manual feathering. The original automatic transition settings and their complete UI are not copied one for one. |
| Every backend behaves identically | Not established | Direct/custom ComfyUI uses the vendored Python workflow engine. Interstice uses a separate native serializer; complete model/resource and image-output parity still needs real-backend checks. |
| Every widget pixel and warning state | In progress | Main layout, custom inpaint row and upscale controls follow the original, but narrow-dock behavior, some dialogs, warnings and touch/desktop gestures still need visual/device comparison. |

The Android app's local 16MP capture guard is an additional constraint, separate
from the plugin's diffusion megapixel setting. This can matter for large documents.

## Intentionally outside this Android port

The desktop managed local ComfyUI installer and its GPU/runtime installation are
not embedded in the tablet APK. The agreed Android design uses remote inference.
They remain available in the desktop plugin. A proposed separate 3D-reference tool
is not an original AI Diffusion feature and is not counted as a missing port feature.

## Suggested order

1. Resource-aware ControlNet, detailed warnings and vector OpenPose.
2. Remaining region composition and bulk history behavior.
3. Remaining settings/dialog visual parity and automatic mask controls.
4. Live, then Custom/Graph and Animation as separate substantial workspaces.

## По-русски

Пока отсутствуют **Live**, **Animation**, **Custom/Graph**, интерактивные векторные
скелеты OpenPose и пользовательские переопределения пресетов ControlNet.
Частично перенесены диагностика/лимиты ControlNet, сложная логика регионов,
групповые действия истории, автоматические переходы маски и полное соответствие
всех backend-ов. Растровая поза и препроцессор уже есть; инпейнт-контекст и
диффузионный апскейл тоже уже есть. Локальный установщик ComfyUI в APK не входит
в согласованную цель — генерация на планшете использует удалённый сервер.
