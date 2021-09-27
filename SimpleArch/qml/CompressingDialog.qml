import QtQuick 2.3
import QtQuick.Dialogs 1.3
import QtQuick.Controls 1.3

import com.example.enums 1.0
import com.example.models 1.0

Dialog {
    id: compressingDialog

    title: "Compressing..."
    standardButtons: Dialog.Cancel
    width: 300

    Text {
        id: dialogText
        anchors.top: parent.top
    }

    ProgressBar {
        id: fileProgress

        anchors.top: dialogText.bottom
        anchors.right: parent.right
        anchors.left: parent.left
        anchors.topMargin: 3
        minimumValue: 0
        maximumValue: 1
    }

    Text {
        id: filesCounter
        anchors.top: fileProgress.bottom
        anchors.topMargin: 9
    }

    ProgressBar {
        id: overallProgress

        anchors.top: filesCounter.bottom
        anchors.right: parent.right
        anchors.left: parent.left
        anchors.topMargin: 3
        minimumValue: 0
        maximumValue: 1
    }

    Connections {
        target: ArchiverModel

        function onOverallProgress(current, whole) {
            filesCounter.text = "[%1 / %2]".arg(current).arg(whole)
            overallProgress.value = whole === 0 ? 0 : current / whole
        }

        function onFileProgress(fileName, current, whole) {
            dialogText.text = fileName
            fileProgress.value = whole === 0 ? 0 : current / whole
        }
    }

    onVisibleChanged: {
        if (!visible && ArchiverModel.archiverState === ArchiverStates.PS_COMPRESSING) {
            ArchiverModel.cancelOperation();
        }
    }
}
