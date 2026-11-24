#!/usr/bin/env python3
"""GUI Windows untuk menyinkronkan RTC DS3231 melalui serial."""

import queue
import sys
import threading
from datetime import datetime
from pathlib import Path

import serial
from PySide6.QtCore import QTimer
from PySide6.QtGui import QCloseEvent, QFont, QIcon
from PySide6.QtWidgets import (
    QApplication,
    QComboBox,
    QGridLayout,
    QGroupBox,
    QHBoxLayout,
    QLabel,
    QLineEdit,
    QMainWindow,
    QMessageBox,
    QPlainTextEdit,
    QPushButton,
    QVBoxLayout,
    QWidget,
)
from serial.tools import list_ports


def resource_path(relative_path: str) -> str:
    """Resolve aset saat dijalankan dari source maupun bundle PyInstaller."""
    bundle_directory = getattr(sys, "_MEIPASS", None)
    base_directory = (
        Path(bundle_directory)
        if bundle_directory
        else Path(__file__).resolve().parents[1]
    )
    return str(base_directory / relative_path)


class RtcSerialGui(QMainWindow):
    BAUD_RATE = 115200

    def __init__(self) -> None:
        super().__init__()
        self.setWindowTitle("RTC DS3231 Serial Sync")
        self.setWindowIcon(QIcon(resource_path("assets/rtc-sync.ico")))
        self.resize(680, 520)
        self.setMinimumSize(600, 450)

        self.connection: serial.Serial | None = None
        self.reader_thread: threading.Thread | None = None
        self.stop_event = threading.Event()
        self.messages: queue.Queue[tuple[str, str]] = queue.Queue()

        self._build_interface()
        self.refresh_ports()
        self._fill_current_time()
        self._set_connected_state(False)

        self.message_timer = QTimer(self)
        self.message_timer.timeout.connect(self._poll_messages)
        self.message_timer.start(100)

    def _build_interface(self) -> None:
        container = QWidget()
        main_layout = QVBoxLayout(container)
        main_layout.setContentsMargins(14, 14, 14, 14)
        self.setCentralWidget(container)

        connection_group = QGroupBox("Koneksi")
        connection_layout = QGridLayout(connection_group)
        connection_layout.addWidget(QLabel("Port serial"), 0, 0)
        self.port_combo = QComboBox()
        connection_layout.addWidget(self.port_combo, 0, 1)
        refresh_button = QPushButton("Refresh")
        refresh_button.clicked.connect(self.refresh_ports)
        connection_layout.addWidget(refresh_button, 0, 2)
        self.connect_button = QPushButton("Connect")
        self.connect_button.clicked.connect(self.toggle_connection)
        connection_layout.addWidget(self.connect_button, 0, 3)
        self.status_label = QLabel("Belum terhubung")
        connection_layout.addWidget(self.status_label, 1, 0, 1, 4)
        main_layout.addWidget(connection_group)

        sync_group = QGroupBox("Atur waktu RTC")
        sync_layout = QGridLayout(sync_group)
        self.auto_button = QPushButton("Sinkronkan dengan waktu komputer")
        self.auto_button.clicked.connect(self.sync_automatically)
        sync_layout.addWidget(self.auto_button, 0, 0, 1, 3)

        sync_layout.addWidget(QLabel("Tanggal"), 1, 0)
        self.date_entry = QLineEdit()
        sync_layout.addWidget(self.date_entry, 1, 1)
        sync_layout.addWidget(QLabel("YYYY-MM-DD"), 1, 2)

        sync_layout.addWidget(QLabel("Waktu"), 2, 0)
        self.time_entry = QLineEdit()
        sync_layout.addWidget(self.time_entry, 2, 1)
        sync_layout.addWidget(QLabel("HH:MM:SS"), 2, 2)

        button_layout = QHBoxLayout()
        self.manual_button = QPushButton("Kirim waktu manual")
        self.manual_button.clicked.connect(self.sync_manually)
        button_layout.addWidget(self.manual_button)
        self.now_button = QPushButton("Baca RTC")
        self.now_button.clicked.connect(lambda: self.send_command("now"))
        button_layout.addWidget(self.now_button)
        sync_layout.addLayout(button_layout, 3, 0, 1, 3)
        main_layout.addWidget(sync_group)

        log_group = QGroupBox("Log serial")
        log_layout = QVBoxLayout(log_group)
        self.log = QPlainTextEdit()
        self.log.setReadOnly(True)
        self.log.setFont(QFont("Consolas", 9))
        log_layout.addWidget(self.log)
        main_layout.addWidget(log_group, 1)

    def refresh_ports(self) -> None:
        selected = self._selected_device()
        self.port_combo.clear()
        ports = list(list_ports.comports())
        for port in ports:
            self.port_combo.addItem(f"{port.device} — {port.description}", port.device)
        if selected:
            index = self.port_combo.findData(selected)
            if index >= 0:
                self.port_combo.setCurrentIndex(index)
        self._append_log(f"Ditemukan {len(ports)} port serial.")

    def _selected_device(self) -> str:
        return str(self.port_combo.currentData() or "")

    def toggle_connection(self) -> None:
        if self.connection and self.connection.is_open:
            self.disconnect()
        else:
            self.connect()

    def connect(self) -> None:
        port = self._selected_device()
        if not port:
            QMessageBox.warning(self, "Port belum dipilih", "Hubungkan board lalu klik Refresh.")
            return
        try:
            self.connection = serial.Serial(port, self.BAUD_RATE, timeout=0.2)
        except serial.SerialException as error:
            QMessageBox.critical(self, "Koneksi gagal", str(error))
            return

        self.stop_event.clear()
        self.reader_thread = threading.Thread(target=self._read_serial, daemon=True)
        self.reader_thread.start()
        self.status_label.setText(f"Terhubung ke {port} @ {self.BAUD_RATE} baud")
        self._append_log(f"Terhubung ke {port}.")
        self._set_connected_state(True)

    def disconnect(self) -> None:
        self.stop_event.set()
        connection = self.connection
        self.connection = None
        if connection and connection.is_open:
            connection.close()
        self.status_label.setText("Belum terhubung")
        self._append_log("Koneksi ditutup.")
        self._set_connected_state(False)

    def _read_serial(self) -> None:
        connection = self.connection
        if connection is None:
            return
        while not self.stop_event.is_set():
            try:
                data = connection.readline()
                if data:
                    text = data.decode("utf-8", errors="replace").rstrip()
                    self.messages.put(("data", text))
            except (serial.SerialException, OSError) as error:
                if not self.stop_event.is_set():
                    self.messages.put(("error", str(error)))
                return

    def _poll_messages(self) -> None:
        try:
            while True:
                message_type, text = self.messages.get_nowait()
                if message_type == "error":
                    self._append_log(f"ERROR: {text}")
                    QMessageBox.critical(self, "Koneksi terputus", text)
                    self.disconnect()
                else:
                    self._append_log(f"RTC> {text}")
        except queue.Empty:
            pass

    def _fill_current_time(self) -> None:
        current = datetime.now()
        self.date_entry.setText(current.strftime("%Y-%m-%d"))
        self.time_entry.setText(current.strftime("%H:%M:%S"))

    def sync_automatically(self) -> None:
        current = datetime.now()
        self.date_entry.setText(current.strftime("%Y-%m-%d"))
        self.time_entry.setText(current.strftime("%H:%M:%S"))
        self.send_command(f"set {current:%Y-%m-%d %H:%M:%S}")

    def sync_manually(self) -> None:
        value = f"{self.date_entry.text().strip()} {self.time_entry.text().strip()}"
        try:
            parsed = datetime.strptime(value, "%Y-%m-%d %H:%M:%S")
        except ValueError:
            QMessageBox.warning(
                self, "Format tidak valid", "Gunakan format YYYY-MM-DD dan HH:MM:SS."
            )
            return
        if not 2000 <= parsed.year <= 2099:
            QMessageBox.warning(self, "Tahun tidak valid", "Tahun harus antara 2000 dan 2099.")
            return
        self.send_command(f"set {parsed:%Y-%m-%d %H:%M:%S}")

    def send_command(self, command: str) -> None:
        connection = self.connection
        if not connection or not connection.is_open:
            QMessageBox.warning(self, "Belum terhubung", "Pilih port lalu klik Connect.")
            return
        try:
            connection.write((command + "\n").encode("ascii"))
            connection.flush()
            self._append_log(f"PC> {command}")
        except serial.SerialException as error:
            QMessageBox.critical(self, "Gagal mengirim", str(error))
            self.disconnect()

    def _set_connected_state(self, connected: bool) -> None:
        self.auto_button.setEnabled(connected)
        self.manual_button.setEnabled(connected)
        self.now_button.setEnabled(connected)
        self.port_combo.setEnabled(not connected)
        self.connect_button.setText("Disconnect" if connected else "Connect")

    def _append_log(self, text: str) -> None:
        timestamp = datetime.now().strftime("%H:%M:%S")
        self.log.appendPlainText(f"[{timestamp}] {text}")

    def closeEvent(self, event: QCloseEvent) -> None:
        self.disconnect()
        event.accept()


def main() -> int:
    application = QApplication(sys.argv)
    application.setWindowIcon(QIcon(resource_path("assets/rtc-sync.ico")))
    window = RtcSerialGui()
    window.show()
    return application.exec()


if __name__ == "__main__":
    raise SystemExit(main())
