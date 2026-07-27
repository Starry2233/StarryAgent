import QtQuick
import QtQuick.Controls

// CodeBlock — renders fenced code with a language label and copy button.
Rectangle {
    id: root

    property string language: ""
    property string code: ""
    property bool isDark: theme.dark
    readonly property real renderViewportLimit: (typeof maxRenderPageSize !== "undefined" && maxRenderPageSize > 0)
        ? maxRenderPageSize
        : 420

    function applyHighlighting() {
        if (typeof codeHighlighter === "undefined" || !codeHighlighter || !codeArea.textDocument)
            return
        codeHighlighter.applyToDocument(codeArea.textDocument, root.language, root.isDark)
    }

    function vlog(message) {
        if (typeof verboseLogging !== "undefined" && verboseLogging)
            console.log("[CodeBlock] " + message)
    }

    width: Math.max(0, parent ? parent.width : 400)
    radius: theme.rMd
    color: root.isDark ? "#1E1E1E" : "#F8F5F0"
    border.color: root.isDark ? "#3B342C" : "#D9CFBC"
    border.width: 1

    // Total height = language row (28px) + code viewport
    implicitHeight: 28 + codeFlick.height + theme.sp4

    // Language label + copy button row
    Row {
        id: langRow
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.leftMargin: theme.sp2
        anchors.rightMargin: theme.sp2
        height: 28
        spacing: theme.sp2

        Text {
            text: root.language ? root.language.toUpperCase() : "CODE"
            color: theme.inkSoft
            font.family: theme.fontMono
            font.pixelSize: 10
            font.letterSpacing: 1
            font.capitalization: Font.AllUppercase
        }
        Item { width: 1; height: 14 }
        Rectangle { width: 1; height: 14; color: theme.line }

        Item { width: 1; height: 1 }

        Text {
            id: copyLabel
            text: "copy"
            color: copyMa.containsMouse ? theme.clay : theme.inkSoft
            font.family: theme.fontMono
            font.pixelSize: 10
            Behavior on color {
                ColorAnimation { duration: 180; easing.type: Easing.OutCubic }
            }
            MouseArea {
                id: copyMa
                anchors.fill: parent
                anchors.margins: -4  // expand hit area slightly
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: {
                    clipboard.setText(root.code)
                    if (!clipboard.showCopyFeedback()) {
                        copyToast.visible = true
                        toastTimer.start()
                    }
                }
            }
        }
    }

    // Code area
    Flickable {
        id: codeFlick
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: langRow.bottom
        anchors.margins: theme.sp2
        height: Math.min(codeArea.contentHeight, root.renderViewportLimit)
        clip: true
        contentWidth: width
        contentHeight: codeArea.contentHeight
        boundsBehavior: Flickable.StopAtBounds
        interactive: contentHeight > height

        TextEdit {
            id: codeArea
            width: Math.max(0, codeFlick.width)
            text: root.code
            textFormat: TextEdit.PlainText
            readOnly: true
            selectByMouse: true
            selectByKeyboard: true
            activeFocusOnPress: true
            color: root.isDark ? "#D4D4D4" : "#1C1916"
            font.family: theme.fontMono
            font.pixelSize: 12
            wrapMode: Text.Wrap

            Component.onCompleted: root.applyHighlighting()
            onTextChanged: Qt.callLater(root.applyHighlighting)
        }
    }

    // Copy feedback toast
    Rectangle {
        id: copyToast
        visible: false
        anchors.centerIn: parent
        color: theme.ink
        radius: theme.rSm
        width: copyText.implicitWidth + theme.sp4
        height: copyText.implicitHeight + theme.sp4
        z: 10
        Text {
            id: copyText
            anchors.centerIn: parent
            text: "copied"
            color: theme.paper
            font.family: theme.fontBody
            font.pixelSize: 11
        }
    }
    Timer {
        id: toastTimer
        interval: 1200
        onTriggered: copyToast.visible = false
    }

    Component.onCompleted: vlog("completed lang=" + language
                                 + " codeLen=" + code.length
                                 + " limit=" + renderViewportLimit)
    Component.onDestruction: vlog("destroyed lang=" + language)
    onCodeChanged: vlog("code changed len=" + code.length)
    onLanguageChanged: Qt.callLater(root.applyHighlighting)
    onIsDarkChanged: Qt.callLater(root.applyHighlighting)
    onHeightChanged: vlog("height=" + height + " contentHeight=" + codeArea.contentHeight)
}
