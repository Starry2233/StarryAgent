import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// First-launch directory picker. PLAN.md's three choices are Android-only
// (/sdcard/...); on Windows we offer three sensible presets plus the default.
// Doubao-style: warm surface card, soft radii, hairline border, gentle motion.
Item {
    id: root

    // three presets resolved from C++ (Config.presetRoots())
    property var presets: config.presetRoots()
    property int recommendedIndex: 0

    Rectangle {
        id: card
        anchors.centerIn: parent
        width: 520
        height: col.implicitHeight + theme.sp5 * 2
        color: theme.surface
        radius: theme.rLg
        border.color: theme.line
        border.width: 1

        Column {
            id: col
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            anchors.margins: theme.sp5
            spacing: theme.sp4

            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: "\u2726\uFE0E"
                color: theme.clay
                font.family: theme.fontDisplay
                font.pixelSize: 28
            }
            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: qsTr("Choose where StarryAgent lives")
                color: theme.ink
                font.family: theme.fontDisplay
                font.pixelSize: 22
                font.weight: Font.Medium
            }
            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: qsTr("This becomes your .starryagent directory — workspace, skills, memories, and tools.")
                color: theme.inkSoft
                font.family: theme.fontBody
                font.pixelSize: 12
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.Wrap
                Layout.fillWidth: true
            }

            Repeater {
                model: root.presets
                delegate: Rectangle {
                    property bool isRecommended: index === root.recommendedIndex
                    width: col.width
                    height: 56
                    color: isRecommended ? theme.accent(0.06) : theme.surfaceAlt
                    radius: theme.rMd
                    border.color: isRecommended ? theme.clay : theme.line
                    border.width: 1

                    Column {
                        anchors.left: parent.left
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.leftMargin: theme.sp4
                        anchors.right: parent.right
                        anchors.rightMargin: theme.sp4
                        spacing: 2
                        Text {
                            text: {
                                if (Qt.platform.os === "android") {
                                    if (index === 0)
                                        return qsTr("App private")
                                    if (index === 1)
                                        return qsTr("Android/data")
                                    return qsTr("Shared internal storage")
                                }
                                return isRecommended ? qsTr("Recommended") : (index === 1 ? qsTr("Home folder") : qsTr("Documents"))
                            }
                            color: isRecommended ? theme.clay : theme.inkSoft
                            font.family: theme.fontBody
                            font.pixelSize: 10
                            font.letterSpacing: 1
                            font.capitalization: Font.AllUppercase
                        }
                        Text {
                            text: modelData
                            color: theme.ink
                            font.family: theme.fontMono
                            font.pixelSize: 11
                            elide: Text.ElideMiddle
                            width: parent.width
                        }
                    }

                    Text {
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.rightMargin: theme.sp4
                        text: "\u2192"
                        color: isRecommended ? theme.clay : theme.inkSoft
                        font.family: theme.fontBody
                        font.pixelSize: 18
                    }

                    MouseArea {
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: androidPermissionBridge.ensureRootAccessAndSetRoot(modelData)
                    }
                }
            }
        }
    }
}
