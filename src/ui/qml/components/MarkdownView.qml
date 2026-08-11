import QtQuick
import QtQuick.Controls

// MarkdownView — renders assistant replies with proper markdown formatting,
// code blocks, and LaTeX blocks. Delegates parsing to the C++ MarkdownParser.
Item {
    id: root

    property string rawText: ""
    property bool isStreaming: false
    // History rows can construct their rich-text segments across frames. The
    // active streaming row and initial tail restoration opt out so their height
    // is immediately reliable for bottom anchoring.
    property bool deferSegments: false
    property var segments: []
    property int refreshToken: 0
    property string pendingText: ""
    property bool pendingSuppressedTrailingTable: false
    signal imageActivated(string source, string alt)
    signal selectionRequested(string text)

    function vlog(message) {
        if (typeof verboseLogging !== "undefined" && verboseLogging)
            console.log("[MarkdownView] " + message)
    }

    function isTableRow(line) {
        const trimmed = (line || "").trim()
        return trimmed.split("|").length - 1 >= 2
    }

    function isTableSeparator(line) {
        const trimmed = (line || "").trim()
        if (!trimmed || trimmed.indexOf("|") === -1)
            return false
        for (let i = 0; i < trimmed.length; ++i) {
            const ch = trimmed[i]
            if (ch !== "|" && ch !== "-" && ch !== ":" && ch !== " " && ch !== "\t")
                return false
        }
        return true
    }

    function trailingOpenTableStart(text) {
        if (!text || text.length === 0)
            return -1

        const lines = text.split("\n")
        let offset = 0
        let inFence = false
        let trailingStart = -1

        for (let i = 0; i < lines.length; ++i) {
            const line = lines[i]
            const trimmed = line.trim()
            const lineStart = offset
            offset += line.length + (i < lines.length - 1 ? 1 : 0)

            if (trimmed.startsWith("```")) {
                inFence = !inFence
                continue
            }
            if (inFence)
                continue

            if (i + 1 >= lines.length)
                continue

            if (!root.isTableRow(line) || !root.isTableSeparator(lines[i + 1]))
                continue

            let j = i + 2
            while (j < lines.length && root.isTableRow(lines[j]))
                ++j

            if (j === lines.length)
                trailingStart = lineStart
        }

        return trailingStart
    }

    function computePendingSegments() {
        let text = rawText || ""
        let suppressedTrailingTable = false
        if (isStreaming) {
            const tableStart = trailingOpenTableStart(text)
            if (tableStart >= 0) {
                text = text.substring(0, tableStart)
                suppressedTrailingTable = true
            }
        }
        pendingText = text
        pendingSuppressedTrailingTable = suppressedTrailingTable
    }

    function applyPendingSegments() {
        root.segments = markdownParser.parse(pendingText)
        vlog("refresh rawLen=" + rawText.length
             + " parsedLen=" + pendingText.length
             + " segments=" + root.segments.length
             + " streaming=" + isStreaming
             + " trailingTableHeld=" + pendingSuppressedTrailingTable)
    }

    function scheduleRefresh() {
        computePendingSegments()
        refreshToken += 1
        refreshTimer.restart()
    }

    function renderMarkdownText(raw) {
        if (typeof codeHighlighter === "undefined" || !codeHighlighter)
            return raw || ""
        return codeHighlighter.renderMarkdown(raw || "", theme.dark)
    }
    onRawTextChanged: scheduleRefresh()
    onIsStreamingChanged: scheduleRefresh()

    Component.onCompleted: {
        vlog("completed width=" + width)
        scheduleRefresh()
    }
    Component.onDestruction: vlog("destroyed")

    Timer {
        id: refreshTimer
        interval: 0
        repeat: false
        onTriggered: root.applyPendingSegments()
    }

    Column {
        id: contentCol
        anchors.left: parent.left
        anchors.right: parent.right
        spacing: theme.sp2

        Repeater {
            model: root.segments
            delegate: Item {
                id: segmentDelegate
                width: Math.max(0, parent ? parent.width : root.width)
                height: contentLoader.item ? Math.max(0, contentLoader.item.implicitHeight) : 0
                readonly property var segmentData: modelData
                readonly property int segmentIndex: index

                Component.onCompleted: root.vlog("segment create kind=" + segmentData.kind
                                                  + " textLen=" + ((segmentData.text || "").length)
                                                  + " width=" + width
                                                  + " height=" + height)
                Component.onDestruction: root.vlog("segment destroy kind=" + segmentData.kind)

                Loader {
                    id: contentLoader
                    anchors.left: parent.left
                    anchors.right: parent.right
                    asynchronous: root.deferSegments && !(typeof list !== "undefined" && list && (list.autoScroll || list.shouldKeepBottomPinned))
                    onLoaded: {
                        if (!item)
                            return
                        item.segmentData = segmentDelegate.segmentData
                        item.segmentIndex = segmentDelegate.segmentIndex
                    }
                    sourceComponent: {
                        if (segmentDelegate.segmentData.kind === "markdown" && (segmentDelegate.segmentData.text || "").length > 0)
                            return markdownComponent
                        if (segmentDelegate.segmentData.kind === "image")
                            return imageComponent
                        if (segmentDelegate.segmentData.kind === "code")
                            return codeComponent
                        if (segmentDelegate.segmentData.kind === "table")
                            return markdownComponent
                        if (segmentDelegate.segmentData.kind === "rule")
                            return ruleComponent
                        if (segmentDelegate.segmentData.kind === "latex")
                            return latexComponent
                        return null
                    }
                }
            }
        }
    }

    implicitHeight: contentCol.implicitHeight

    Component {
        id: markdownComponent
        Text {
            property var segmentData
            property int segmentIndex: -1
            width: Math.max(0, parent.width)
            text: root.renderMarkdownText(segmentData ? segmentData.text : "")
            textFormat: Text.RichText
            color: theme.ink
            font.family: theme.fontBody
            font.pixelSize: 14
            wrapMode: Text.Wrap
            onLinkActivated: (link) => Qt.openUrlExternally(link)
            Component.onCompleted: root.vlog("markdown text block len=" + text.length
                                              + " implicitHeight=" + implicitHeight)

            MouseArea {
                anchors.fill: parent
                acceptedButtons: Qt.RightButton
                cursorShape: Qt.IBeamCursor
                onClicked: function(mouse) {
                    if (mouse.button !== Qt.RightButton)
                        return
                    root.selectionRequested(segmentData ? (segmentData.text || "") : "")
                }
            }
        }
    }

    Component {
        id: imageComponent
        Item {
            property var segmentData
            property int segmentIndex: -1
            width: Math.max(0, parent.width)
            implicitHeight: previewImage.height

            Image {
                id: previewImage
                width: Math.min(parent.width, 360)
                height: 220
                fillMode: Image.PreserveAspectFit
                source: segmentData ? (segmentData.url || "") : ""
                asynchronous: true
                cache: false
                onStatusChanged: root.vlog("image status=" + status + " source=" + source)
            }

            MouseArea {
                anchors.fill: previewImage
                preventStealing: true
                cursorShape: Qt.PointingHandCursor
                onClicked: root.imageActivated(segmentData ? (segmentData.url || "") : "",
                                               segmentData ? (segmentData.alt || "") : "")
            }
        }
    }

    Component {
        id: codeComponent
        CodeBlock {
            property var segmentData
            property int segmentIndex: -1
            anchors.left: parent.left
            anchors.right: parent.right
            language: segmentData ? (segmentData.lang || "") : ""
            code: segmentData ? (segmentData.text || "") : ""
        }
    }

    Component {
        id: ruleComponent
        Item {
            property var segmentData
            property int segmentIndex: -1
            anchors.left: parent.left
            anchors.right: parent.right
            implicitHeight: theme.sp4

            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                height: 1
                color: theme.line
                opacity: 0.9
            }
        }
    }

    Component {
        id: latexComponent
        LatexBlock {
            property var segmentData
            property int segmentIndex: -1
            anchors.left: parent.left
            anchors.right: parent.right
            math: segmentData ? (segmentData.text || "") : ""
            isBlock: segmentData ? (segmentData.block || false) : false
        }
    }
}
