import QtQuick
import QtQuick.Controls

Item {
    id: root

    property string imageSource: ""
    property string imageName: ""
    property bool dark: false
    property bool showCloseButton: true

    signal closeRequested()
    signal downloadRequested(string source, string name)

    Rectangle {
        anchors.fill: parent
        color: Qt.rgba(0, 0, 0, dark ? 0.92 : 0.82)
    }

    Flickable {
        id: viewport
        anchors.fill: parent
        anchors.topMargin: 56
        anchors.bottomMargin: 20
        anchors.leftMargin: 20
        anchors.rightMargin: 20
        clip: true
        contentWidth: contentFrame.width
        contentHeight: contentFrame.height

        Item {
            id: contentFrame
            width: viewport.width
            height: viewport.height

            Image {
                id: preview
                anchors.fill: parent
                source: root.imageSource ? root.imageSource : ""
                fillMode: Image.PreserveAspectFit
                asynchronous: true
                cache: false
                sourceSize.width: Math.max(960, parent.width)
                sourceSize.height: Math.max(960, parent.height)
            }
        }
    }

    Row {
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.topMargin: 12
        anchors.rightMargin: 12
        spacing: 8

        Rectangle {
            width: 36
            height: 36
            radius: 18
            color: Qt.rgba(1, 1, 1, dark ? 0.16 : 0.20)
            border.color: Qt.rgba(1, 1, 1, 0.18)

            Text {
                anchors.centerIn: parent
                text: "\u2b07"
                color: "white"
                font.family: theme.fontBody
                font.pixelSize: 16
            }

            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                onClicked: root.downloadRequested(root.imageSource, root.imageName)
            }
        }

        Rectangle {
            visible: root.showCloseButton
            width: 36
            height: 36
            radius: 18
            color: Qt.rgba(1, 1, 1, dark ? 0.16 : 0.20)
            border.color: Qt.rgba(1, 1, 1, 0.18)

            Text {
                anchors.centerIn: parent
                text: "\u00d7"
                color: "white"
                font.family: theme.fontBody
                font.pixelSize: 18
            }

            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                onClicked: root.closeRequested()
            }
        }
    }
}
