import QtQuick

// Miuix-style checkbox: a filled circle with a white checkmark when checked.
// Visual reference: compose-miuix-ui/miuix Checkbox.kt — circle shape,
// two-segment checkmark path (5,9.4 → 10.3,14.9 → 17.9,5.1 in a 23×23
// viewport), 9% stroke width, round caps. Black/white palette per request.
Item {
    id: root

    property bool checked: false
    signal toggled()

    width: 20
    height: 20

    // Circle background — black when checked, transparent when unchecked.
    Rectangle {
        id: circle
        anchors.fill: parent
        radius: width / 2
        color: root.checked ? theme.ink : "transparent"
        border.color: root.checked ? theme.ink : theme.line
        border.width: 1.5
        Behavior on color { ColorAnimation { duration: 200; easing.type: Easing.OutCubic } }
        Behavior on border.color { ColorAnimation { duration: 200; easing.type: Easing.OutCubic } }
    }

    // Checkmark — white, drawn via Canvas, fades in when checked.
    Canvas {
        id: check
        anchors.fill: parent
        opacity: root.checked ? 1 : 0
        Behavior on opacity { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }
        onPaint: {
            var ctx = getContext("2d")
            ctx.reset()
            ctx.strokeStyle = theme.paper   // white in light mode, dark in dark mode — inverse of the circle
            ctx.lineWidth = width * 0.09
            ctx.lineCap = "round"
            ctx.lineJoin = "round"
            ctx.beginPath()
            var s = width / 23   // Miuix viewport is 23×23
            ctx.moveTo(5 * s, 9.4 * s)
            ctx.lineTo(10.3 * s, 14.9 * s)
            ctx.lineTo(17.9 * s, 5.1 * s)
            ctx.stroke()
        }
    }

    MouseArea {
        anchors.fill: parent
        cursorShape: Qt.PointingHandCursor
        onClicked: {
            root.checked = !root.checked
            root.toggled()
        }
    }
}
