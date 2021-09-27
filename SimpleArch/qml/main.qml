import QtQuick 2.15
import QtQuick.Window 2.15
import QtQuick.Controls 1.4
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Dialogs 1.0

import "."
import com.example.enums 1.0
import com.example.models 1.0

ApplicationWindow {
    width: 1024
    height: 600
    visible: true
    title: qsTr("SimpleArch")

    footer: Item {
        id: pathItem

        width: parent.width
        height: currentDirInput.height * 1.2

        Text {
            id: pathLabel

            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
            text: "Path: "
        }

        TextInput {
            id: currentDirInput

            anchors.left: pathLabel.right
            anchors.leftMargin: 3
            anchors.verticalCenter: parent.verticalCenter
            text: filesystemView.model.currentDir
        }
    }

    header: ToolBar {
        enabled: !fileDialog.visible

        RowLayout {
            Button {
                text: FilesystemDirModel.browsingFilesystem ? "Compress selected" : "Decompress selected"
                onClicked: {
                    fileDialog.open()
                }
            }
            Button {
                id: decompressButton

                property bool decompressWholeFile: false
                text: "Decompress"
                onClicked: {
                    decompressWholeFile = true;
                    fileDialog.open();
                }
            }

            Repeater {
                model: FilesystemDirModel.drives
                delegate: Button {
                    text: modelData
                    onClicked: {
                        FilesystemDirModel.currentDir = text
                    }
                }
            }

            Text {
                text: "Compression level:"
            }

            ComboBox {
                id: compressionLevel

                model: ArchiverModel.compressionLevels
                currentIndex: 6
            }
        }
    }

    TableView {
        id: filesystemView

        anchors.fill: parent
        selectionMode: SelectionMode.SingleSelection
        model: FilesystemDirModel
        enabled: !fileDialog.visible

        TableViewColumn {
            title: ""
            role: "EntrySelectedRole"
            width: filesystemView.width * 0.03
            delegate: Item {
                Text {
                    anchors.centerIn: parent
                    text: styleData.value.selected ? "√" : ""
                }
            }
        }

        TableViewColumn {
            title: "Name"
            role: "DirEntryRole"
            width: filesystemView.width * 0.85
            delegate: Item {
                Image {
                    id: iconImage

                    source: styleData.value.iconUri
                    sourceSize {
                        width: parent.height
                        height: parent.height
                    }

                    Text {
                        anchors.leftMargin: 3
                        anchors.left: iconImage.right
                        anchors.verticalCenter: parent.verticalCenter
                        text: styleData.value.fileName
                    }
                }
            }
        }

        TableViewColumn {
            title: "Size"
            role: "EntrySizeRole"
            width: filesystemView.width * 0.1
            delegate: Item {
                Text {
                    anchors.fill: parent
                    text: styleData.value.size
                    horizontalAlignment: Text.AlignRight
                }
            }
        }

        Keys.onSpacePressed: {
            model.toggleSelection(currentRow)
        }

        Keys.onEnterPressed: {
            model.enterDirectory(currentRow)
        }

        Keys.onReturnPressed: {
            model.enterDirectory(currentRow)
        }

        onDoubleClicked: {
            model.enterDirectory(currentRow)
        }
    }

    ScanningFilesystemDialog {
        visible: ArchiverModel.archiverState === ArchiverStates.PS_SCANNING_FILESYSTEM
    }

    CompressingDialog {
        title: ArchiverModel.archiverState === ArchiverStates.PS_COMPRESSING ? "Compressing..." : "Decompressing"
        visible: ArchiverModel.archiverState === ArchiverStates.PS_COMPRESSING || ArchiverModel.archiverState === ArchiverStates.PS_DECOMPRESSING
    }

    FileDialog {
        id: fileDialog

        title: "Please choose a file"
        folder: shortcuts.home
        selectExisting: false
        selectMultiple: false
        selectFolder: !FilesystemDirModel.browsingFilesystem || decompressButton.decompressWholeFile
        nameFilters: [ "Simple Archive files (*.sar)" ]

        onVisibleChanged: {
            //FileDialog issue - selectExisting sets to true sometimes instead of pre-binded false
            selectExisting = false;
        }

        onAccepted: {
            console.log("File choosen: " + fileDialog.fileUrls)
            if (FilesystemDirModel.browsingFilesystem && !decompressButton.decompressWholeFile) {
                ArchiverModel.compressSelected(filesystemView.currentRow, fileUrl, compressionLevel.currentIndex);
            } else {
                ArchiverModel.decompressSelected(filesystemView.currentRow, fileUrl, decompressButton.decompressWholeFile);
            }
            decompressButton.decompressWholeFile = false;
        }
        onRejected: {
            decompressButton.decompressWholeFile = false;
            console.log("Canceled")
        }
    }
}
