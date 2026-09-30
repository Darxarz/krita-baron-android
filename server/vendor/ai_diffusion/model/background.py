from __future__ import annotations

from dataclasses import dataclass
from typing import TYPE_CHECKING

from PyQt5.QtCore import QObject, QUuid, pyqtSignal
from PyQt5.QtGui import QImage

from .. import eventloop, util
from ..backend import background
from ..backend.api import BackgroundRemovalInput
from ..image import Bounds, Image
from ..localization import translate as _
from .jobs import Job, JobKind, JobParams
from .properties import ObservableProperties, Property

if TYPE_CHECKING:
    from .model import DocumentModel


@dataclass
class _Source:
    image: Image
    hide_layers: list[QUuid]
    hide_original: bool
    create_background: bool


def result_mask(mask_image: Image, source: Image):
    mask = Image(mask_image._qimage.convertToFormat(QImage.Format.Format_Grayscale8))
    alpha = source._qimage.convertToFormat(QImage.Format.Format_Alpha8)
    alpha.reinterpretAsFormat(QImage.Format.Format_Grayscale8)
    return Image.mask_multiply(mask, Image(alpha))


class BackgroundWorkspace(QObject, ObservableProperties):
    model = Property("BiRefNet-HR-matting", persist=True)
    source = Property("canvas", persist=True)
    isolate_subject = Property(True, persist=True)
    subject = Property("person", persist=True)
    exclude = Property("chair", persist=True)
    threshold = Property(0.3, persist=True)
    detail_margin = Property(8, persist=True)
    process_resolution = Property(1536, persist=True)
    edge_offset = Property(0, persist=True)
    edge_blur = Property(0, persist=True)
    refine_foreground = Property(True, persist=True)
    hide_original = Property(True, persist=True)
    create_background = Property(True, persist=True)
    subject_detector = Property("segm/person_yolov8m-seg.pt", persist=True)
    refine_subject = Property(False, persist=True)
    sam_model = Property("sam_vit_b_01ec64.pth", persist=True)
    in_progress = Property(False)

    model_changed = pyqtSignal(str)
    source_changed = pyqtSignal(str)
    isolate_subject_changed = pyqtSignal(bool)
    subject_changed = pyqtSignal(str)
    exclude_changed = pyqtSignal(str)
    threshold_changed = pyqtSignal(float)
    detail_margin_changed = pyqtSignal(int)
    process_resolution_changed = pyqtSignal(int)
    edge_offset_changed = pyqtSignal(int)
    edge_blur_changed = pyqtSignal(int)
    refine_foreground_changed = pyqtSignal(bool)
    hide_original_changed = pyqtSignal(bool)
    create_background_changed = pyqtSignal(bool)
    subject_detector_changed = pyqtSignal(str)
    refine_subject_changed = pyqtSignal(bool)
    sam_model_changed = pyqtSignal(str)
    in_progress_changed = pyqtSignal(bool)
    modified = pyqtSignal(QObject, str)

    def __init__(self, model: DocumentModel):
        super().__init__()
        self._document_model = model
        self._sources: dict[int, _Source] = {}

    def remove_background(self):
        if self.in_progress:
            return
        m = self._document_model
        job = None
        try:
            ok, error = m.document.check_color_mode()
            if not ok:
                raise ValueError(error or _("Unsupported document color mode"))
            bounds = Bounds(0, 0, *m.document.extent)
            if self.source == "layer":
                layer = m.layers.active
                if layer.type.is_mask and layer.parent_layer:
                    layer = layer.parent_layer
                if layer.is_root or not layer.type.is_image:
                    raise ValueError(_("Select a paint layer or group to process"))
                image = layer.get_pixels(bounds)
                hide_layers = [layer.id]
            else:
                image = m._get_current_image(bounds)
                hide_layers = [l.id for l in m.layers.root.child_layers if l.is_visible]
            params = BackgroundRemovalInput(
                self.model,
                self.isolate_subject,
                self.subject,
                self.exclude,
                self.threshold,
                self.detail_margin,
                self.process_resolution,
                self.edge_offset,
                self.edge_blur,
                self.refine_foreground,
                self.subject_detector,
                refine_subject=self.refine_subject,
                sam_model=self.sam_model,
            )
            request = background.prepare(image, params, m._connection.client.models.node_inputs)
            job = m.jobs.add(
                JobKind.background_removal, JobParams(bounds, _("Separate background"))
            )
            self._sources[id(job)] = _Source(
                image, hide_layers, self.hide_original, self.create_background
            )
            self.in_progress = True
            m.clear_error()
            eventloop.run(self._enqueue(job, request))
        except Exception as e:
            if job:
                self.finish(job)
                m.jobs.notify_cancelled(job)
            m.report_error(util.log_error(e))

    async def _enqueue(self, job, request):
        try:
            await self._document_model._enqueue_job(job, request)
        except Exception as e:
            self.finish(job)
            self._document_model.jobs.notify_cancelled(job)
            self._document_model.report_error(util.log_error(e))

    def apply(self, job: Job):
        source = self._sources.get(id(job))
        if source is None:
            raise RuntimeError(_("The source for background separation is no longer available"))
        if len(job.results) != 2 or any(i.extent != source.image.extent for i in job.results):
            raise RuntimeError(_("Background separation returned an invalid image or mask"))
        if self._document_model.document.extent != source.image.extent:
            raise RuntimeError(_("The canvas size changed during separation. Run it again."))
        rgb, mask_image = job.results
        mask = result_mask(mask_image, source.image)
        if mask.average() < 0.0001:
            raise RuntimeError(
                _("No subject found. Change the description or disable subject isolation.")
            )
        m = self._document_model
        m.hide_preview()
        group = m.layers.create_group(_("Separated character"))
        if source.create_background:
            bg = m.layers.create(
                _("Background (with subject hole)"), source.image, job.params.bounds, parent=group
            )
            inverse = Image(mask._qimage.copy())
            inverse.invert()
            m.layers.create_mask(_("Background mask"), inverse, job.params.bounds, parent=bg)
            bg.hide()
        foreground = m.layers.create(_("Character"), rgb, job.params.bounds, parent=group)
        m.layers.create_mask(
            _("Character mask — editable"), mask, job.params.bounds, parent=foreground
        )
        if source.hide_original:
            for layer_id in source.hide_layers:
                if layer := m.layers.find(layer_id):
                    layer.hide()
        m.layers.active = foreground
        foreground.refresh()

    def finish(self, job: Job):
        self._sources.pop(id(job), None)
        self.in_progress = bool(self._sources)
