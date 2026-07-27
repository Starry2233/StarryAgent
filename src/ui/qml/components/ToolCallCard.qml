import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// ToolCallCard — the per-tool-call UI. States track the dispatch rule:
//   composing  → args still streaming (DO NOT execute)
//   pending    → args complete & validated, awaiting user approval
//   running    → dispatched, executing
//   done       → finished, result available (collapsed by default)
//   denied     → user rejected
//   error      → tool execution failed
//
// In "pending", approve/deny buttons are shown UNLESS the tool does not
// require permission. bypassPermissions (a Settings flag) skips approval
// entirely — the parent auto-dispatches on toolCallReady.
Rectangle {
    id: card

    property string toolCallId
    property string toolName
    property string argsText   // pretty-printed JSON string for display
    property string status: "composing"
    property string result: ""
    property bool needsApproval: true   // tool.permissionRequired
    readonly property real renderViewportLimit: (typeof maxRenderPageSize !== "undefined" && maxRenderPageSize > 0)
        ? maxRenderPageSize
        : 420

    signal approve()
    signal deny()

    function vlog(message) {
        if (typeof verboseLogging !== "undefined" && verboseLogging)
            console.log("[ToolCallCard] " + message)
    }

    width: parent ? parent.width : 400
    // Height is computed from the visible sections directly. The earlier
    // `void [...]` dependency hack was unreliable (qmlsc can optimize a
    // discarded expression away), and Column.implicitHeight lags a frame
    // behind a child's visibility flip — together they left the card at its
    // old (shorter) height when state changed, spilling the new section into
    // the next delegate. Referencing each section's height + visibility
    // directly updates the moment the properties flip.
    height: headerRow.height
            + (argsPreview.visible ? argsPreview.height + theme.sp2 : 0)
            + (resultViewport.visible ? resultViewport.height + theme.sp2 : 0)
            + (resultRevealRow.visible ? resultRevealRow.height + theme.sp2 : 0)
            + (approveRow.visible ? approveRow.height + theme.sp2 : 0)
            + theme.sp3 * 2
    radius: theme.rMd
    border.width: 1
    border.color: {
        if (status === "error") return theme.clayDeep
        if (status === "done") return theme.line
        if (status === "pending") return theme.clay
        return theme.line
    }
    color: {
        if (status === "composing") return theme.surfaceAlt
        if (status === "pending") return theme.accent(0.05)
        return theme.surface
    }

    Column {
        id: layout
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        anchors.margins: theme.sp3
        spacing: theme.sp2

        Row {
            id: headerRow
            spacing: theme.sp2
            Text {
                text: {
                    if (status === "composing") return "\u29d6"   // ⧖ hourglass
                    if (status === "pending")   return "\u2754"   // ❔
                    if (status === "running")   return "\u21bb"   // ↻
                    if (status === "done")      return "\u2713\uFE0E"   // ✓
                    if (status === "denied")    return "\u2717"   // ✗
                    if (status === "error")     return "!"
                    return ""
                }
                color: status === "error" || status === "denied" ? theme.clayDeep
                       : status === "done" ? theme.moss
                       : theme.clay
                font.family: theme.fontBody
                font.pixelSize: 13
            }
            Text {
                text: card.toolName
                color: theme.ink
                font.family: theme.fontMono
                font.pixelSize: 12
                font.weight: Font.Medium
            }
            Text {
                // status label
                text: {
                    if (status === "composing") return qsTr("composing\u2026")
                    if (status === "pending")   return qsTr("awaiting approval")
                    if (status === "running")   return qsTr("running\u2026")
                    if (status === "done")      return qsTr("done")
                    if (status === "denied")    return qsTr("denied")
                    if (status === "error")     return qsTr("failed")
                    return ""
                }
                color: theme.inkSoft
                font.family: theme.fontBody
                font.pixelSize: 10
                font.letterSpacing: 1
                font.capitalization: Font.AllUppercase
            }
        }

        // args preview (hidden while composing — no half-args shown)
        Text {
            id: argsPreview
            width: parent.width
            visible: status !== "composing" && argsText.length > 0
            text: card.argsText
            color: theme.inkSoft
            font.family: theme.fontMono
            font.pixelSize: 11
            wrapMode: Text.Wrap
            maximumLineCount: 4
            elide: Text.ElideRight
        }

        // result (done/error state) — collapsed by default, click to expand
        Flickable {
            id: resultViewport
            width: parent.width
            visible: (status === "done" || status === "error") && result.length > 0 && resultReveal.checked
            height: visible ? Math.min(resultText.contentHeight, card.renderViewportLimit) : 0
            clip: true
            contentWidth: width
            contentHeight: resultText.contentHeight
            boundsBehavior: Flickable.StopAtBounds
            interactive: contentHeight > height

            TextEdit {
                id: resultText
                width: Math.max(0, resultViewport.width)
                readOnly: true
                selectByMouse: true
                selectByKeyboard: true
                activeFocusOnPress: true
                text: resultViewport.visible ? card.result : ""
                color: status === "error" ? theme.clayDeep : theme.inkSoft
                font.family: theme.fontMono
                font.pixelSize: 11
                wrapMode: Text.Wrap
            }
        }
        Row {
            id: resultRevealRow
            visible: (status === "done" || status === "error") && result.length > 0
            spacing: theme.sp1
            MiuixCheckBox {
                id: resultReveal
                checked: false
                onToggled: {}
            }
            Text {
                anchors.verticalCenter: resultReveal.verticalCenter
                text: status === "error" ? qsTr("show error") : qsTr("show result")
                color: status === "error" ? theme.clayDeep : theme.inkSoft
                font.family: theme.fontBody
                font.pixelSize: 10
                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: resultReveal.checked = !resultReveal.checked
                }
            }
        }

        // approve / deny buttons (pending state only)
        Row {
            id: approveRow
            visible: status === "pending" && card.needsApproval
            spacing: theme.sp2
            Button {
                text: qsTr("Approve")
                onClicked: card.approve()
                contentItem: Text {
                    text: parent.text
                    color: "white"
                    font.family: theme.fontBody
                    font.pixelSize: 12
                }
                background: Rectangle {
                    color: parent.down ? theme.clayDeep : theme.clay
                    radius: theme.rPill
                    implicitHeight: 28
                    implicitWidth: 84
                }
            }
            Button {
                text: qsTr("Deny")
                onClicked: card.deny()
                contentItem: Text {
                    text: parent.text
                    color: theme.ink
                    font.family: theme.fontBody
                    font.pixelSize: 12
                }
                background: Rectangle {
                    color: parent.down ? theme.surfaceAlt : theme.surface
                    radius: theme.rPill
                    border.color: theme.line
                    border.width: 1
                    implicitHeight: 28
                    implicitWidth: 64
                }
            }
        }
    }

    Component.onCompleted: vlog("completed tool=" + toolName
                                 + " status=" + status
                                 + " argsLen=" + argsText.length
                                 + " resultLen=" + result.length)
    Component.onDestruction: vlog("destroyed tool=" + toolName)
    onStatusChanged: vlog("status=" + status + " tool=" + toolName)
    onResultChanged: vlog("result changed len=" + result.length + " tool=" + toolName)
}
