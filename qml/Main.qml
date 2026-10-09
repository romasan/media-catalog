import QtQuick
import QtQuick.Controls
import MediaCatalog

// Главное окно приложения.
ApplicationWindow {
    id: window

    // Ссылки на синглтоны проверяются через Library: если бы context property
    // не пришла, обращение ниже уже дало бы ошибку загрузки.
    readonly property var mediaLibrary: Library

    title: qsTr("Media Catalog")
    visible: true
    width: 1280
    height: 840
    minimumWidth: 720
    minimumHeight: 480
    color: Theme.background

    // ==== Шапка ====
    header: Rectangle {
        height: 48
        color: Theme.background

        BurgerMenu {
            id: burger
            anchors.left: parent.left
            anchors.leftMargin: Theme.spacing * 1.5
            anchors.verticalCenter: parent.verticalCenter
            onOpenCatalogManager: catalogManager.open()
            onOpenTagManager: tagManager.openForSelection()
        }

        Text {
            anchors.left: burger.right
            anchors.leftMargin: Theme.spacing * 2
            anchors.verticalCenter: parent.verticalCenter
            text: "Media Catalog"
            color: Theme.text
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeLarge
            font.bold: true
        }

        Text {
            anchors.right: parent.right
            anchors.rightMargin: Theme.spacing * 2
            anchors.verticalCenter: parent.verticalCenter
            text: {
                if (mediaLibrary.scanning)
                    return "Сканирование…";
                if (mediaLibrary.generatingThumbnails)
                    return "Превью: " + Math.round(mediaLibrary.thumbnailProgress * 100) + "%";
                return "Фото: " + mediaLibrary.photoCount + "   Видео: " + mediaLibrary.videoCount;
            }
            color: Theme.textMuted
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeSmall
        }
    }

    // ==== Основная область ====
    Column {
        anchors.fill: parent
        spacing: 0

        FilterBar {
            width: parent.width
            height: implicitHeight
        }

        Item {
            width: parent.width
            height: parent.height - parent.spacing

            MediaGrid {
                id: grid
                anchors.fill: parent
                anchors.bottomMargin: App.hasSelection ? 46 : 0
            }

            BatchTagBar {
                id: batchBar
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                height: App.hasSelection ? 46 : 0
            }
        }
    }

    // ==== Прогресс генерации превью ====
    ThumbnailProgressBar {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.topMargin: 48
        active: mediaLibrary.generatingThumbnails
        progress: mediaLibrary.thumbnailProgress
    }

    // ==== Попапы ====
    CatalogManagerPopup {
        id: catalogManager
        anchors.fill: parent
    }

    TagManagerPopup {
        id: tagManager
        anchors.fill: parent
    }

    ToastContainer {
        id: toasts
        anchors.fill: parent
    }

    // ==== Реакция на события ядра ====
    Connections {
        target: mediaLibrary

        // Завершение сканирования: показываем, что изменилось.
        function onScanFinished(result) {
            const parts = [];
            if (result.addedFiles > 0)
                parts.push("Добавлено файлов: " + result.addedFiles);
            if (result.removedFiles > 0)
                parts.push("Удалено файлов: " + result.removedFiles);
            if (result.addedFolders > 0)
                parts.push("Добавлено папок: " + result.addedFolders);
            if (parts.length > 0)
                toasts.show(parts.join(", "), "success");
        }

        function onToast(message, type) {
            toasts.show(message, type);
        }

        function onLastErrorChanged() {
            if (mediaLibrary.lastError.length > 0)
                toasts.show(mediaLibrary.lastError, "error");
        }
    }

    // Обновляем подписи превью не чаще пяти раз в секунду: иначе интерфейс
    // перерисовывается на каждый готовый файл.
    Timer {
        id: thumbnailRefreshTimer
        interval: 200
        onTriggered: grid.update()
    }

    Connections {
        target: mediaLibrary
        function onThumbnailProgressChanged() {
            thumbnailRefreshTimer.restart();
        }
    }
}
