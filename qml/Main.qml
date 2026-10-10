import QtQuick
import QtQuick.Controls
import MediaCatalog

// Главное окно приложения.
ApplicationWindow {
    id: window

    readonly property var mediaLibrary: Library

    title: qsTr("Media Catalog")
    visible: true
    width: 1280
    height: 840
    minimumWidth: 720
    minimumHeight: 480
    color: Theme.background

    Item {
        id: rootItem
        anchors.fill: parent

        // Полоса прогресса генерации превью — тонкая линия у верхнего края.
        ThumbnailProgressBar {
            id: progressBar
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            active: mediaLibrary.generatingThumbnails
            progress: mediaLibrary.thumbnailProgress
        }

        // Сетка превью.
        MediaGrid {
            id: grid
            anchors.fill: parent
            // Отступ снизу под панель фильтра и кнопку меню.
            anchors.bottomMargin: App.hasSelection ? 118 : 84
        }

        // Панель фильтра — плашка в левом нижнем углу.
        FilterBar {
            id: filterBar
            anchors.left: parent.left
            anchors.bottom: parent.bottom
            anchors.margins: 12
            width: implicitWidth
            height: implicitHeight
        }

        // Панель массового тегирования — над панелью фильтра.
        BatchTagBar {
            id: batchBar
            anchors.left: filterBar.right
            anchors.right: burgerButton.left
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 12
            anchors.leftMargin: 12
            anchors.rightMargin: 12
            height: App.hasSelection ? 46 : 0
            onTagManagerRequested: tagManager.openForSelection()
        }

        // Круглая кнопка меню — правый нижний угол.
        BurgerMenu {
            id: burgerButton
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.margins: 24
            width: 56
            height: 56
            onOpenCatalogManager: catalogManager.open()
            onOpenTagManager: tagManager.openForSelection()
        }

        ToastContainer {
            id: toasts
            anchors.fill: parent
        }

        // Попапы: перекрывают окно, закрываются крестиком, Esc и кликом по фону.
        CatalogManagerPopup {
            id: catalogManager
            objectName: "catalogManager"
            anchors.fill: parent
            z: 200
        }

        TagManagerPopup {
            id: tagManager
            objectName: "tagManager"
            anchors.fill: parent
            z: 200
        }
    }

    // ==== Попапы ====
    // Живут внутри корневого Item: у ApplicationWindow может быть только один
    // визуальный ребёнок, иначе окно не раскладывает содержимое.

    // ==== События ядра ====
    Connections {
        target: mediaLibrary

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
