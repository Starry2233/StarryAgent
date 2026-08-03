import QtQuick
import QtQuick.Controls

Item {
    id: root

    Rectangle { anchors.fill: parent; color: theme.pageOverlay }

    Flickable {
        anchors.fill: parent
        anchors.topMargin: theme.sp4
        contentHeight: content.implicitHeight + theme.sp6
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        Column {
            id: content
            width: Math.min(parent.width - theme.sp6 * 2, 720)
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: theme.sp3

            Text {
                width: parent.width
                text: qsTr("Scheduled tasks keep running in the background. When they finish, you will be notified and the tool calls and replies stay in the related conversation.")
                color: theme.inkSoft
                font.family: theme.fontBody
                font.pixelSize: 12
                wrapMode: Text.Wrap
            }

            Text {
                visible: Qt.platform.os === "android"
                width: parent.width
                text: qsTr("On Android, background runtime and battery optimization exceptions are also required, and some vendors add extra autostart limits. Open Settings > General > Background Runtime to grant the standard permissions. If it still does not work, refer to Don’t Kill My App.")
                color: theme.inkSoft
                font.family: theme.fontBody
                font.pixelSize: 12
                wrapMode: Text.Wrap
            }

            ToggleRow {
                title: qsTr("Global Scheduled Tasks")
                description: qsTr("When this switch is off, all scheduled tasks pause. When it is on, they continue on schedule.")
                checked: scheduledTasks.globalEnabled
                onToggled: scheduledTasks.globalEnabled = checked
            }

            Text {
                text: qsTr("Tasks")
                color: theme.inkSoft
                font.family: theme.fontBody
                font.pixelSize: 10
                font.letterSpacing: 1
                font.capitalization: Font.AllUppercase
                topPadding: theme.sp2
            }

            Repeater {
                model: scheduledTasks
                delegate: Rectangle {
                    required property string taskId
                    required property string prompt
                    required property string scope
                    required property string conversationTitle
                    required property string nextRun
                    required property int intervalMinutes
                    required property string recurrence
                    required property bool temp
                    required property bool enabled
                    required property string status
                    width: content.width
                    height: taskColumn.implicitHeight + theme.sp3 * 2
                    radius: theme.rSm
                    color: theme.surface
                    border.color: theme.line
                    border.width: 1

                    Column {
                        id: taskColumn
                        anchors.left: parent.left; anchors.right: parent.right
                        anchors.top: parent.top; anchors.margins: theme.sp3
                        spacing: theme.sp1

                        Row {
                            width: parent.width
                            spacing: theme.sp2

                            Text {
                                width: parent.width - taskSwitch.width - parent.spacing
                                text: conversationTitle.length > 0 ? conversationTitle : qsTr("Deleted Conversation")
                                color: theme.ink
                                font.family: theme.fontBody
                                font.pixelSize: 13
                                font.weight: Font.Medium
                                elide: Text.ElideRight
                                verticalAlignment: Text.AlignVCenter
                            }

                            MiuixSwitch {
                                id: taskSwitch
                                anchors.verticalCenter: parent.verticalCenter
                                checked: enabled
                                onToggled: scheduledTasks.setTaskEnabled(taskId, checked)
                            }
                        }
                        Text {
                            width: parent.width
                            text: prompt
                            color: theme.inkSoft
                            font.family: theme.fontBody
                            font.pixelSize: 12
                            wrapMode: Text.Wrap
                            maximumLineCount: 2
                            elide: Text.ElideRight
                        }
                        Row {
                            width: parent.width
                            spacing: theme.sp2
                            Text {
                                width: parent.width - deleteButton.width - parent.spacing
                                text: (scope === "global" ? qsTr("Global") : qsTr("This Conversation")) + "  |  "
                                      + qsTr("Next %1").arg(nextRun.replace("T", " "))
                                      + "  |  "
                                      + (temp ? qsTr("One-Time Temporary")
                                                : recurrence === "daily" ? qsTr("Daily")
                                                : recurrence === "weekly" ? qsTr("Weekly")
                                                : recurrence === "monthly" ? qsTr("Monthly")
                                                : recurrence === "interval" ? qsTr("Every %1 Minutes").arg(intervalMinutes)
                                                : qsTr("One-Time"))
                                color: theme.inkSoft
                                font.family: theme.fontMono
                                font.pixelSize: 10
                                elide: Text.ElideRight
                            }
                            ThemeButton {
                                id: deleteButton
                                compact: true
                                danger: true
                                width: 32
                                height: 28
                                text: "\u00d7"
                                onClicked: scheduledTasks.deleteTask(taskId)
                            }
                        }
                    }
                }
            }

            Text {
                visible: scheduledTasks.count === 0
                width: parent.width
                topPadding: theme.sp5
                horizontalAlignment: Text.AlignHCenter
                text: qsTr("No scheduled tasks yet")
                color: theme.inkSoft
                font.family: theme.fontBody
                font.pixelSize: 13
            }
        }
    }
}
