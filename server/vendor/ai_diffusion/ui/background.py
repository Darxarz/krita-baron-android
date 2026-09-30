from PyQt5.QtCore import QMetaObject, Qt
from PyQt5.QtWidgets import (
    QCheckBox,
    QComboBox,
    QDoubleSpinBox,
    QFormLayout,
    QGroupBox,
    QHBoxLayout,
    QLabel,
    QLineEdit,
    QProgressBar,
    QPushButton,
    QScrollArea,
    QSpinBox,
    QVBoxLayout,
    QWidget,
)

from ..backend.background import available_models
from ..localization import translate as _
from ..model.model import DocumentModel
from ..model.properties import Bind, Binding, bind, bind_combo, bind_toggle
from ..model.root import root
from .widget import ErrorBox, WorkspaceSelectWidget


class BackgroundWidget(QWidget):
    def __init__(self):
        super().__init__()
        self._model = root.active_model
        self._bindings: list[QMetaObject.Connection | Binding] = []
        outer = QVBoxLayout(self)
        outer.setContentsMargins(0, 0, 0, 0)
        scroll = QScrollArea(self)
        scroll.setWidgetResizable(True)
        scroll.setFrameShape(QScrollArea.NoFrame)
        scroll.setHorizontalScrollBarPolicy(Qt.ScrollBarPolicy.ScrollBarAlwaysOff)
        content = QWidget()
        scroll.setWidget(content)
        outer.addWidget(scroll)
        layout = QVBoxLayout(content)
        layout.setContentsMargins(0, 2, 4, 0)
        header = QHBoxLayout()
        self.workspace_select = WorkspaceSelectWidget(self)
        header.addWidget(self.workspace_select)
        header.addWidget(QLabel(_("Separate background")), 1)
        layout.addLayout(header)
        help_text = QLabel(_("Extract the character with an editable mask. The original is kept."))
        help_text.setWordWrap(True)
        layout.addWidget(help_text)
        form = QFormLayout()
        self.source_select = QComboBox()
        self.source_select.addItem(_("Visible canvas"), "canvas")
        self.source_select.addItem(_("Active layer or group"), "layer")
        form.addRow(_("Source"), self.source_select)
        self.model_select = QComboBox()
        form.addRow(_("Edge model"), self.model_select)
        self.detector_select = QComboBox()
        form.addRow(_("Subject detector"), self.detector_select)
        for combo in (self.source_select, self.model_select, self.detector_select):
            combo.setSizeAdjustPolicy(QComboBox.AdjustToMinimumContentsLengthWithIcon)
            combo.setMinimumContentsLength(10)
        layout.addLayout(form)
        self.isolate = QCheckBox(_("Keep only the described subject"))
        layout.addWidget(self.isolate)
        self.subject = QLineEdit()
        self.exclude = QLineEdit()
        self.subject.setPlaceholderText("person / character / cat")
        self.exclude.setPlaceholderText("chair . table")
        self.subject.setToolTip(
            _("Use English object names. Separate multiple objects with a dot.")
        )
        self.exclude.setToolTip(self.subject.toolTip())
        descriptions = QFormLayout()
        self.subject_label = QLabel(_("Keep"))
        self.exclude_label = QLabel(_("Exclude"))
        descriptions.addRow(self.subject_label, self.subject)
        descriptions.addRow(self.exclude_label, self.exclude)
        layout.addLayout(descriptions)
        self.refine = QCheckBox(_("Clean background color from edges"))
        self.refine_subject = QCheckBox(_("Refine the character contour with SAM"))
        self.hide_original = QCheckBox(_("Hide original after separation"))
        self.create_background = QCheckBox(_("Also create a background layer"))
        for widget in (
            self.refine_subject,
            self.refine,
            self.hide_original,
            self.create_background,
        ):
            layout.addWidget(widget)
        advanced = QGroupBox(_("Edge settings"))
        details = QFormLayout(advanced)
        self.threshold = QDoubleSpinBox()
        self.threshold.setRange(0.05, 0.95)
        self.threshold.setSingleStep(0.05)
        self.margin = QSpinBox()
        self.margin.setRange(0, 32)
        self.margin.setSuffix(" px")
        self.margin.setToolTip(
            _("Room around the detected subject to retain hair and thin details.")
        )
        self.resolution = QComboBox()
        for size in (512, 1024, 1536, 2048):
            self.resolution.addItem(str(size), size)
        self.resolution.setToolTip(
            _("RMBG resolution. BiRefNet uses its model's native resolution.")
        )
        self.offset = QSpinBox()
        self.offset.setRange(-20, 20)
        self.offset.setSuffix(" px")
        self.blur = QSpinBox()
        self.blur.setRange(0, 8)
        self.blur.setSuffix(" px")
        details.addRow(_("Detection threshold"), self.threshold)
        details.addRow(_("Hair detail margin"), self.margin)
        details.addRow(_("RMBG resolution"), self.resolution)
        details.addRow(_("Expand / shrink edge"), self.offset)
        details.addRow(_("Edge softness"), self.blur)
        advanced_toggle = QPushButton(_("Edge settings"))
        advanced_toggle.setCheckable(True)
        advanced_toggle.toggled.connect(advanced.setVisible)
        layout.addWidget(advanced_toggle)
        advanced.hide()
        layout.addWidget(advanced)
        self.notice = QLabel()
        self.notice.setWordWrap(True)
        layout.addWidget(self.notice)
        self.run_button = QPushButton(_("Separate character"))
        self.run_button.clicked.connect(self._run)
        layout.addWidget(self.run_button)
        self.progress = QProgressBar()
        self.progress.setTextVisible(False)
        self.progress.setFixedHeight(6)
        layout.addWidget(self.progress)
        self.errors = ErrorBox(self)
        layout.addWidget(self.errors)
        layout.addStretch()
        self.isolate.toggled.connect(self._update_enabled)
        self.model_select.currentIndexChanged.connect(self._update_enabled)
        self.detector_select.currentIndexChanged.connect(self._update_enabled)
        root.connection.state_changed.connect(self.update_models)
        root.connection.models_changed.connect(self.update_models)
        self.update_models()
        self._bind_model()

    @property
    def model(self):
        return self._model

    @model.setter
    def model(self, model: DocumentModel):
        if model is not self._model:
            Binding.disconnect_all(self._bindings)
            self._model = model
            self._bind_model()

    def _bind_model(self):
        m, b = self._model, self._model.background
        self._bindings = [
            bind(m, "workspace", self.workspace_select, "value", Bind.one_way),
            bind_combo(b, "model", self.model_select),
            bind_combo(b, "subject_detector", self.detector_select),
            bind_combo(b, "source", self.source_select),
            bind_combo(b, "process_resolution", self.resolution),
            bind_toggle(b, "isolate_subject", self.isolate),
            bind_toggle(b, "refine_foreground", self.refine),
            bind_toggle(b, "refine_subject", self.refine_subject),
            bind_toggle(b, "hide_original", self.hide_original),
            bind_toggle(b, "create_background", self.create_background),
            bind(b, "subject", self.subject, "text"),
            bind(b, "exclude", self.exclude, "text"),
            bind(b, "threshold", self.threshold, "value"),
            bind(b, "detail_margin", self.margin, "value"),
            bind(b, "edge_offset", self.offset, "value"),
            bind(b, "edge_blur", self.blur, "value"),
            b.in_progress_changed.connect(self._update_enabled),
            m.progress_changed.connect(self._update_progress),
        ]
        self.errors.model = m
        self._update_enabled()

    def update_models(self):
        selected = self._model.background.model
        self.model_select.blockSignals(True)
        self.model_select.clear()
        client = root.connection.client_if_connected
        if client:
            for name in available_models(client.models.node_inputs):
                self.model_select.addItem(name, name)
        index = self.model_select.findData(selected)
        self.model_select.setCurrentIndex(index)
        self.model_select.blockSignals(False)
        detector = self._model.background.subject_detector
        self.detector_select.blockSignals(True)
        self.detector_select.clear()
        if client:
            detectors = [
                name
                for name in client.models.node_inputs.options(
                    "UltralyticsDetectorProvider", "model_name"
                )
                if name.startswith("segm/")
            ]
            for name in detectors:
                label = _("Person") if len(detectors) == 1 else _("Person") + f" ({name})"
                self.detector_select.addItem(label, name)
            if "Segment" in client.models.node_inputs:
                self.detector_select.addItem(_("Text description (requires GroundingDINO)"), "text")
        self.detector_select.setCurrentIndex(self.detector_select.findData(detector))
        self.detector_select.blockSignals(False)
        self._update_enabled()

    def _update_enabled(self, *args):
        use_text = self.detector_select.currentData() == "text"
        for widget in (self.subject, self.exclude, self.subject_label, self.exclude_label):
            widget.setVisible(self.isolate.isChecked() and use_text)
        self.subject.setEnabled(self.isolate.isChecked() and use_text)
        self.exclude.setEnabled(self.isolate.isChecked() and use_text)
        self.detector_select.setEnabled(self.isolate.isChecked())
        self.refine_subject.setEnabled(self.isolate.isChecked() and not use_text)
        self.threshold.setEnabled(self.isolate.isChecked())
        self.margin.setEnabled(self.isolate.isChecked())
        name = str(self.model_select.currentData() or "")
        self.resolution.setEnabled(bool(name) and not name.startswith("BiRefNet"))
        supported = bool(name) and (
            not self.isolate.isChecked() or self.detector_select.currentIndex() >= 0
        )
        self.run_button.setEnabled(supported and not self._model.background.in_progress)
        self.notice.setText(
            _(
                "Text isolation needs the GroundingDINO Python package and models on the server. The installed node alone is not enough."
            )
            if use_text and self.isolate.isChecked()
            else _(
                "First use may download the selected models on ComfyUI. The background layer has a hole where the character was."
            )
            if supported
            else _("This mode requires ComfyUI-RMBG on your ComfyUI server.")
        )

    def _update_progress(self, value: float):
        self.progress.setRange(0, 0 if value < 0 else 100)
        self.progress.setValue(round(value * 100) if value >= 0 else 0)

    def _run(self):
        self._model.background.remove_background()
