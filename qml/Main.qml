import QtQuick
import QtQuick.Controls
import MediaCatalog

// Главное окно приложения.
//
// Компоновка повторяет версию 1.x: сетка превью занимает всё окно, а элементы
// управления лежат поверх неё (панель фильтра в левом нижнем углу, круглая
// кнопка меню в правом нижнем углу, полоса прогресса у верхнего края). Ни
// «шапки», ни «футера» у окна нет, поэтому карточки видны по всей области.
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
        focus: true

        // Esc закрывает просмотр или попап на уровне окна: фокус в этот момент
        // может быть у любого элемента, поэтому обработчик на самом попапе
        // срабатывает не всегда.
        Keys.onEscapePressed: {
            if (viewerLoader.active) {
                viewerLoader.item.close();
            } else if (catalogManager.visible) {
                catalogManager.close();
            } else if (tagManager.visible) {
                tagManager.close();
            }
        }

        // ==== Сетка превью: вся область окна ====
        MediaGrid {
            id: grid
            anchors.fill: parent
            onOpenRequested: function (index) { viewerLoader.openAt(index); }
        }

        // ==== Элементы управления поверх сетки ====

        // Полоса прогресса генерации превью — тонкая линия у верхнего края.
        ThumbnailProgressBar {
            id: progressBar
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            z: 10
            active: mediaLibrary.generatingThumbnails
            progress: mediaLibrary.thumbnailProgress
        }

        // Панель фильтра — плашка в левом нижнем углу.
        FilterBar {
            id: filterBar
            anchors.left: parent.left
            anchors.bottom: parent.bottom
            anchors.margins: 12
            z: 10
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
            z: 10
            height: App.hasSelection ? 46 : 0
            onTagManagerRequested: tagManager.openForSelection()
        }

        // Круглая кнопка меню — правый нижний угол.
        BurgerMenu {
            id: burgerButton
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.margins: 24
            z: 10
            width: 56
            height: 56
            onOpenCatalogManager: catalogManager.open()
            onOpenTagManager: tagManager.openForSelection()
        }

        // Уведомления — поверх панелей, но под попапами и просмотром.
        ToastContainer {
            id: toasts
            anchors.fill: parent
            z: 20
        }

        // Попапы: перекрывают окно, закрываются крестиком, Esc и кликом по фону.
        CatalogManagerPopup {
            id: catalogManager
            anchors.fill: parent
            z: 200
        }

        TagManagerPopup {
            id: tagManager
            anchors.fill: parent
            z: 200
        }

        // Полноэкранный просмотр: живёт в окне, а не в сетке, чтобы перекрывать
        // и панели управления.
        Loader {
            id: viewerLoader
            objectName: "viewerLoader"
            anchors.fill: parent
            z: 300
            active: false
            asynchronous: false

            property int startIndex: 0

            function openAt(index) {
                startIndex = index;
                // Пересоздаём элемент: так просмотр всегда начинается заново,
                // с нужного файла и с чистым плеером.
                active = false;
                active = true;
            }

            sourceComponent: FullscreenViewer {
                startIndex: viewerLoader.startIndex
                onClosed: viewerLoader.active = false
            }

            // Открываем просмотр сразу после создания элемента.
            onLoaded: item.open()
        }
    }

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
