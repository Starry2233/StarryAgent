import QtQuick
import QtQuick.Controls
import QtQuick.Window

Window {
    id: root

    width: 920
    height: 640
    minimumWidth: 520
    minimumHeight: 360
    visible: false
    color: theme.paper
    title: selectionTitle && selectionTitle.length > 0 ? selectionTitle : qsTr("Text Selection")

    property string selectionTitle: ""
    property string selectionText: ""
    property string pendingSelectionText: ""
    property bool contentReady: false
    property bool dark: false

    function openFor(title, text) {
        selectionTitle = title || ""
        selectionText = ""
        pendingSelectionText = text || ""
        contentReady = false
        visible = true
        raise()
        requestActivate()
        loadTextTimer.restart()
    }

    Timer {
        id: loadTextTimer
        interval: 80
        repeat: false
        onTriggered: {
            if (!root.visible || root.width <= 0 || root.height <= 0) {
                restart()
                return
            }
            root.selectionText = root.pendingSelectionText
            root.contentReady = true
        }
    }

    Rectangle {
        anchors.fill: parent
        color: theme.paper
    }

    Column {
        anchors.fill: parent
        anchors.margins: theme.sp3
        spacing: theme.sp3

        Rectangle {
            width: Math.max(0, parent.width)
            height: 44
            radius: theme.rMd
            color: theme.surface
            border.color: theme.line
            border.width: 1

            Row {
                anchors.fill: parent
                anchors.leftMargin: theme.sp3
                anchors.rightMargin: theme.sp3
                spacing: theme.sp2

                Text {
                    width: Math.max(0, parent.width - copyButton.width - parent.spacing)
                    anchors.verticalCenter: parent.verticalCenter
                    text: root.selectionTitle
                    color: theme.ink
                    font.family: theme.fontBody
                    font.pixelSize: 14
                    font.weight: Font.DemiBold
                    elide: Text.ElideRight
                }

                Rectangle {
                    id: copyButton
                    width: 56
                    height: 28
                    anchors.verticalCenter: parent.verticalCenter
                    radius: theme.rSm
                    color: copyMouse.containsMouse ? theme.clayDeep : theme.clay
                    border.color: theme.clayDeep
                    border.width: 1

                    Text {
                        anchors.centerIn: parent
                        text: "Copy"
                        color: "white"
                        font.family: theme.fontBody
                        font.pixelSize: 12
                    }

                    MouseArea {
                        id: copyMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            clipboard.setText(root.selectionText)
                            if (!clipboard.showCopyFeedback())
                                toast.showMessage("Copied")
                        }
                    }
                }
            }
        }

        Rectangle {
            width: Math.max(0, parent.width)
            height: Math.max(0, parent.height - 44 - theme.sp3)
            radius: theme.rLg
            color: theme.surface
            border.color: theme.line
            border.width: 1

            Loader {
                id: selectionLoader
                anchors.fill: parent
                anchors.margins: theme.sp2
                active: root.contentReady && width > 0 && height > 0
                sourceComponent: selectionAreaComponent
            }

            Text {
                anchors.centerIn: parent
                visible: !selectionLoader.active
                text: qsTr("Loading")
                textFormat: Text.PlainText
                renderType: Text.NativeRendering
                color: theme.inkSoft
                font.family: theme.fontBody
                font.pixelSize: 13
            }

            Component {
                id: selectionAreaComponent
                ScrollView {
                    clip: true

                    TextArea {
                        width: Math.max(0, selectionLoader.width)
                        text: root.selectionText
                        textFormat: TextEdit.PlainText
                        renderType: Text.NativeRendering
                        readOnly: true
                        selectByMouse: true
                        selectByKeyboard: true
                        activeFocusOnPress: true
                        wrapMode: TextEdit.Wrap
                        color: theme.ink
                        selectedTextColor: "white"
                        selectionColor: theme.clay
                        font.family: theme.fontBody
                        font.pixelSize: 14
                        background: null
                    }
                }
            }
        }
    }
}
