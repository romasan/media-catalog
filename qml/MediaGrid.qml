import QtQuick
import QtQuick.Controls
import MediaCatalog

// Виртуализированная сетка превью.
//
// GridView создаёт делегаты только для видимых ячеек, поэтому прокрутка
// остаётся плавной и на десятках тысяч файлов. Размер ячейки подбирается так,
// чтобы карточки занимали всю ширину, оставаясь не меньше минимального размера.
Item {
    id: root

    /// Карточка, выбранная последней (Ctrl/Cmd+клик) — якорь для Shift+клика.
    property string anchorId: ""

    /// Просит сетку перечитать превью. Превью приходят потоком, поэтому
    /// обновление вызывается с задержкой, а не на каждый файл.
    function update() {
        grid.forceLayout();
        grid.recomputeVisibleRange();
    }

    Column {
        anchors.fill: parent
        spacing: 0

        // Заголовок с диапазоном дат видимых карточек.
        Item {
            id: header
            width: parent.width
            height: App.mediaModel.count > 0 ? 30 : 0

            DateRangeLabel {
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                anchors.rightMargin: Theme.spacing
                minTimestamp: grid.visibleMinDate
                maxTimestamp: grid.visibleMaxDate
            }
        }

        Item {
            id: gridContainer
            width: parent.width
            height: parent.height - header.height

            GridView {
                id: grid
                anchors.fill: parent
                clip: true

                readonly property int columnCount: Math.max(1, Math.floor(width / Theme.minCardSize))
                cellWidth: width / columnCount
                cellHeight: cellWidth
                cacheBuffer: cellHeight * 3
                boundsBehavior: Flickable.StopAtBounds

                model: App.mediaModel

                // Диапазон дат карточек, реально попавших в область просмотра.
                property double visibleMinDate: 0
                property double visibleMaxDate: 0

                ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                delegate: Item {
                    id: cell
                    width: grid.cellWidth
                    height: grid.cellHeight

                    required property int index
                    required property string mediaId
                    required property string name
                    required property bool isVideo
                    required property bool selected
                    required property string thumbnailUrl
                    required property double modifiedAt

                    /// Попадает ли карточка в видимую область (без overscan).
                    readonly property bool inViewport: {
                        const top = cell.y - grid.contentY;
                        return top + cell.height > 0 && top < grid.height;
                    }

                    MediaCard {
                        anchors.fill: parent
                        mediaId: cell.mediaId
                        name: cell.name
                        video: cell.isVideo
                        selected: cell.selected
                        thumbnailUrl: cell.thumbnailUrl

                        onClicked: function (mouse) {
                            grid.handleCardClick(cell.index, cell.mediaId, mouse);
                        }
                        onRightClicked: function (mouse) {
                            // На macOS Ctrl+клик приходит как правый клик:
                            // используем его для добавления к выделению.
                            if (mouse.modifiers & Qt.ControlModifier
                                    || mouse.modifiers & Qt.MetaModifier) {
                                App.mediaModel.toggleSelection(cell.mediaId);
                                root.anchorId = cell.mediaId;
                            }
                        }
                    }
                }

                function handleCardClick(index, mediaId, mouse) {
                    // Shift+клик — выделяем диапазон от якоря.
                    if ((mouse.modifiers & Qt.ShiftModifier) && root.anchorId.length > 0) {
                        App.mediaModel.selectRange(root.anchorId, mediaId);
                        return;
                    }
                    // Ctrl/Cmd+клик — добавляем карточку к выделению.
                    if ((mouse.modifiers & Qt.ControlModifier) || (mouse.modifiers & Qt.MetaModifier)) {
                        App.mediaModel.toggleSelection(mediaId);
                        root.anchorId = mediaId;
                        return;
                    }
                    // Обычный клик — открываем полноэкранный просмотр.
                    App.mediaModel.clearSelection();
                    fullscreenLoader.openAt(index);
                }

                /// Пересчитывает диапазон дат по карточкам в области просмотра.
                /// Индексы вычисляются по геометрии, а не через indexAt():
                /// так результат не зависит от кэша делегатов GridView.
                function recomputeVisibleRange() {
                    const columns = columnCount;
                    const cellH = cellHeight;
                    if (count === 0 || columns <= 0 || cellH <= 0) {
                        visibleMinDate = 0;
                        visibleMaxDate = 0;
                        return;
                    }

                    const firstRow = Math.max(0, Math.floor(contentY / cellH));
                    const lastRow = Math.floor((contentY + height) / cellH);
                    const first = Math.max(0, firstRow * columns);
                    const last = Math.min(count - 1, (lastRow + 1) * columns - 1);

                    let minValue = 0;
                    let maxValue = 0;
                    let found = false;

                    for (let i = first; i <= last; ++i) {
                        const item = itemAtIndex(i);
                        if (!item || !item.inViewport)
                            continue;
                        const value = item.modifiedAt;
                        if (value === undefined || value <= 0)
                            continue;
                        if (!found) {
                            minValue = value;
                            maxValue = value;
                            found = true;
                        } else {
                            minValue = Math.min(minValue, value);
                            maxValue = Math.max(maxValue, value);
                        }
                    }

                    visibleMinDate = minValue;
                    visibleMaxDate = maxValue;
                }

                onContentYChanged: rangeTimer.restart()
                onCountChanged: rangeTimer.restart()
                onHeightChanged: rangeTimer.restart()
                onWidthChanged: rangeTimer.restart()
                Component.onCompleted: rangeTimer.restart()

                Timer {
                    id: rangeTimer
                    interval: 80
                    onTriggered: grid.recomputeVisibleRange()
                }
            }

            // Пустое состояние.
            Column {
                anchors.centerIn: parent
                visible: App.mediaModel.count === 0
                spacing: Theme.spacing

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: App.library.scanning ? "Сканирование каталогов…" : "Каталог пуст"
                    color: Theme.text
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeLarge
                }

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    visible: !App.library.scanning
                    text: "Добавьте папки с фото и видео через меню в правом нижнем углу"
                    color: Theme.textMuted
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeNormal
                }
            }
        }
    }

    Loader {
        id: fullscreenLoader

        property int startIndex: 0

        function openAt(index) {
            startIndex = index;
            active = true;
        }

        active: false
        asynchronous: true
        sourceComponent: FullscreenViewer {
            startIndex: fullscreenLoader.startIndex
            onClosed: fullscreenLoader.active = false
        }
    }
}
