import QtQuick
import QtQuick.Window

Window {
    id: root

    width: 960
    height: 720
    minimumWidth: 420
    minimumHeight: 320
    visible: false
    color: "black"
    title: imageName && imageName.length > 0 ? imageName : qsTr("Image Preview")

    property string imageSource: ""
    property string imageName: ""
    property bool dark: false

    signal downloadRequested(string source, string name)

    function openFor(source, name) {
        imageSource = source || ""
        imageName = name || ""
        visible = true
        raise()
        requestActivate()
    }

    ImagePreviewPane {
        anchors.fill: parent
        imageSource: root.imageSource
        imageName: root.imageName
        dark: root.dark
        showCloseButton: false
        onCloseRequested: root.close()
        onDownloadRequested: function(source, name) {
            root.downloadRequested(source, name)
        }
    }
}
