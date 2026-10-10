import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import MediaCatalog

// Управление каталогами: добавление папок, статистика, удаление.
Item {
    id: root

    visible: false
    focus: true

    signal closed()

    function open() {
        visible = true;
        window.centerInParent();
        window.forceActiveFocus();
    }

    function close() {
        visible = false;
        closed();
    }

    Keys.onEscapePressed: root.close()

    // Подложка: перехватывает клики по окну под попапом и закрывает его.
    Rectangle {
        anchors.fill: parent
        color: "#4d000000"

        MouseArea {
            anchors.fill: parent
            onClicked: root.close()
        }
    }

    /// Статистика по каталогу: сколько фото и видео найдено.
    function statsFor(catalogId) {
        const stats = Library.catalogStats();
        for (let i = 0; i < stats.length; ++i) {
            if (stats[i].catalogId === catalogId)
                return stats[i];
        }
        return { "photoCount": 0, "videoCount": 0 };
    }

    DraggableResizable {
        id: window
        width: 520
        height: 420
        title: "Каталоги"
        minContentWidth: 360
        minContentHeight: 260
        onClosed: root.close()

        Column {
            anchors.fill: parent
            spacing: Theme.spacing

            Row {
                width: parent.width
                spacing: Theme.spacing

                Button {
                    text: "Добавить папку…"
                    onClicked: folderDialog.open()
                }

                Button {
                    text: "Сканировать заново"
                    flat: true
                    onClicked: Library.requestInitialScan()
                }
            }

            Text {
                width: parent.width
                text: "Файлы сканируются рекурсивно, включая дочерние папки."
                color: Theme.textDim
                wrapMode: Text.WordWrap
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeSmall
            }

            ListView {
                id: catalogList
                width: parent.width
                height: parent.height - y - Theme.spacing
                clip: true
                model: Library.catalogs()

                ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                delegate: Rectangle {
                    required property var modelData

                    width: catalogList.width
                    height: 52
                    color: removeMouse.containsMouse ? Theme.hover : "transparent"
                    border.color: Theme.border
                    border.width: 0

                    readonly property var stats: root.statsFor(modelData.id)

                    Column {
                        anchors.left: parent.left
                        anchors.leftMargin: Theme.spacing
                        anchors.verticalCenter: parent.verticalCenter
                        width: parent.width - 100
                        spacing: 2

                        Text {
                            width: parent.width
                            text: modelData.path
                            elide: Text.ElideMiddle
                            color: Theme.text
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSizeNormal
                        }

                        Text {
                            text: "Фото: " + parent.parent.stats.photoCount
                                  + "   Видео: " + parent.parent.stats.videoCount
                            color: Theme.textMuted
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSizeSmall
                        }
                    }

                    Button {
                        anchors.right: parent.right
                        anchors.rightMargin: Theme.spacing
                        anchors.verticalCenter: parent.verticalCenter
                        text: "Удалить"
                        flat: true
                        onClicked: {
                            pendingRemoval = modelData.id;
                            confirmRemove.open();
                        }

                        MouseArea {
                            id: removeMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            acceptedButtons: Qt.NoButton
                        }
                    }
                }
            }

            Text {
                visible: Library.catalogs().length === 0
                width: parent.width
                text: "Каталоги не добавлены"
                color: Theme.textMuted
                horizontalAlignment: Text.AlignHCenter
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeNormal
            }
        }

        FolderDialog {
            id: folderDialog
            title: "Выберите папку для сканирования"
            currentFolder: Qt.url(App.defaultCatalogDirectory)
            // selectedFolder — это URL (file:///…), поэтому приводим его
            // к локальному пути: иначе каталог не найдётся и сканирование
            // ничего не сделает.
            onAccepted: App.addCatalog(urlToLocalFile(selectedFolder))
        }

        Dialog {
            id: confirmRemove
            anchors.centerIn: parent
            title: "Удаление каталога"
            modal: true
            standardButtons: Dialog.Ok | Dialog.Cancel

            Text {
                text: "Удалить каталог из списка?\nФайлы на диске останутся нетронутыми, "
                      + "но теги, привязанные к ним, будут сняты."
                color: Theme.text
                wrapMode: Text.WordWrap
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeNormal
            }

            onAccepted: {
                App.removeCatalog(pendingRemoval);
                pendingRemoval = "";
            }
        }
    }

    property string pendingRemoval: ""
}
