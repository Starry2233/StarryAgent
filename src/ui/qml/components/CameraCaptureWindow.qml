import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtMultimedia
import StarryAgent 1.0

StarryWindow {
    id: root

    property bool dark: false
    signal captured(string path)

    width: 900
    height: 640
    minimumWidth: 720
    minimumHeight: 520
    visible: false
    title: qsTr("Camera")
    color: dark ? "#161311" : "#F7F1E7"

    Camera {
        id: camera
    }

    ImageCapture {
        id: imageCapture
        onImageSaved: function(requestId, path) {
            root.captured(path)
            root.hide()
        }
    }

    CaptureSession {
        camera: camera
        imageCapture: imageCapture
        videoOutput: viewfinder
    }

    onVisibleChanged: {
        if (visible)
            camera.start()
        else
            camera.stop()
    }

    Rectangle {
        anchors.fill: parent
        color: root.color

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 16
            spacing: 12

            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                radius: 8
                color: dark ? "#241F1A" : "#EDE3D2"
                border.color: dark ? "#3B342C" : "#D9CFBC"
                border.width: 1
                clip: true

                VideoOutput {
                    id: viewfinder
                    anchors.fill: parent
                    fillMode: VideoOutput.PreserveAspectCrop
                }

                Text {
                    anchors.centerIn: parent
                    visible: !camera.active
                    text: qsTr("No camera detected")
                    color: dark ? "#E8E1D0" : "#1C1916"
                    font.pixelSize: 18
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 12

                Button {
                    text: qsTr("Cancel")
                    onClicked: root.hide()
                }

                Item { Layout.fillWidth: true }

                Button {
                    text: qsTr("Capture")
                    enabled: camera.active
                    onClicked: imageCapture.captureToFile()
                }
            }
        }
    }
}
