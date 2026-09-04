import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import org.kde.layershell as LayerShell

QtObject {
    id: root

    required property int cfgSeconds
    required property string cfgTask
    required property bool cfgPicker
    required property int cfgLastMinutes

    property int remaining: cfgSeconds
    property string task: cfgTask

    function formatTime(s) {
        const m = Math.floor(s / 60)
        const sec = s % 60
        return String(m).padStart(2, "0") + ":" + String(sec).padStart(2, "0")
    }

    function startOverlay() {
        picker.visible = false
        overlay.visible = true
        overlay.raise()
    }

    property Window overlay: Window {
        id: overlay
        title: "cclock"
        flags: Qt.FramelessWindowHint | Qt.WindowTransparentForInput | Qt.WindowDoesNotAcceptFocus
        color: Qt.rgba(0, 0, 0, 0.35)
        width: Math.max(320, col.implicitWidth + 40)
        height: Math.max(100, col.implicitHeight + 16)
        visible: !root.cfgPicker

        LayerShell.Window.scope: "cclock"
        LayerShell.Window.layer: LayerShell.Window.LayerOverlay
        LayerShell.Window.anchors: LayerShell.Window.AnchorBottom | LayerShell.Window.AnchorLeft
        LayerShell.Window.keyboardInteractivity: LayerShell.Window.KeyboardInteractivityNone
        LayerShell.Window.exclusionZone: 0
        LayerShell.Window.activateOnShow: false
        LayerShell.Window.wantsToBeOnActiveScreen: true

        Column {
            id: col
            anchors.centerIn: parent
            spacing: 0

            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: root.formatTime(root.remaining)
                font.pixelSize: 72
                color: "#fcefd4"
            }

            Item {
                id: marquee
                visible: root.task.length > 0
                width: Math.max(280, parent.width)
                height: 55
                clip: true

                Text {
                    id: taskLabel
                    y: (parent.height - height) / 2
                    text: root.task
                    font.pixelSize: 36
                    color: "#fcefd4"
                    x: width <= marquee.width - 20 ? (marquee.width - width) / 2 : marquee.width
                }

                Timer {
                    interval: 30
                    running: overlay.visible && taskLabel.width > marquee.width - 20
                    repeat: true
                    onTriggered: {
                        taskLabel.x -= 1.5
                        if (taskLabel.x < -taskLabel.width)
                            taskLabel.x = marquee.width
                    }
                }
            }
        }

        Timer {
            interval: 1000
            running: overlay.visible
            repeat: true
            onTriggered: {
                if (root.remaining <= 1) {
                    sys.beep()
                    Qt.quit()
                    return
                }
                root.remaining--
            }
        }
    }

    property Window picker: Window {
        id: picker
        title: "CClock Picker"
        color: "transparent"
        visible: root.cfgPicker
        flags: Qt.FramelessWindowHint

        LayerShell.Window.scope: "cclock-picker"
        LayerShell.Window.layer: LayerShell.Window.LayerOverlay
        LayerShell.Window.anchors: LayerShell.Window.AnchorTop | LayerShell.Window.AnchorBottom | LayerShell.Window.AnchorLeft | LayerShell.Window.AnchorRight
        LayerShell.Window.keyboardInteractivity: LayerShell.Window.KeyboardInteractivityOnDemand
        LayerShell.Window.exclusionZone: -1
        LayerShell.Window.activateOnShow: true
        LayerShell.Window.wantsToBeOnActiveScreen: true

        onVisibleChanged: if (visible) {
            requestActivate()
            mins.forceActiveFocus()
        }

        function startFromInput() {
            const raw = mins.text.length ? mins.text : ("" + root.cfgLastMinutes)
            const m = parseInt(raw, 10)
            if (!(m > 0))
                return
            sys.saveMinutes(m)
            root.remaining = m * 60
            root.startOverlay()
        }

        Rectangle {
            anchors.fill: parent
            color: Qt.rgba(0, 0, 0, 0.55)
            MouseArea {
                anchors.fill: parent
                onClicked: Qt.quit()
            }
        }

        Rectangle {
            width: 268
            height: card.implicitHeight + 24
            anchors.centerIn: parent
            color: "#12141a"
            radius: 16
            border.color: Qt.rgba(252 / 255, 239 / 255, 212 / 255, 0.22)
            border.width: 1

            MouseArea {
                anchors.fill: parent
            }

            Column {
                id: card
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                anchors.leftMargin: 14
                anchors.rightMargin: 14
                spacing: 8

                Text {
                    text: "Timer"
                    color: Qt.rgba(252 / 255, 239 / 255, 212 / 255, 0.55)
                    font.pixelSize: 11
                    font.letterSpacing: 1.4
                    font.capitalization: Font.AllUppercase
                }

                Row {
                    width: parent.width
                    spacing: 8

                    Item {
                        width: parent.width - unit.width - 8
                        height: 40

                        Rectangle {
                            anchors.fill: parent
                            color: "#0b0d12"
                            radius: 10
                            border.width: mins.activeFocus ? 1 : 0
                            border.color: "#fcefd4"
                        }

                        Text {
                            anchors.centerIn: parent
                            visible: mins.text.length === 0
                            text: "" + root.cfgLastMinutes
                            color: Qt.rgba(252 / 255, 239 / 255, 212 / 255, 0.32)
                            font.pixelSize: 22
                        }

                        TextField {
                            id: mins
                            anchors.fill: parent
                            color: "#fcefd4"
                            font.pixelSize: 22
                            horizontalAlignment: Text.AlignHCenter
                            inputMethodHints: Qt.ImhDigitsOnly
                            validator: IntValidator {
                                bottom: 1
                                top: 999
                            }
                            background: Item {}
                            Keys.onPressed: event => {
                                if (event.key === Qt.Key_Tab && text.length === 0) {
                                    text = "" + root.cfgLastMinutes
                                    event.accepted = true
                                }
                            }
                            Keys.onReturnPressed: picker.startFromInput()
                            Keys.onEnterPressed: picker.startFromInput()
                            Keys.onEscapePressed: Qt.quit()
                        }
                    }

                    Rectangle {
                        id: unit
                        width: 48
                        height: 40
                        radius: 10
                        color: "#0b0d12"

                        Text {
                            anchors.centerIn: parent
                            text: "min"
                            color: "#fcefd4"
                            font.pixelSize: 14
                        }
                    }
                }

                Text {
                    text: "Tab · last time   Enter · start   Esc · close"
                    color: Qt.rgba(252 / 255, 239 / 255, 212 / 255, 0.38)
                    font.pixelSize: 11
                    width: parent.width
                    horizontalAlignment: Text.AlignHCenter
                }
            }
        }
    }
}
