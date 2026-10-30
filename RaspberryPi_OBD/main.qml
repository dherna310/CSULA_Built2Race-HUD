import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

ApplicationWindow {
    visible: true
    width: 1280
    height: 720
    title: "BUILT2RACE x CSULA - OBD2 Dashboard"
    color: "#000000"

    property string currentTime: new Date().toLocaleTimeString("en-US", {
        hour: "2-digit",
        minute: "2-digit",
        second: "2-digit",
        timeZoneName: "short"
    })
    property bool obdConnected: dataProvider ? dataProvider.obd_connected : false
    property bool imuConnected: dataProvider ? dataProvider.imu_connected : false

    Timer {
        interval: 1000
        running: true
        repeat: true
        onTriggered: currentTime = new Date().toLocaleTimeString("en-US", {
            hour: "2-digit",
            minute: "2-digit",
            second: "2-digit",
            timeZoneName: "short"
        })
    }

    // ==================== BACKGROUND IMAGE (Commented out) ====================
    // Image {
    //     anchors.fill: parent
    //     source: "file:///home/dash_arhud/IMU_OB2_Dashboard/background.jpg"
    //     fillMode: Image.PreserveAspectCrop
    //     opacity: 0.5
    //     z: -1
    // }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // ==================== TOP BAR ====================
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 100
            color: "#000000"

            RowLayout {
                anchors.fill: parent
                anchors.margins: 20
                spacing: 20

                // ==================== TOP LEFT: COMPASS (YAW) ====================
                Rectangle {
                    Layout.preferredWidth: 180
                    Layout.preferredHeight: 80
                    color: "transparent"
                    border.color: imuConnected ? "#d0312d" : "#444444"
                    border.width: 2
                    radius: 10
                    
                    Rectangle {
                        anchors.fill: parent
                        anchors.margins: 5
                        color: "#1a1a1a"
                        radius: 8
                        
                        Rectangle {
                            width: 60
                            height: 60
                            radius: 30
                            color: "#0a0a0a"
                            border.color: "#333333"
                            border.width: 1
                            anchors.centerIn: parent
                            
                            Rectangle {
                                width: 56
                                height: 56
                                radius: 28
                                color: "transparent"
                                anchors.centerIn: parent
                                rotation: dataProvider ? -dataProvider.yaw : 0
                                Behavior on rotation { 
                                    NumberAnimation { duration: 100 }
                                }
                                
                                Rectangle {
                                    width: 2
                                    height: 22
                                    color: "#d0312d"
                                    anchors.top: parent.top
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    anchors.topMargin: 4
                                }
                                
                                Rectangle {
                                    width: 2
                                    height: 15
                                    color: "white"
                                    anchors.bottom: parent.bottom
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    anchors.bottomMargin: 5
                                }
                                
                                Rectangle {
                                    width: 15
                                    height: 2
                                    color: "white"
                                    anchors.right: parent.right
                                    anchors.verticalCenter: parent.verticalCenter
                                    anchors.rightMargin: 5
                                }
                                
                                Rectangle {
                                    width: 15
                                    height: 2
                                    color: "white"
                                    anchors.left: parent.left
                                    anchors.verticalCenter: parent.verticalCenter
                                    anchors.leftMargin: 5
                                }
                                
                                Text {
                                    text: "N"
                                    color: "#d0312d"
                                    font.pixelSize: 9
                                    font.bold: true
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    anchors.top: parent.top
                                    anchors.topMargin: 26
                                }
                                
                                Text {
                                    text: "S"
                                    color: "white"
                                    font.pixelSize: 8
                                    font.bold: true
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    anchors.bottom: parent.bottom
                                    anchors.bottomMargin: 18
                                }
                                
                                Text {
                                    text: "E"
                                    color: "white"
                                    font.pixelSize: 8
                                    font.bold: true
                                    anchors.right: parent.right
                                    anchors.verticalCenter: parent.verticalCenter
                                    anchors.rightMargin: 16
                                }
                                
                                Text {
                                    text: "W"
                                    color: "white"
                                    font.pixelSize: 8
                                    font.bold: true
                                    anchors.left: parent.left
                                    anchors.verticalCenter: parent.verticalCenter
                                    anchors.leftMargin: 16
                                }
                            }
                            
                            Rectangle {
                                width: 5
                                height: 5
                                radius: 2.5
                                color: "#d0312d"
                                anchors.centerIn: parent
                            }
                        }
                        
                        Text {
                            anchors.bottom: parent.bottom
                            anchors.horizontalCenter: parent.horizontalCenter
                            anchors.bottomMargin: 5
                            text: dataProvider ? Math.round(dataProvider.yaw) + "°" : "---"
                            color: imuConnected ? "#d0312d" : "#444444"
                            font.pixelSize: 11
                            font.bold: true
                        }
                    }
                    
                    Text {
                        anchors.centerIn: parent
                        text: imuConnected ? "IMU 3D" : "IMU OFFLINE"
                        color: imuConnected ? "#d0312d" : "#444444"
                        font.pixelSize: 14
                        font.bold: true
                        visible: !imuConnected
                    }
                }

                // ==================== CENTER: BUILT2RACE LOGO ====================
                Item {
                    Layout.fillWidth: true
                    
                    RowLayout {
                        anchors.centerIn: parent
                        spacing: 0
                        Text { text: "BUILT"; color: "#d0312d"; font.pixelSize: 38; font.bold: true }
                        Text { text: "2"; color: "#d0312d"; font.pixelSize: 38; font.bold: true }
                        Text { text: "RACE"; color: "white"; font.pixelSize: 38; font.bold: true }
                        Text { text: " x "; color: "#8899aa"; font.pixelSize: 28; font.bold: true }
                        Text { text: "CSU"; color: "#FFD700"; font.pixelSize: 34; font.bold: true }
                        Text { text: "LA"; color: "white"; font.pixelSize: 34; font.bold: true }
                    }
                }

                // ==================== TOP RIGHT: TIME & STATUS ====================
                ColumnLayout {
                    Layout.alignment: Qt.AlignRight
                    spacing: 5

                    RowLayout {
                        spacing: 8
                        Rectangle {
                            width: 10
                            height: 10
                            radius: 5
                            color: obdConnected ? "#00ff88" : "#ff4444"
                        }
                        Text {
                            text: obdConnected ? "LIVE" : "NO SIGNAL"
                            color: obdConnected ? "#00ff88" : "#ff4444"
                            font.pixelSize: 12
                            font.bold: true
                        }
                    }
                    Text {
                        text: currentTime
                        color: "#FFD700"
                        font.pixelSize: 22
                        font.bold: true
                        font.family: "Monospace"
                    }
                }
            }
        }

        // ==================== MAIN CONTENT AREA ====================
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            Item {
                anchors.fill: parent

                // RPM GAUGE (Bottom Left)
                Rectangle {
                    width: 350
                    height: 350
                    color: "transparent"
                    radius: 20
                    anchors.bottom: parent.bottom
                    anchors.left: parent.left
                    anchors.margins: 30

                    ColumnLayout {
                        anchors.centerIn: parent
                        spacing: 20

                        Text {
                            text: "ENGINE RPM"
                            color: "#d0312d"
                            font.pixelSize: 22
                            font.bold: true
                            Layout.alignment: Qt.AlignHCenter
                        }

                        Item {
                            Layout.preferredWidth: 260
                            Layout.preferredHeight: 260
                            Layout.alignment: Qt.AlignHCenter

                            Canvas {
                                id: rpmGauge
                                anchors.fill: parent
                                onPaint: {
                                    var ctx = getContext("2d")
                                    var centerX = width / 2
                                    var centerY = height / 2
                                    var radius = width / 2 - 15
                                    var rpm = dataProvider ? dataProvider.rpm : 0
                                    var angle = (rpm / 8000) * 270 - 135

                                    ctx.clearRect(0, 0, width, height)

                                    ctx.beginPath()
                                    ctx.arc(centerX, centerY, radius, -135 * Math.PI / 180, 135 * Math.PI / 180)
                                    ctx.strokeStyle = "#333333"
                                    ctx.lineWidth = 15
                                    ctx.stroke()

                                    ctx.beginPath()
                                    ctx.arc(centerX, centerY, radius, -135 * Math.PI / 180, angle * Math.PI / 180)
                                    ctx.strokeStyle = "#d0312d"
                                    ctx.lineWidth = 15
                                    ctx.stroke()

                                    var needleAngle = angle * Math.PI / 180
                                    var needleX = centerX + Math.cos(needleAngle) * (radius - 20)
                                    var needleY = centerY + Math.sin(needleAngle) * (radius - 20)
                                    ctx.beginPath()
                                    ctx.moveTo(centerX, centerY)
                                    ctx.lineTo(needleX, needleY)
                                    ctx.strokeStyle = "white"
                                    ctx.lineWidth = 3
                                    ctx.stroke()

                                    ctx.beginPath()
                                    ctx.arc(centerX, centerY, 8, 0, 2 * Math.PI)
                                    ctx.fillStyle = "#d0312d"
                                    ctx.fill()
                                }
                                Connections {
                                    target: dataProvider
                                    function onRpmChanged() { rpmGauge.requestPaint() }
                                }
                            }

                            Column {
                                anchors.centerIn: parent
                                spacing: 5
                                Text {
                                    text: dataProvider ? Math.round(dataProvider.rpm) : "0"
                                    color: "white"
                                    font.pixelSize: 38
                                    font.bold: true
                                    horizontalAlignment: Text.AlignHCenter
                                }
                                Text {
                                    text: "RPM"
                                    color: "#d0312d"
                                    font.pixelSize: 16
                                    horizontalAlignment: Text.AlignHCenter
                                }
                            }
                        }
                    }
                }

                // FUEL LEVEL (Bottom Center-Left)
                Rectangle {
                    width: 100
                    height: 350
                    color: "transparent"
                    radius: 20
                    anchors.bottom: parent.bottom
                    anchors.left: parent.left
                    anchors.leftMargin: 400
                    anchors.margins: 30

                    ColumnLayout {
                        anchors.centerIn: parent
                        spacing: 20

                        Text {
                            text: "FUEL"
                            color: "#d0312d"
                            font.pixelSize: 18
                            font.bold: true
                            Layout.alignment: Qt.AlignHCenter
                        }

                        Rectangle {
                            Layout.preferredWidth: 50
                            Layout.preferredHeight: 250
                            Layout.alignment: Qt.AlignHCenter
                            color: "#0a0a0a"
                            border.color: "#333333"
                            border.width: 2
                            radius: 25

                            Rectangle {
                                width: parent.width - 10
                                height: parent.height * ((dataProvider ? dataProvider.fuel_level : 75) / 100) - 10
                                anchors.bottom: parent.bottom
                                anchors.horizontalCenter: parent.horizontalCenter
                                anchors.bottomMargin: 5
                                color: {
                                    var fuel = dataProvider ? dataProvider.fuel_level : 75
                                    if (fuel >= 60) return "#00ff88"
                                    else if (fuel >= 40) return "#FFD700"
                                    else return "#d0312d"
                                }
                                radius: 20
                                Behavior on height {
                                    NumberAnimation { duration: 300 }
                                }
                            }
                        }

                        Text {
                            text: dataProvider ? Math.round(dataProvider.fuel_level) + "%" : "75%"
                            color: "white"
                            font.pixelSize: 22
                            font.bold: true
                            Layout.alignment: Qt.AlignHCenter
                        }
                    }
                }

                // SPEED GAUGE (Bottom Right)
                Rectangle {
                    width: 350
                    height: 350
                    color: "transparent"
                    radius: 20
                    anchors.bottom: parent.bottom
                    anchors.right: parent.right
                    anchors.margins: 30

                    ColumnLayout {
                        anchors.centerIn: parent
                        spacing: 20

                        Text {
                            text: "SPEED"
                            color: "#d0312d"
                            font.pixelSize: 22
                            font.bold: true
                            Layout.alignment: Qt.AlignHCenter
                        }

                        Item {
                            Layout.preferredWidth: 260
                            Layout.preferredHeight: 260
                            Layout.alignment: Qt.AlignHCenter

                            Canvas {
                                id: speedGauge
                                anchors.fill: parent
                                onPaint: {
                                    var ctx = getContext("2d")
                                    var centerX = width / 2
                                    var centerY = height / 2
                                    var radius = width / 2 - 15
                                    var speed = dataProvider ? dataProvider.speed : 0
                                    var angle = (speed / 160) * 270 - 135

                                    ctx.clearRect(0, 0, width, height)

                                    ctx.beginPath()
                                    ctx.arc(centerX, centerY, radius, -135 * Math.PI / 180, 135 * Math.PI / 180)
                                    ctx.strokeStyle = "#333333"
                                    ctx.lineWidth = 15
                                    ctx.stroke()

                                    ctx.beginPath()
                                    ctx.arc(centerX, centerY, radius, -135 * Math.PI / 180, angle * Math.PI / 180)
                                    ctx.strokeStyle = "#d0312d"
                                    ctx.lineWidth = 15
                                    ctx.stroke()

                                    var needleAngle = angle * Math.PI / 180
                                    var needleX = centerX + Math.cos(needleAngle) * (radius - 20)
                                    var needleY = centerY + Math.sin(needleAngle) * (radius - 20)
                                    ctx.beginPath()
                                    ctx.moveTo(centerX, centerY)
                                    ctx.lineTo(needleX, needleY)
                                    ctx.strokeStyle = "white"
                                    ctx.lineWidth = 3
                                    ctx.stroke()

                                    ctx.beginPath()
                                    ctx.arc(centerX, centerY, 8, 0, 2 * Math.PI)
                                    ctx.fillStyle = "#d0312d"
                                    ctx.fill()
                                }
                                Connections {
                                    target: dataProvider
                                    function onSpeedChanged() { speedGauge.requestPaint() }
                                }
                            }

                            Column {
                                anchors.centerIn: parent
                                spacing: 5
                                Text {
                                    text: dataProvider ? Math.round(dataProvider.speed) : "0"
                                    color: "white"
                                    font.pixelSize: 38
                                    font.bold: true
                                    horizontalAlignment: Text.AlignHCenter
                                }
                                Text {
                                    text: "MPH"
                                    color: "#d0312d"
                                    font.pixelSize: 16
                                    horizontalAlignment: Text.AlignHCenter
                                }
                            }
                        }
                    }
                }
            }
        }

        // ==================== BOTTOM STATUS BAR ====================
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 40
            color: "transparent"

            RowLayout {
                anchors.fill: parent
                anchors.margins: 15

                Text {
                    text: "TEAM 113 - AR HUD 2026"
                    color: "white"
                    font.pixelSize: 12
                    font.bold: true
                    Layout.alignment: Qt.AlignLeft
                }

                Item { Layout.fillWidth: true }

                RowLayout {
                    spacing: 15
                    
                    RowLayout {
                        spacing: 5
                        Rectangle {
                            width: 8
                            height: 8
                            radius: 4
                            color: imuConnected ? "#00ff88" : "#ff4444"
                        }
                        Text {
                            text: imuConnected ? "IMU OK" : "IMU OFF"
                            color: imuConnected ? "#00ff88" : "#ff4444"
                            font.pixelSize: 10
                            font.bold: true
                        }
                    }
                    
                    RowLayout {
                        spacing: 5
                        Rectangle {
                            width: 8
                            height: 8
                            radius: 4
                            color: obdConnected ? "#00ff88" : "#ff4444"
                        }
                        Text {
                            text: obdConnected ? "OBD2 OK" : "OBD2 OFF"
                            color: obdConnected ? "#00ff88" : "#ff4444"
                            font.pixelSize: 10
                            font.bold: true
                        }
                    }
                }

                Item { Layout.fillWidth: true }

                RowLayout {
                    spacing: 0
                    Text { text: "BUILT"; color: "#d0312d"; font.pixelSize: 12; font.bold: true }
                    Text { text: "2"; color: "#d0312d"; font.pixelSize: 12; font.bold: true }
                    Text { text: "RACE"; color: "white"; font.pixelSize: 12; font.bold: true }
                    Text { text: " x "; color: "#8899aa"; font.pixelSize: 12; font.bold: true }
                    Text { text: "CSULA"; color: "#FFD700"; font.pixelSize: 12; font.bold: true }
                }
            }
        }
    }
}