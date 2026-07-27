import QtQuick
import QtQuick.Controls
import QtQuick.Window

Popup {
    id: root

    property var items: []
    property int itemHeight: 48
    property int minMenuWidth: 180

    signal triggered(int index)

    width: Math.max(minMenuWidth, list.implicitWidth + theme.sp2 * 2)
    height: list.contentHeight + theme.sp2 * 2
    padding: theme.sp1
    modal: false
    focus: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    transformOrigin: Item.Top

    function openNear(item, localX, localY) {
        const host = Window.window && Window.window.contentItem ? Window.window.contentItem : item.parent
        const p = item.mapToItem(host, localX, localY)
        parent = host
        x = p.x
        y = p.y
        open()
    }

    background: Item {
        Rectangle {
            anchors.fill: parent
            anchors.topMargin: 6
            radius: 18
            color: theme.shadowColor
            opacity: root.opacity
        }
        Rectangle {
            anchors.fill: parent
            radius: 18
            color: theme.dark ? Qt.rgba(theme.surface.r, theme.surface.g, theme.surface.b, 0.96)
                              : Qt.rgba(theme.surface.r, theme.surface.g, theme.surface.b, 0.98)
            border.width: 1
            border.color: theme.line
        }
    }

    contentItem: ListView {
        id: list
        implicitWidth: 0
        implicitHeight: contentHeight
        clip: true
        interactive: false
        model: root.items
        spacing: 2

        delegate: ItemDelegate {
            id: delegateRoot
            required property int index
            required property var modelData

            width: root.width - root.padding * 2
            height: root.itemHeight
            padding: 0
            hoverEnabled: true

            background: Rectangle {
                radius: 14
                color: delegateRoot.hovered
                       ? Qt.rgba(theme.clay.r, theme.clay.g, theme.clay.b, theme.dark ? 0.14 : 0.08)
                       : "transparent"
                Behavior on color { ColorAnimation { duration: 120 } }
            }

            contentItem: Text {
                leftPadding: theme.sp3
                rightPadding: theme.sp3
                text: typeof delegateRoot.modelData === "string" ? delegateRoot.modelData : (delegateRoot.modelData.text || "")
                color: theme.ink
                font.family: theme.fontBody
                font.pixelSize: 14
                verticalAlignment: Text.AlignVCenter
                elide: Text.ElideRight
            }

            onClicked: {
                root.close()
                root.triggered(index)
            }

            Component.onCompleted: {
                list.implicitWidth = Math.max(list.implicitWidth, contentItem.implicitWidth + theme.sp6)
            }
        }
    }

    enter: Transition {
        ParallelAnimation {
            NumberAnimation { property: "opacity"; from: 0; to: 1; duration: 140; easing.type: Easing.OutCubic }
            NumberAnimation { property: "scale"; from: 0.96; to: 1; duration: 160; easing.type: Easing.OutCubic }
        }
    }
    exit: Transition {
        ParallelAnimation {
            NumberAnimation { property: "opacity"; from: 1; to: 0; duration: 100; easing.type: Easing.InCubic }
            NumberAnimation { property: "scale"; from: 1; to: 0.98; duration: 100; easing.type: Easing.InCubic }
        }
    }
}
