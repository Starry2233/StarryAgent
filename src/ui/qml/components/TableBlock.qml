import QtQuick
import QtQuick.Controls

Rectangle {
    id: root

    property string tableText: ""
    property bool isDark: theme.dark
    readonly property real maxViewportHeight: (typeof maxRenderPageSize !== "undefined" && maxRenderPageSize > 0)
        ? maxRenderPageSize
        : 420

    function vlog(message) {
        if (typeof verboseLogging !== "undefined" && verboseLogging)
            console.log("[TableBlock] " + message)
    }

    width: Math.max(0, parent ? parent.width : 400)
    radius: theme.rMd
    color: root.isDark ? "#1E1E1E" : "#F8F5F0"
    border.color: root.isDark ? "#3B342C" : "#D9CFBC"
    border.width: 1
    implicitHeight: 28 + flick.height + theme.sp2

    Row {
        id: headerRow
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.leftMargin: theme.sp2
        anchors.rightMargin: theme.sp2
        height: 28
        spacing: theme.sp2

        Text {
            text: "TABLE"
            color: theme.inkSoft
            font.family: theme.fontMono
            font.pixelSize: 10
            font.letterSpacing: 1
            font.capitalization: Font.AllUppercase
        }
    }

    Flickable {
        id: flick
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: headerRow.bottom
        anchors.margins: theme.sp2
        height: Math.min(tableTextItem.paintedHeight, root.maxViewportHeight)
        clip: true
        contentWidth: tableTextItem.paintedWidth
        contentHeight: tableTextItem.paintedHeight
        boundsBehavior: Flickable.StopAtBounds
        interactive: contentWidth > width || contentHeight > height

        Text {
            id: tableTextItem
            text: root.tableText
            color: root.isDark ? "#D4D4D4" : "#1C1916"
            font.family: theme.fontMono
            font.pixelSize: 12
            textFormat: Text.PlainText
            wrapMode: Text.NoWrap
        }
    }

    Component.onCompleted: vlog("completed len=" + tableText.length + " limit=" + maxViewportHeight)
    Component.onDestruction: vlog("destroyed")
    onTableTextChanged: vlog("table changed len=" + tableText.length)
    onHeightChanged: vlog("height=" + height + " contentHeight=" + tableTextItem.paintedHeight)
}
