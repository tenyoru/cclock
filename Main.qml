import QtQuick
import QtQuick.Controls.Basic
import org.kde.layershell as LayerShell

QtObject {
    id: root

    required property bool cfgPicker
    required property color cfgColor
    required property color cfgPauseColor
    required property color cfgOvertimeColor
    required property bool cfgDarkText
    required property bool cfgPauseDarkText
    required property int cfgLastMinutes
    required property string cfgEdge
    required property real cfgOffset
    required property string cfgScreen
    required property string cfgOled
    required property int cfgOledInterval
    required property int cfgOledShift
    required property int cfgOledTimeout
    required property bool cfgKeyboardMotion
    required property int cfgKeyboardStep

    property string edge: cfgEdge
    property real offset: cfgOffset
    property string desiredScreen: cfgScreen
    property string screenName: ""
    property var overlays: ({})
    property int oledPhase: 0
    property bool oledAwake: true
    property bool oledHovered: false
    readonly property bool oledProtected: cfgOled === "all" || cfgOled === screenName
    readonly property int oledTextX: oledPhase === 1 ? cfgOledShift
                                                     : (oledPhase === 3 ? -cfgOledShift : 0)
    readonly property int oledTextY: oledPhase === 2 ? cfgOledShift
                                                     : (oledPhase === 4 ? -cfgOledShift : 0)
    property bool dragging: false
    property real dragX: 0
    property real dragY: 0
    property real grabX: 0
    property real grabY: 0
    property int lastBw: 0
    property int lastBh: 0
    property int lastSw: 0
    property int lastSh: 0
    property double deadline: 0

    function resetDeadline() {
        deadline = Date.now() + sys.remaining * 1000
    }

    function updateRemaining() {
        const previous = sys.remaining
        const current = Math.ceil((deadline - Date.now()) / 1000)
        if (current === previous)
            return
        sys.remaining = current
        if (previous > 0 && current <= 0) {
            sys.beep()
            sys.reachedZero()
            wakeOled()
        }
    }

    function endSession() {
        sys.finish()
        Qt.quit()
    }

    function hasScreen(names, name) {
        return names.indexOf(name) !== -1
    }

    function wakeOled() {
        oledIdleTimer.stop()
        oledAwake = true
        if (oledProtected && (sys.paused || sys.remaining < 0) && !oledHovered)
            oledIdleTimer.start()
    }

    function syncScreens() {
        const names = sys.screenNames()
        const wanted = {}
        for (let i = 0; i < names.length; i++) {
            const name = names[i]
            wanted[name] = true
            if (!overlays[name])
                overlays[name] = overlayComp.createObject(root, { "screenHint": name })
        }
        for (const name in overlays) {
            if (!wanted[name]) {
                overlays[name].destroy()
                delete overlays[name]
            }
        }
        if (hasScreen(names, desiredScreen))
            screenName = desiredScreen
        else if (!hasScreen(names, screenName)) {
            const fallback = sys.screenAtCursor()
            screenName = hasScreen(names, fallback) ? fallback : (names.length ? names[0] : "")
        }
        wakeOled()
    }

    function nudge(pixels) {
        const overlay = overlays[screenName]
        if (!overlay)
            return
        const vertical = edge === "left" || edge === "right"
        const travel = vertical ? overlay.geo.h - overlay.blobHeight : overlay.geo.w - overlay.blobWidth
        if (travel <= 0)
            return
        offset = Math.max(0, Math.min(1, offset + pixels / travel))
        sys.set("offset", offset)
    }

    function finishDrag() {
        if (!root.dragging)
            return
        const vertical = root.edge === "left" || root.edge === "right"
        // Same dimensions the drag used, so saving and restoring share one
        // denominator and the blob cannot drift between drop and redraw.
        const travel = vertical ? root.lastSh - root.lastBh : root.lastSw - root.lastBw
        const pos = vertical ? root.dragY : root.dragX
        root.offset = travel > 0 ? Math.max(0, Math.min(1, pos / travel)) : 0
        sys.set("edge", root.edge)
        sys.set("offset", root.offset)
        root.dragging = false
    }

    property Connections sysConn: Connections {
        target: sys
        function onStopRequested() {
            root.endSession()
        }
        function onFocusRequested() {
            const overlay = root.overlays[root.screenName]
            if (overlay) {
                root.wakeOled()
                overlay.requestActivate()
            }
        }
        function onPausedChanged() {
            if (sys.paused)
                root.updateRemaining()
            else
                root.resetDeadline()
            root.wakeOled()
        }
    }

    property Timer tickTimer: Timer {
        interval: 250
        running: !picker.visible && !sys.paused
        repeat: true
        onTriggered: root.updateRemaining()
    }

    property Timer oledTimer: Timer {
        interval: root.cfgOledInterval * 1000
        running: !picker.visible && root.cfgOledShift > 0 && root.oledProtected
        repeat: true
        onTriggered: root.oledPhase = (root.oledPhase + 1) % 5
    }

    property Timer oledIdleTimer: Timer {
        interval: root.cfgOledTimeout * 1000
        repeat: false
        onTriggered: if (!root.oledHovered)
            root.oledAwake = false
    }

    property Component overlayComp: Component {
        Window {
            id: overlay
            property string screenHint
            readonly property int blobWidth: blob.width
            readonly property int blobHeight: blob.height
            // Keyed by name, so it always describes the output the layer
            // surface is pinned to. `Screen.*` describes whichever output Qt
            // thinks the window is on, which is not the same thing.
            readonly property var geo: sys.screenGeometry(screenHint)
            readonly property bool home: screenHint === root.screenName
            title: "cclock"
            flags: Qt.FramelessWindowHint
            color: "transparent"
            width: geo.w
            height: geo.h
            visible: !picker.visible

            LayerShell.Window.scope: "cclock"
            LayerShell.Window.layer: LayerShell.Window.LayerOverlay
            LayerShell.Window.anchors: LayerShell.Window.AnchorTop | LayerShell.Window.AnchorBottom | LayerShell.Window.AnchorLeft | LayerShell.Window.AnchorRight
            LayerShell.Window.margins: ({ left: 0, top: 0, right: 0, bottom: 0 })
            LayerShell.Window.keyboardInteractivity: root.cfgKeyboardMotion && home
                                                     ? LayerShell.Window.KeyboardInteractivityOnDemand
                                                     : LayerShell.Window.KeyboardInteractivityNone
            LayerShell.Window.exclusionZone: -1
            LayerShell.Window.activateOnShow: false
            LayerShell.Window.wantsToBeOnActiveScreen: false
            LayerShell.Window.screen: sys.screenByName(overlay.screenHint)

            readonly property int anchorLeft: Math.max(0, Math.min(Math.round(root.offset * (geo.w - blob.collapsedWidth)), geo.w - blob.width))
            readonly property int anchorTop: Math.max(0, Math.min(Math.round(root.offset * (geo.h - blob.collapsedHeight)), geo.h - blob.height))

            function blobX() {
                if (root.edge === "left")
                    return 0
                if (root.edge === "right")
                    return geo.w - blob.width
                return anchorLeft
            }

            function blobY() {
                if (root.edge === "top")
                    return 0
                if (root.edge === "bottom")
                    return geo.h - blob.height
                return anchorTop
            }

            // Wayland's implicit grab keeps every event on the surface where
            // the press happened, so coordinates must go through the virtual
            // desktop to land on another monitor.
            function placeBlob(localX, localY) {
                const gx = geo.x + localX - Math.min(root.grabX, blob.width) + blob.width / 2
                const gy = geo.y + localY - Math.min(root.grabY, blob.height) + blob.height / 2
                const at = sys.screenAtGlobal(Math.round(gx), Math.round(gy))
                const name = at.length ? at : screenHint
                const g = sys.screenGeometry(name)
                const r = sys.placeOnRim(gx - g.x, gy - g.y, blob.width, blob.height, g.w, g.h, root.edge)
                root.edge = r.edge
                root.dragX = r.x
                root.dragY = r.y
                root.lastBw = blob.width
                root.lastBh = blob.height
                root.lastSw = g.w
                root.lastSh = g.h
                root.screenName = name
                root.desiredScreen = name
                sys.set("screen", name)
            }

            function syncMask() {
                if (root.dragging)
                    sys.clearInputMask(overlay)
                else if (home)
                    sys.setInputMask(overlay, Math.round(blob.x), Math.round(blob.y), blob.width, blob.height)
                else
                    sys.blockInput(overlay)
            }

            onVisibleChanged: if (visible)
                syncMask()
            onHomeChanged: syncMask()
            Component.onCompleted: syncMask()

            Rectangle {
                id: blob
                visible: home
                z: 1
                readonly property bool hovered: ma.containsMouse || closeArea.containsMouse
                readonly property bool ghost: hovered && !root.dragging
                readonly property bool vertical: root.edge === "left" || root.edge === "right"
                readonly property bool oledPausedHidden: root.oledProtected && sys.paused && !root.oledAwake
                readonly property bool oledOvertimeDimmed: root.oledProtected && sys.remaining < 0 && !root.oledAwake
                readonly property color baseColor: sys.paused ? root.cfgPauseColor
                                                               : (sys.remaining < 0
                                                                  ? (oledOvertimeDimmed ? "#000000" : root.cfgOvertimeColor)
                                                                  : root.cfgColor)
                readonly property bool darkText: sys.paused ? root.cfgPauseDarkText
                                                             : (sys.remaining < 0
                                                                ? !oledOvertimeDimmed
                                                                : root.cfgDarkText)
                readonly property color inkColor: oledOvertimeDimmed ? root.cfgOvertimeColor
                                                                      : (darkText ? "#0b0b0d" : "#f5f5f7")
                // Tight where the blob fuses to the screen, roomier on the free
                // side where the corner radius curves in toward the digits.
                // The two orientations differ because the font bakes ~6px of
                // leading into implicitHeight but only ~1px into implicitWidth.
                readonly property int padNear: vertical ? 10 : 6
                readonly property int padFar: vertical ? 13 : 9
                readonly property int padCross: 16
                readonly property bool nearIsMin: root.edge === "left" || root.edge === "top"
                function boxW(w) {
                    return Math.round(w) + (vertical ? padNear + padFar : 2 * padCross)
                }
                function boxH(h) {
                    return Math.round(h) + (vertical ? 2 * padCross : padNear + padFar)
                }
                readonly property int collapsedWidth: boxW(timeText.implicitWidth)
                readonly property int collapsedHeight: boxH(timeText.implicitHeight)
                width: boxW(content.implicitWidth)
                height: boxH(content.implicitHeight)
                // The 1px border is drawn inside the bounds and is 92%
                // transparent, so at the contact edge it reveals the wallpaper
                // as a bright seam. Push it past the surface edge so the fill
                // meets the screen directly.
                readonly property int bleedX: root.edge === "left" ? -1 : (root.edge === "right" ? 1 : 0)
                readonly property int bleedY: root.edge === "top" ? -1 : (root.edge === "bottom" ? 1 : 0)
                x: (root.dragging ? root.dragX : overlay.blobX()) + bleedX
                y: (root.dragging ? root.dragY : overlay.blobY()) + bleedY
                onXChanged: if (overlay) overlay.syncMask()
                onYChanged: if (overlay) overlay.syncMask()
                onWidthChanged: if (overlay) overlay.syncMask()
                onHeightChanged: if (overlay) overlay.syncMask()
                property color surfaceColor: Qt.rgba(baseColor.r, baseColor.g, baseColor.b, ghost ? 0.35 : 0.94)
                property color outlineColor: Qt.rgba(inkColor.r, inkColor.g, inkColor.b, darkText ? (ghost ? 0.05 : 0.12) : (ghost ? 0.03 : 0.08))
                color: "transparent"
                border.width: 0
                opacity: oledPausedHidden ? 0 : 1
                Behavior on opacity {
                    NumberAnimation {
                        duration: 300
                    }
                }
                Behavior on surfaceColor {
                    ColorAnimation {
                        duration: 160
                    }
                }
                Behavior on outlineColor {
                    ColorAnimation {
                        duration: 160
                    }
                }

                Canvas {
                    id: meniscus
                    readonly property int spread: 18
                    readonly property int depth: 18
                    property color fillColor: blob.surfaceColor
                    property color strokeColor: blob.outlineColor
                    property string edge: root.edge
                    x: -spread
                    y: -spread
                    width: blob.width + 2 * spread
                    height: blob.height + 2 * spread
                    onFillColorChanged: requestPaint()
                    onStrokeColorChanged: requestPaint()
                    onEdgeChanged: requestPaint()
                    onPaint: {
                        const ctx = getContext("2d")
                        const s = spread
                        const d = depth
                        const w = width
                        const h = height
                        const left = s
                        const top = s
                        const right = w - s
                        const bottom = h - s
                        const radius = Math.min(blob.width, blob.height) / 2
                        ctx.clearRect(0, 0, w, h)
                        ctx.fillStyle = fillColor
                        ctx.beginPath()
                        if (edge === "top") {
                            ctx.moveTo(0, top)
                            ctx.bezierCurveTo(left * 0.55, top, left, top + d * 0.45, left, top + d)
                            ctx.lineTo(left, bottom - radius)
                            ctx.quadraticCurveTo(left, bottom, left + radius, bottom)
                            ctx.lineTo(right - radius, bottom)
                            ctx.quadraticCurveTo(right, bottom, right, bottom - radius)
                            ctx.lineTo(right, top + d)
                            ctx.bezierCurveTo(right, top + d * 0.45, w - left * 0.55, top, w, top)
                        } else if (edge === "bottom") {
                            ctx.moveTo(0, bottom)
                            ctx.bezierCurveTo(left * 0.55, bottom, left, bottom - d * 0.45, left, bottom - d)
                            ctx.lineTo(left, top + radius)
                            ctx.quadraticCurveTo(left, top, left + radius, top)
                            ctx.lineTo(right - radius, top)
                            ctx.quadraticCurveTo(right, top, right, top + radius)
                            ctx.lineTo(right, bottom - d)
                            ctx.bezierCurveTo(right, bottom - d * 0.45, w - left * 0.55, bottom, w, bottom)
                        } else if (edge === "left") {
                            ctx.moveTo(left, 0)
                            ctx.bezierCurveTo(left, top * 0.55, left + d * 0.45, top, left + d, top)
                            ctx.lineTo(right - radius, top)
                            ctx.quadraticCurveTo(right, top, right, top + radius)
                            ctx.lineTo(right, bottom - radius)
                            ctx.quadraticCurveTo(right, bottom, right - radius, bottom)
                            ctx.lineTo(left + d, bottom)
                            ctx.bezierCurveTo(left + d * 0.45, bottom, left, h - top * 0.55, left, h)
                        } else {
                            ctx.moveTo(right, 0)
                            ctx.bezierCurveTo(right, top * 0.55, right - d * 0.45, top, right - d, top)
                            ctx.lineTo(left + radius, top)
                            ctx.quadraticCurveTo(left, top, left, top + radius)
                            ctx.lineTo(left, bottom - radius)
                            ctx.quadraticCurveTo(left, bottom, left + radius, bottom)
                            ctx.lineTo(right - d, bottom)
                            ctx.bezierCurveTo(right - d * 0.45, bottom, right, h - top * 0.55, right, h)
                        }
                        ctx.closePath()
                        ctx.fill()
                        ctx.strokeStyle = strokeColor
                        ctx.lineWidth = 1
                        ctx.stroke()
                    }
                }

                Grid {
                    id: content
                    x: blob.vertical ? (blob.nearIsMin ? blob.padNear : blob.padFar) : Math.round((blob.width - width) / 2)
                    y: blob.vertical ? Math.round((blob.height - height) / 2) : (blob.nearIsMin ? blob.padNear : blob.padFar)
                    columns: blob.vertical ? 1 : 3
                    horizontalItemAlignment: Grid.AlignHCenter
                    verticalItemAlignment: Grid.AlignVCenter
                    spacing: blob.hovered ? (blob.vertical ? 6 : 8) : 0

                    Text {
                        id: timeText
                        text: sys.formatTime(Math.abs(sys.remaining), blob.vertical ? "\n" : ":")
                        horizontalAlignment: Text.AlignHCenter
                        lineHeight: 0.92
                        font.pixelSize: 30
                        font.weight: Font.DemiBold
                        font.features: ({ "tnum": 1 })
                        color: blob.inkColor
                        transform: Translate {
                            x: root.oledProtected ? root.oledTextX : 0
                            y: root.oledProtected ? root.oledTextY : 0
                        }
                    }

                    Text {
                        id: textLabel
                        readonly property bool shown: visible && blob.hovered && !root.dragging
                        visible: sys.text.length > 0
                        width: shown ? Math.min(implicitWidth, blob.vertical ? 150 : 320) : 0
                        height: blob.vertical ? (shown ? contentHeight : 0) : timeText.contentHeight
                        horizontalAlignment: blob.vertical ? Text.AlignHCenter : Text.AlignLeft
                        verticalAlignment: Text.AlignVCenter
                        clip: true
                        elide: Text.ElideRight
                        text: sys.text
                        font.pixelSize: 15
                        color: Qt.rgba(blob.inkColor.r, blob.inkColor.g, blob.inkColor.b, 0.7)
                        opacity: shown ? 1 : 0
                        Behavior on width {
                            enabled: !root.dragging
                            NumberAnimation {
                                duration: 220
                                easing.type: Easing.OutCubic
                            }
                        }
                        Behavior on height {
                            enabled: !root.dragging
                            NumberAnimation {
                                duration: 220
                                easing.type: Easing.OutCubic
                            }
                        }
                        Behavior on opacity {
                            enabled: !root.dragging
                            NumberAnimation {
                                duration: 160
                            }
                        }
                    }

                    Rectangle {
                        id: closeBtn
                        readonly property bool shown: blob.hovered && !root.dragging
                        readonly property int full: 26
                        width: shown ? full : 0
                        height: blob.vertical ? (shown ? full : 0) : full
                        radius: height / 2
                        clip: true
                        color: closeArea.containsMouse ? "#ff453a" : Qt.rgba(blob.inkColor.r, blob.inkColor.g, blob.inkColor.b, 0.25)
                        opacity: shown ? 1 : 0

                        Text {
                            anchors.centerIn: parent
                            text: "\u00d7"
                            color: closeArea.containsMouse ? "#ffffff" : blob.inkColor
                            font.pixelSize: 18
                            font.weight: Font.Bold
                        }

                        MouseArea {
                            id: closeArea
                            anchors.fill: parent
                            hoverEnabled: true
                            acceptedButtons: Qt.LeftButton
                            onClicked: root.endSession()
                        }

                        Behavior on width {
                            enabled: !root.dragging
                            NumberAnimation {
                                duration: 220
                                easing.type: Easing.OutCubic
                            }
                        }
                        Behavior on height {
                            enabled: !root.dragging
                            NumberAnimation {
                                duration: 220
                                easing.type: Easing.OutCubic
                            }
                        }
                        Behavior on opacity {
                            enabled: !root.dragging
                            NumberAnimation {
                                duration: 160
                            }
                        }
                    }
                }
            }

            MouseArea {
                id: ma
                anchors.fill: parent
                hoverEnabled: true
                acceptedButtons: Qt.LeftButton
                property real pressX: 0
                property real pressY: 0

                onPressed: mouse => {
                    root.wakeOled()
                    if (root.cfgKeyboardMotion)
                        overlay.requestActivate()
                    pressX = mouse.x
                    pressY = mouse.y
                }

                onPositionChanged: mouse => {
                    // hoverEnabled means this also fires with no button held;
                    // without this guard a post-release move resumes the drag.
                    if (!pressed)
                        return
                    if (root.dragging) {
                        overlay.placeBlob(mouse.x, mouse.y)
                        return
                    }
                    if (Math.hypot(mouse.x - pressX, mouse.y - pressY) < 10)
                        return
                    const grabX = pressX - blob.x
                    const grabY = pressY - blob.y
                    root.dragX = overlay.blobX()
                    root.dragY = overlay.blobY()
                    root.screenName = overlay.screenHint
                    root.dragging = true
                    root.grabX = Math.max(0, Math.min(blob.width, grabX))
                    root.grabY = Math.max(0, Math.min(blob.height, grabY))
                    root.lastBw = blob.width
                    root.lastBh = blob.height
                    root.lastSw = overlay.geo.w
                    root.lastSh = overlay.geo.h
                    overlay.placeBlob(mouse.x, mouse.y)
                }

                // Branching here rather than in onClicked, which fires after
                // finishDrag() has already cleared `dragging` and so cannot
                // tell a click from the end of a drag.
                onReleased: {
                    if (root.dragging)
                        root.finishDrag()
                    else
                        sys.paused = !sys.paused
                }
                onCanceled: root.finishDrag()
                onContainsMouseChanged: {
                    if (!overlay.home)
                        return
                    root.oledHovered = containsMouse
                    root.wakeOled()
                }
            }

            Connections {
                target: root
                function onDraggingChanged() {
                    overlay.syncMask()
                }
            }

            Item {
                anchors.fill: parent
                focus: overlay.active
                Keys.onPressed: event => {
                    const vertical = root.edge === "left" || root.edge === "right"
                    if ((!vertical && event.key === Qt.Key_H) || (vertical && event.key === Qt.Key_K))
                        root.nudge(-root.cfgKeyboardStep)
                    else if ((!vertical && event.key === Qt.Key_L) || (vertical && event.key === Qt.Key_J))
                        root.nudge(root.cfgKeyboardStep)
                    else
                        return
                    event.accepted = true
                }
            }
        }
    }

    Component.onCompleted: {
        syncScreens()
        if (!cfgPicker) {
            sys.timerStarted()
            resetDeadline()
        }
    }

    property Connections screenConn: Connections {
        target: sys
        function onScreensChanged() {
            root.syncScreens()
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
            sys.set("lastMinutes", m)
            sys.remaining = m * 60
            sys.timerStarted()
            root.resetDeadline()
            picker.visible = false
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
