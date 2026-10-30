#!/usr/bin/env python3
import sys
import threading
import time
import math
import struct
import spidev
import obd
import logging
from PyQt6.QtCore import QObject, QTimer, pyqtProperty, pyqtSignal, QUrl
from PyQt6.QtGui import QGuiApplication
from PyQt6.QtQml import QQmlApplicationEngine

# Suppress OBD2 library logging
obd.logger.setLevel(logging.CRITICAL)
logging.getLogger('obd').setLevel(logging.CRITICAL)

# KPH to MPH conversion factor
KPH_TO_MPH = 0.621371

# ==================== IMU READER (SPI) ====================
class IMUReader:
    def __init__(self):
        self.connected = False
        self.spi = None
        self._demo_start_time = time.time()
        
        try:
            self.spi = spidev.SpiDev()
            self.spi.open(0, 0)
            self.spi.max_speed_hz = 1000000
            self.spi.mode = 0
            self.reg_phi_theta = 0x70
            self.reg_psi = 0x71
            self.scale_factor = 91.02222
            
            # Test the connection
            test_data = self.spi.xfer2([0x00, self.reg_phi_theta])
            if test_data:
                self.connected = True
                print("✅ IMU connected via SPI")
            else:
                print("⚠️ IMU detected but no data")
                self.connected = False
            
        except Exception as e:
            print(f"⚠️ IMU not detected: {e}")
            print("   Using demo IMU data (status will show IMU OFF)")
            self.connected = False
        
    def read_euler(self):
        # If real IMU is connected, read real data
        if self.connected and self.spi:
            try:
                # Read roll and pitch
                self.spi.xfer2([0x00, self.reg_phi_theta])
                time.sleep(5e-6)
                rx_data = self.spi.xfer2([0x00] * 4)
                roll_raw, pitch_raw = struct.unpack('>hh', bytes(rx_data[0:4]))
                
                # Read yaw
                self.spi.xfer2([0x00, self.reg_psi])
                time.sleep(5e-6)
                rx_data = self.spi.xfer2([0x00] * 4)
                yaw_raw, _ = struct.unpack('>hh', bytes(rx_data[0:4]))
                
                # Convert to degrees
                roll = roll_raw / self.scale_factor
                pitch = pitch_raw / self.scale_factor
                yaw = yaw_raw / self.scale_factor
                
                return roll, pitch, yaw
            except Exception as e:
                print(f"IMU read error: {e}")
                return self._get_demo_data()
        else:
            # Use demo data when no physical IMU
            return self._get_demo_data()
    
    def _get_demo_data(self):
        """Generate smooth demo IMU data for testing"""
        t = time.time() - self._demo_start_time
        roll = math.sin(t * 0.3) * 30
        pitch = math.sin(t * 0.2) * 20
        yaw = (math.sin(t * 0.1) * 180)
        return roll, pitch, yaw
    
    def close(self):
        if self.spi:
            self.spi.close()

# ==================== OBD2 READER (Threaded, Non-blocking) ====================
class OBD2Reader(QObject):
    connectionChanged = pyqtSignal(bool)
    dataUpdated = pyqtSignal(int, int, int)  # rpm, speed_mph, fuel
    
    def __init__(self, port="/dev/serial/by-id/usb-www.vgatemall.com_vLinker_FS_D3C0KEN4-if00-port0", baudrate=115200):
        super().__init__()
        self.port = port
        self.baudrate = baudrate
        self.connected = False
        self.running = True
        self._rpm = 0
        self._speed_mph = 0
        self._fuel = 75
        self._fuel_smooth = 75
        self.connection = None
        self._counter = 0
        
        # Fuel smoothing factor
        self._fuel_smoothing = 0.98
        
        # Start connection thread
        self.thread = threading.Thread(target=self._run, daemon=True)
        self.thread.start()
    
    def _run(self):
        """Run in separate thread to avoid blocking"""
        try:
            print(f"🔌 Attempting OBD2 connection on {self.port} at {self.baudrate} baud...")
            self.connection = obd.OBD(
                portstr=self.port,
                baudrate=self.baudrate,
                fast=False,
                timeout=2
            )
            
            if self.connection and self.connection.is_connected():
                self.connected = True
                self.connectionChanged.emit(True)
                print(f"✅ OBD2 connected on {self.port}")
                
                while self.running:
                    try:
                        self._counter += 1
                        
                        # Read RPM
                        rpm_resp = self.connection.query(obd.commands.RPM)
                        if rpm_resp and rpm_resp.value is not None:
                            self._rpm = int(rpm_resp.value.magnitude)
                        
                        # Read Speed - Convert KPH to MPH
                        speed_resp = self.connection.query(obd.commands.SPEED)
                        if speed_resp and speed_resp.value is not None:
                            speed_kph = int(speed_resp.value.magnitude)
                            self._speed_mph = int(speed_kph * KPH_TO_MPH)
                        
                        # Read Fuel every 20 cycles with smoothing
                        if self._counter % 20 == 0:
                            fuel_resp = self.connection.query(obd.commands.FUEL_LEVEL)
                            if fuel_resp and fuel_resp.value is not None:
                                raw_fuel = int(fuel_resp.value.magnitude)
                                if 0 <= raw_fuel <= 100:
                                    self._fuel_smooth = (self._fuel_smoothing * self._fuel_smooth) + ((1 - self._fuel_smoothing) * raw_fuel)
                                    self._fuel = int(self._fuel_smooth)
                        
                        self.dataUpdated.emit(self._rpm, self._speed_mph, self._fuel)
                        
                    except Exception:
                        pass
                    
                    time.sleep(0.1)
            else:
                print(f"⚠️ No OBD2 device on {self.port}")
                
        except Exception as e:
            print(f"⚠️ OBD2 connection failed: {e}")
    
    def get_rpm(self):
        return self._rpm
    
    def get_speed(self):
        return self._speed_mph
    
    def get_fuel_level(self):
        return self._fuel
    
    def close(self):
        self.running = False
        if self.connection:
            self.connection.close()

# ==================== DATA PROVIDER ====================
class DataProvider(QObject):
    # Signals
    rpmChanged = pyqtSignal()
    speedChanged = pyqtSignal()
    fuelLevelChanged = pyqtSignal()
    obdConnectedChanged = pyqtSignal()
    rollChanged = pyqtSignal()
    pitchChanged = pyqtSignal()
    yawChanged = pyqtSignal()
    imuConnectedChanged = pyqtSignal()
    
    def __init__(self):
        super().__init__()
        
        # Initialize sensors
        self.obd = OBD2Reader()
        self.imu = IMUReader()
        
        # Connect signals
        self.obd.connectionChanged.connect(self.on_obd_connected)
        self.obd.dataUpdated.connect(self.on_obd_data)
        
        # Data values
        self._rpm = 0
        self._speed = 0
        self._fuel_level = 75
        self._obd_connected = False
        self._roll = 0.0
        self._pitch = 0.0
        self._yaw = 0.0
        self._imu_connected = self.imu.connected
        
        # Timer for updates (20 Hz)
        self.timer = QTimer()
        self.timer.timeout.connect(self.update_data)
        self.timer.start(50)
    
    def on_obd_connected(self, connected):
        self.obd_connected = connected
    
    def on_obd_data(self, rpm, speed_mph, fuel):
        """Update data from OBD2 thread"""
        self.rpm = rpm
        self.speed = speed_mph
        self.fuel_level = fuel
    
    def update_data(self):
        # Update IMU data
        r, p, y = self.imu.read_euler()
        self.roll = r
        self.pitch = p
        self.yaw = y
        
        # Update physical connection status
        self.imu_connected = self.imu.connected
        
        # If no OBD2 data, use demo data
        if not self._obd_connected:
            t = time.time()
            self._rpm = int(800 + (math.sin(t * 0.5) + 1) / 2 * 5700)
            self._speed = int(self._rpm / 80)
            self._fuel_level = 75
            
            self.rpmChanged.emit()
            self.speedChanged.emit()
            self.fuelLevelChanged.emit()
    
    # ==================== IMU PROPERTIES ====================
    @pyqtProperty(float, notify=rollChanged)
    def roll(self):
        return self._roll
    
    @roll.setter
    def roll(self, value):
        if abs(self._roll - value) > 0.01:
            self._roll = value
            self.rollChanged.emit()
    
    @pyqtProperty(float, notify=pitchChanged)
    def pitch(self):
        return self._pitch
    
    @pitch.setter
    def pitch(self, value):
        if abs(self._pitch - value) > 0.01:
            self._pitch = value
            self.pitchChanged.emit()
    
    @pyqtProperty(float, notify=yawChanged)
    def yaw(self):
        return self._yaw
    
    @yaw.setter
    def yaw(self, value):
        if abs(self._yaw - value) > 0.01:
            self._yaw = value
            self.yawChanged.emit()
    
    @pyqtProperty(bool, notify=imuConnectedChanged)
    def imu_connected(self):
        return self._imu_connected
    
    @imu_connected.setter
    def imu_connected(self, value):
        if self._imu_connected != value:
            self._imu_connected = value
            self.imuConnectedChanged.emit()
    
    # ==================== OBD2 PROPERTIES ====================
    @pyqtProperty(int, notify=rpmChanged)
    def rpm(self):
        return self._rpm
    
    @rpm.setter
    def rpm(self, value):
        if self._rpm != value:
            self._rpm = value
            self.rpmChanged.emit()
    
    @pyqtProperty(int, notify=speedChanged)
    def speed(self):
        return self._speed
    
    @speed.setter
    def speed(self, value):
        if self._speed != value:
            self._speed = value
            self.speedChanged.emit()
    
    @pyqtProperty(int, notify=fuelLevelChanged)
    def fuel_level(self):
        return int(self._fuel_level)
    
    @fuel_level.setter
    def fuel_level(self, value):
        int_value = int(value)
        if self._fuel_level != int_value:
            self._fuel_level = int_value
            self.fuelLevelChanged.emit()
    
    @pyqtProperty(bool, notify=obdConnectedChanged)
    def obd_connected(self):
        return self._obd_connected
    
    @obd_connected.setter
    def obd_connected(self, value):
        if self._obd_connected != value:
            self._obd_connected = value
            self.obdConnectedChanged.emit()
    
    def cleanup(self):
        self.obd.close()
        self.imu.close()

# ==================== MAIN APPLICATION ====================
def main():
    app = QGuiApplication(sys.argv)
    engine = QQmlApplicationEngine()
    
    provider = DataProvider()
    engine.rootContext().setContextProperty("dataProvider", provider)
    
    engine.load(QUrl.fromLocalFile("main.qml"))
    
    if not engine.rootObjects():
        print("Failed to load QML file")
        sys.exit(-1)
    
    app.aboutToQuit.connect(provider.cleanup)
    
    print("=" * 50)
    print("BUILT2RACE x CSULA - IMU + OBD2 DASHBOARD")
    print("=" * 50)
    print("✅ Dashboard started")
    print(f"   - IMU Physical Status: {'CONNECTED' if provider.imu.connected else 'NOT CONNECTED'}")
    print(f"   - OBD2: {'Connected' if provider.obd.connected else 'Demo mode'}")
    print("=" * 50)
    
    sys.exit(app.exec())

if __name__ == "__main__":
    main()