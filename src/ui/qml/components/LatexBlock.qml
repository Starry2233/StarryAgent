import QtQuick
import QtQuick.Controls

// LatexBlock — renders a LaTeX segment. Block mode ($$...$$) shows on a
// card-like surface; inline mode ($...$) renders inline with text.
Item {
    id: root

    property string math: ""
    property bool isBlock: false
    property bool isDark: theme.dark

    implicitHeight: root.isBlock ? (28 + mathArea.implicitHeight) : inlineText.implicitHeight
    width: parent ? parent.width : 400

    // Inline LaTeX
    Text {
        id: inlineText
        visible: !root.isBlock
        width: parent.width
        text: root.math.length > 0 ? "$" + root.math + "$" : ""
        color: theme.ink
        font.family: theme.fontMono
        font.pixelSize: 13
        font.italic: true
        wrapMode: Text.Wrap
    }

    // Block LaTeX
    Rectangle {
        id: blockCard
        visible: root.isBlock
        width: parent.width
        height: root.implicitHeight
        radius: theme.rMd
        color: root.isDark ? "#1E1E1E" : "#F8F5F0"
        border.color: root.isDark ? "#3B342C" : "#D9CFBC"
        border.width: 1

        Text {
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.leftMargin: theme.sp3
            anchors.topMargin: theme.sp1
            height: 20
            text: "LATEX"
            color: theme.inkSoft
            font.family: theme.fontMono
            font.pixelSize: 10
            font.letterSpacing: 1
            font.capitalization: Font.AllUppercase
        }

        Text {
            id: mathArea
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.leftMargin: theme.sp3
            anchors.rightMargin: theme.sp3
            anchors.bottomMargin: theme.sp2
            text: root.math
            color: root.isDark ? "#D4D4D4" : "#1C1916"
            font.family: theme.fontMono
            font.pixelSize: 13
            wrapMode: Text.Wrap
        }
    }
}
