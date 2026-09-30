from __future__ import annotations

import asyncio
from time import time
from urllib.parse import urlsplit

from PyQt5.QtCore import QSize, Qt, QTimer
from PyQt5.QtGui import QIcon, QPixmap
from PyQt5.QtWidgets import (
    QComboBox,
    QDialog,
    QHBoxLayout,
    QLabel,
    QLineEdit,
    QListWidget,
    QListWidgetItem,
    QPushButton,
    QVBoxLayout,
    QWidget,
)

from .. import eventloop
from ..backend.client import resolve_arch
from ..backend.network import NetworkError, RequestManager
from ..backend.orchestrion_client import (
    OrchestrionAPI,
    OrchestrionClient,
    estimate_prompt,
    lora_compatible,
    protect_token,
    quote_text,
    service_url,
)
from ..localization import translate as _
from ..model.connection import ConnectionState
from ..model.jobs import JobKind
from ..model.root import root
from ..settings import settings
from ..style import Styles
from . import theme

_panel_style = """
QWidget#orchestrionPanel, QDialog#orchestrionGallery {background:#17232b; border-radius:12px;}
QLabel {color:#e0ebec;}
QLineEdit, QComboBox {background:#24353f; color:#e0ebec; border:1px solid #45606a; border-radius:6px; padding:7px;}
QLineEdit:disabled {color:#a4b8bc;}
QComboBox QAbstractItemView {background:#24353f; color:#e0ebec; selection-background-color:#32634f;}
QPushButton {background:#2c414b; color:#e0ebec; border:1px solid #45606a; border-radius:7px; padding:8px;}
QPushButton:hover {background:#34545c; border-color:#82cdb6;}
QPushButton:disabled {color:#91a4aa; border-color:#34464f; background:#24313a;}
QPushButton#orchestrionLogin {background:#82cdb6; color:#102820; font-weight:bold;}
QListWidget {background:#17232b; color:#e0ebec; border:0;}
QListWidget::item {background:#24353f; border:1px solid #344b56; border-radius:10px;}
QListWidget::item:selected {background:#2d514c; border:1px solid #82cdb6;}
"""


class OrchestrionWidget(QWidget):
    def __init__(self, parent=None):
        super().__init__(parent)
        self.setObjectName("orchestrionPanel")
        self.setStyleSheet(_panel_style)
        layout = QVBoxLayout(self)
        title = QLabel("Orchestrion × Baron Edition", self)
        title.setStyleSheet("font-size:20px;font-weight:bold;color:#82cdb6")
        layout.addWidget(title)
        description = QLabel(
            _("Your workshop: browser login, balance, prices and model catalog."), self
        )
        description.setWordWrap(True)
        layout.addWidget(description)
        self.url = QLineEdit(settings.orchestrion_url, self)
        self.url.setPlaceholderText("https://orchestrion.su")
        layout.addWidget(QLabel(_("Orchestrion address"), self))
        layout.addWidget(self.url)
        self.status = QLabel(self)
        self.status.setTextFormat(Qt.TextFormat.PlainText)
        self.status.setWordWrap(True)
        layout.addWidget(self.status)
        self.code = QLabel(self)
        self.code.setTextInteractionFlags(Qt.TextInteractionFlag.TextSelectableByMouse)
        self.code.setStyleSheet("font-size:26px;letter-spacing:3px;font-weight:bold;color:#82cdb6")
        layout.addWidget(self.code)
        self.connect_button = QPushButton(_("Sign in through browser"), self)
        self.connect_button.setObjectName("orchestrionLogin")
        self.connect_button.setMinimumHeight(38)
        self.connect_button.clicked.connect(self._connect)
        layout.addWidget(self.connect_button)
        self.cancel_button = QPushButton(_("Cancel sign-in"), self)
        self.cancel_button.clicked.connect(root.connection.cancel_sign_in)
        layout.addWidget(self.cancel_button)
        actions = QHBoxLayout()
        self.gallery_button = QPushButton(_("Model gallery"), self)
        self.gallery_button.clicked.connect(self._open_gallery)
        actions.addWidget(self.gallery_button)
        self.refresh_button = QPushButton(_("Refresh balance"), self)
        self.refresh_button.clicked.connect(self._refresh)
        actions.addWidget(self.refresh_button)
        self.logout_button = QPushButton(_("Sign out"), self)
        self.logout_button.clicked.connect(self._sign_out)
        actions.addWidget(self.logout_button)
        layout.addLayout(actions)
        layout.addStretch()
        root.connection.sign_in_code_changed.connect(self.code.setText)
        root.connection.state_changed.connect(self.update_connection_state)
        self.update_connection_state(root.connection.state)

    def _open_gallery(self):
        ModelGallery(self).exec()

    def _sign_out(self):
        eventloop.run(self._logout())

    def _connect(self):
        try:
            url = service_url(self.url.text())
            if url != settings.orchestrion_url:
                settings.orchestrion_token = ""
            settings.orchestrion_url = url
            settings.save()
            if settings.orchestrion_token:
                if client := root.connection.create_client(settings):
                    root.connection.connect(client)
            else:
                root.connection.sign_in_orchestrion()
        except ValueError as e:
            self.status.setText(str(e))

    def _refresh(self):
        async def refresh():
            if isinstance(client := root.connection.client_if_connected, OrchestrionClient):
                try:
                    await client.refresh_account()
                    self.update_connection_state(root.connection.state)
                except Exception as e:
                    self.status.setText(str(e))

        eventloop.run(refresh())

    async def _logout(self):
        if settings.orchestrion_token:
            try:
                client = root.connection.client_if_connected
                api = (
                    client.api
                    if isinstance(client, OrchestrionClient)
                    else OrchestrionAPI(
                        settings.orchestrion_url,
                        protect_token(settings.orchestrion_token, decrypt=True),
                    )
                )
                await api.logout()
            except Exception as e:
                if not isinstance(e, NetworkError) or e.status != 401:
                    self.status.setText(
                        _(
                            "Could not revoke the website connection: {error}. Try signing out again.",
                            error=str(e),
                        )
                    )
                    return
        settings.orchestrion_token = ""
        settings.save()
        await root.connection.disconnect()

    def update_connection_state(self, state: ConnectionState):
        if settings.server_mode.name != "orchestrion":
            return
        client = root.connection.client_if_connected
        connected = state is ConnectionState.connected and isinstance(client, OrchestrionClient)
        pending = state in (ConnectionState.auth_pending, ConnectionState.auth_requesting)
        busy = pending or state in (ConnectionState.connecting, ConnectionState.discover_models)
        self.url.setEnabled(not busy and not connected)
        self.connect_button.setVisible(not connected)
        self.connect_button.setEnabled(not busy)
        self.connect_button.setText(
            _("Connect") if settings.orchestrion_token else _("Sign in through browser")
        )
        self.cancel_button.setVisible(pending)
        self.code.setVisible(pending)
        for button in (self.gallery_button, self.refresh_button):
            button.setEnabled(connected)
        self.logout_button.setEnabled(bool(settings.orchestrion_token))
        if connected:
            assert isinstance(client, OrchestrionClient)
            self.status.setText(
                _(
                    "Connected · {name}\nBalance: {amount} bleatbucks",
                    name=client.account_data["name"],
                    amount=f"{client.account_data['coins']:g}",
                )
            )
        elif pending:
            self.status.setText(
                _("Sign in on the opened website, enter this code and approve the connection:")
            )
        elif state in (ConnectionState.error, ConnectionState.auth_error):
            self.status.setText(root.connection.error)
        elif busy:
            self.status.setText(_("Connecting and loading models…"))
        else:
            self.status.setText(
                _(
                    "Sign in on the website through your browser. Krita does not store your password."
                )
            )


class ModelGallery(QDialog):
    def __init__(self, parent=None):
        super().__init__(parent)
        self.setObjectName("orchestrionGallery")
        self.setStyleSheet(_panel_style)
        self.setWindowTitle(_("Orchestrion · Model gallery"))
        self.resize(780, 640)
        self._items: list[dict] = []
        self._tasks: list[asyncio.Task] = []
        self._icons: dict[str, QIcon] = {}
        self._network = RequestManager()  # Preview requests never include the device credential.
        self._semaphore = asyncio.Semaphore(4)
        layout = QVBoxLayout(self)
        self.search = QLineEdit(self)
        self.search.setPlaceholderText(_("Search for a model, family or LoRA…"))
        self.search.textChanged.connect(self._filter)
        layout.addWidget(self.search)
        self.kind = QComboBox(self)
        self.kind.addItem(_("Models"), "checkpoint")
        self.kind.addItem("LoRA", "lora")
        self.kind.currentIndexChanged.connect(self._filter)
        layout.addWidget(self.kind)
        self.list = QListWidget(self)
        self.list.setViewMode(QListWidget.ViewMode.IconMode)
        self.list.setResizeMode(QListWidget.ResizeMode.Adjust)
        self.list.setMovement(QListWidget.Movement.Static)
        self.list.setIconSize(QSize(150, 150))
        self.list.setGridSize(QSize(180, 220))
        self.list.setWordWrap(True)
        self.list.setSpacing(8)
        self.list.currentItemChanged.connect(self._selection)
        layout.addWidget(self.list, 1)
        self.details = QLabel(_("Loading catalog…"), self)
        self.details.setWordWrap(True)
        self.details.setTextFormat(Qt.TextFormat.PlainText)
        layout.addWidget(self.details)
        buttons = QHBoxLayout()
        self.select = QPushButton(_("Use in Krita"), self)
        self.select.setEnabled(False)
        self.select.clicked.connect(self._apply)
        buttons.addWidget(self.select)
        more = QPushButton(_("Show more"), self)
        more.clicked.connect(self._more)
        buttons.addWidget(more)
        close = QPushButton(_("Close"), self)
        close.clicked.connect(self.reject)
        buttons.addWidget(close)
        layout.addLayout(buttons)
        self._limit = 80
        self._tasks.append(eventloop.run(self._load()))

    async def _load(self):
        try:
            if isinstance(client := root.connection.client_if_connected, OrchestrionClient):
                self._items = await client.api.catalog()
                self._filter()
        except Exception as e:
            self.details.setText(str(e))

    def _more(self):
        self._limit += 80
        self._filter()

    def _filter(self):
        for task in self._tasks:
            if not task.done() and task.get_name() == "orchestrion-preview":
                task.cancel()
        self.list.clear()
        text = self.search.text().casefold()
        found = [
            m
            for m in self._items
            if m["kind"] == self.kind.currentData()
            and text in (m["title"] + " " + m["name"] + " " + m.get("family", "")).casefold()
        ]
        for model in found[: self._limit]:
            item = QListWidgetItem(model["title"] + "\n" + model.get("family", ""), self.list)
            item.setData(Qt.ItemDataRole.UserRole, model)
            item.setToolTip(model["name"])
            item.setIcon(self._icons.get(model.get("preview", ""), theme.icon("control-reference")))
            item.setSizeHint(QSize(180, 220))
            if model.get("preview") and model["preview"] not in self._icons:
                task = eventloop.run(self._preview(model["preview"], item))
                task.set_name("orchestrion-preview")
                self._tasks.append(task)
        self._tasks = [t for t in self._tasks if not t.done()]
        self.details.setText(
            _(
                "Found: {total} · shown: {shown}. Choose a card.",
                total=len(found),
                shown=min(len(found), self._limit),
            )
        )
        self.select.setEnabled(False)

    async def _preview(self, url: str, item: QListWidgetItem):
        # Only the website's public HTTPS CivitAI previews; no local/private URLs.
        parsed = urlsplit(url)
        if parsed.scheme != "https" or parsed.hostname != "image.civitai.com":
            return
        try:
            async with self._semaphore:
                data = await self._network.download(url, timeout=15)
            if len(data) > 8 * 1024 * 1024:
                return
            pixmap = QPixmap()
            if pixmap.loadFromData(data):
                icon = QIcon(
                    pixmap.scaled(
                        150,
                        150,
                        Qt.AspectRatioMode.KeepAspectRatio,
                        Qt.TransformationMode.SmoothTransformation,
                    )
                )
                if len(self._icons) < 256:
                    self._icons[url] = icon
                item.setIcon(icon)
        except (Exception, asyncio.CancelledError):
            return

    def _selection(self):
        if not (item := self.list.currentItem()):
            self.select.setEnabled(False)
            return
        model = item.data(Qt.ItemDataRole.UserRole)
        client = root.connection.client_if_connected
        compatible = isinstance(client, OrchestrionClient) and (
            model["name"]
            in (client.models.checkpoints if model["kind"] == "checkpoint" else client.models.loras)
        )
        if compatible and model["kind"] == "checkpoint":
            assert isinstance(client, OrchestrionClient)
            compatible = client.supports_arch(client.models.checkpoints[model["name"]].arch)
        if compatible and model["kind"] == "lora":
            compatible = (
                lora_compatible(model.get("family", ""), root.active_model.arch) is not False
            )
        self.select.setEnabled(bool(compatible))
        self.details.setText(
            model["name"]
            + (
                _("\nTrigger words: ") + ", ".join(model.get("triggers", []))
                if model.get("triggers")
                else ""
            )
            + (
                ""
                if compatible
                else _("\nThis model is unavailable for the current Krita connection.")
            )
        )

    def _apply(self):
        item = self.list.currentItem()
        client = root.connection.client_if_connected
        if not item or not isinstance(client, OrchestrionClient):
            return
        model = item.data(Qt.ItemDataRole.UserRole)
        styles = Styles.list()
        current = root.active_model.style
        if model["kind"] == "checkpoint":
            checkpoint = client.models.checkpoints.get(model["name"])
            if not checkpoint or not client.supports_arch(checkpoint.arch):
                return
            source = (
                current
                if resolve_arch(current, client) is checkpoint.arch
                else next((s for s in styles if resolve_arch(s, client) is checkpoint.arch), None)
            )
            style = styles.create("orchestrion.json", copy_from=source)
            style.checkpoints = [model["name"]]
            style.architecture = checkpoint.arch
        else:
            if model["name"] not in client.models.loras:
                return
            if lora_compatible(model.get("family", ""), root.active_model.arch) is False:
                return
            style = styles.create("orchestrion-lora.json", copy_from=current)
            style.loras = [lora for lora in style.loras if lora["name"] != model["name"]] + [
                {"name": model["name"], "strength": 1.0, "enabled": True}
            ]
        style.name = model["title"]
        style.save()
        styles.changed.emit()
        root.active_model.style = style
        self.accept()

    def done(self, a0: int):
        for task in self._tasks:
            task.cancel()
        super().done(a0)


class OrchestrionBar(QWidget):
    def __init__(self, kind: JobKind, parent=None):
        super().__init__(parent)
        self.kind = kind
        self.model = root.active_model
        self._task: asyncio.Task | None = None
        self._key = None
        self._quote: dict | None = None
        layout = QHBoxLayout(self)
        layout.setContentsMargins(0, 3, 0, 3)
        self.price = QLabel(_("Price: connect to Orchestrion"), self)
        self.price.setWordWrap(True)
        self.price.setTextFormat(Qt.TextFormat.PlainText)
        layout.addWidget(self.price, 1)
        self.gallery = QPushButton(_("Models"), self)
        self.gallery.clicked.connect(self._open_gallery)
        layout.addWidget(self.gallery)
        self._timer = QTimer(self)
        self._timer.setInterval(1500)
        self._timer.timeout.connect(self._update)
        self._timer.start()
        root.connection.state_changed.connect(self._visibility)
        self._visibility()

    def _open_gallery(self):
        ModelGallery(self).exec()

    def _visibility(self):
        self.setVisible(isinstance(root.connection.client_if_connected, OrchestrionClient))
        self._key = None
        self._quote = None
        self._update()

    def _update(self):
        if not self.isVisible() or (self._task and not self._task.done()):
            return
        client = root.connection.client_if_connected
        if not isinstance(client, OrchestrionClient):
            return
        try:
            if self.kind is JobKind.diffusion:
                work, *_unused = self.model._prepare_workflow(dryrun=True)
                count = self.model.batch_count
            else:
                work, _unused = self.model._prepare_upscale_image(dryrun=True)
                count = 1
            graph = estimate_prompt(work)
            key = (
                work.kind,
                str(work.images.extent) if work.images else "",
                work.batch_count,
                work.crop_upscale_extent,
                count,
                id(client),
                int(time() / 30),
            )
            if key == self._key and self._quote:
                return
            self._key = key
            self.price.setText(_("Estimating price…"))
            self._task = eventloop.run(self._fetch(client, graph, key, count))
        except Exception:
            self.price.setText(_("Choose a model and size to see the price"))

    async def _fetch(self, client: OrchestrionClient, graph: dict, key, count):
        try:
            quote = await client.api.quote(graph)
            if client is not root.connection.client_if_connected or self._key != key:
                return
            self._quote = quote
            self.price.setText(quote_text(quote, count))
            detail = _(
                "Balance: {amount} bleatbucks. Estimated total for the batch; charged after success. Final cost depends on output size.",
                amount=f"{quote['balance']:g}",
            )
            self.price.setToolTip(detail)
        except Exception:
            self._quote = None
            self.price.setText(_("Price temporarily unavailable · website rates apply"))
