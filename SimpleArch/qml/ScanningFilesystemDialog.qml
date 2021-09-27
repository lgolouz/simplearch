import QtQuick 2.3
import QtQuick.Dialogs 1.3
import QtQuick.Controls 1.3

import com.example.enums 1.0
import com.example.models 1.0

Dialog {
    id: scanningFilesystemDialog

    title: "Scanning filesystem..."
    standardButtons: Dialog.Cancel
    width: 300

    Text {
        id: dialogText
        anchors.top: parent.top
    }

    ProgressBar {
        anchors.top: dialogText.bottom
        anchors.right: parent.right
        anchors.left: parent.left
        anchors.topMargin: 3
        indeterminate: true
    }

    Connections {
        target: ArchiverModel
        function onScanningFilesystem(fileName) {
            dialogText.text = fileName
        }
    }

    onVisibleChanged: {
        if (!visible && ArchiverModel.archiverState === ArchiverStates.PS_SCANNING_FILESYSTEM) {
            ArchiverModel.cancelOperation();
        }
    }
}
