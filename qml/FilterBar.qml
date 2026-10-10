import QtQuick
import QtQuick.Controls
import MediaCatalog

// Панель фильтра — компактная плашка в левом нижнем углу.
//
// Повторяет вид версии 1.x:
//   - переключатель «все / любой» появляется только при двух и более
//     выбранных тегах;
//   - при пустом фильтре показывается только счётчик;
//   - счётчик отражает активный фильтр (или весь каталог, если фильтр пуст).
Item {
    id: root

    implicitWidth: Math.min(panel.implicitWidth, 320)
    implicitHeight: panel.implicitHeight
    visible: true

    Rectangle {
        id: panel
        anchors.fill: parent
        implicitWidth: content.implicitWidth + 20
        implicitHeight: content.implicitHeight + 20
        color: "#eb1e1e1e"
        radius: 10
        border.color: Theme.border

        Column {
            id: content
            anchors.centerIn: parent
            spacing: Theme.spacing

            // Переключатель режима: «все» (И) или «любой» (ИЛИ).
            Rectangle {
                id: modeToggle
                visible: App.library.filterTagIds().length > 1
                width: parent.width
                height: visible ? 26 : 0
                color: Theme.surfaceAlt
                radius: 6

                Row {
                    anchors.fill: parent
                    anchors.margins: 3
                    spacing: 4

                    Repeater {
                        model: [
                            { "label": "все", "mode": "AND",
                              "hint": "Показывать только файлы со всеми выбранными тегами" },
                            { "label": "любой", "mode": "OR",
                              "hint": "Показывать файлы хотя бы с одним из выбранных тегов" }
                        ]

                        delegate: Rectangle {
                            required property var modelData

                            width: (modeToggle.width - 10) / 2
                            height: parent.height
                            radius: 4
                            color: App.library.filterMode === modelData.mode
                                   ? Theme.accent : "transparent"

                            Text {
                                anchors.centerIn: parent
                                text: modelData.label
                                color: App.library.filterMode === modelData.mode
                                       ? "#ffffff" : Theme.textMuted
                                font.pixelSize: 12
                            }

                            MouseArea {
                                anchors.fill: parent
                                onClicked: App.library.setFilterMode(modelData.mode)
                            }

                            ToolTip.text: modelData.hint
                            ToolTip.visible: hoverArea.containsMouse
                            ToolTip.delay: 600

                            MouseArea {
                                id: hoverArea
                                anchors.fill: parent
                                hoverEnabled: true
                                acceptedButtons: Qt.NoButton
                            }
                        }
                    }
                }
            }

            // Строка с чипами тегов и счётчиком файлов.
            Row {
                spacing: Theme.spacing

                // Чипы выбранных тегов.
                Flow {
                    id: chips
                    width: Math.min(implicitWidth, 220)
                    spacing: 6

                    Repeater {
                        model: App.library.filterTagIds()

                        delegate: Rectangle {
                            required property string modelData

                            readonly property bool isMeta: modelData.indexOf("meta:") === 0
                            readonly property string label: App.library.tagName(modelData)

                            width: chipRow.implicitWidth + 16
                            height: 24
                            radius: 14
                            color: isMeta ? Theme.surfaceAlt : "#2d4a6b"

                            Row {
                                id: chipRow
                                anchors.centerIn: parent
                                spacing: 4

                                Text {
                                    anchors.verticalCenter: parent.verticalCenter
                                    text: parent.parent.label
                                    color: parent.parent.isMeta ? Theme.textMuted : "#d6e6f7"
                                    font.pixelSize: 13
                                }

                                CloseGlyph {
                                    anchors.verticalCenter: parent.verticalCenter
                                    width: 10
                                    height: 10
                                    color: closeArea.containsMouse ? Theme.danger
                                                                   : "#99ffffff"
                                }
                            }

                            MouseArea {
                                id: closeArea
                                anchors.fill: parent
                                hoverEnabled: true
                                onClicked: App.library.removeTagFromFilter(modelData)
                            }

                            ToolTip.text: parent.isMeta ? "Удалить метатег из фильтра"
                                                        : "Удалить тег из фильтра"
                            ToolTip.visible: closeArea.containsMouse
                            ToolTip.delay: 600
                        }
                    }
                }

                // Счётчик файлов: столько же, сколько показано в сетке.
                Row {
                    id: countBlock
                    spacing: 10

                    PhotoGlyph {
                        anchors.verticalCenter: parent.verticalCenter
                        width: 13
                        height: 13
                        color: Theme.textMuted
                    }

                    Text {
                        id: photoCount
                        anchors.verticalCenter: parent.verticalCenter
                        text: App.mediaModel.photoCount
                        color: Theme.textMuted
                        font.pixelSize: 12
                    }

                    VideoGlyph {
                        anchors.verticalCenter: parent.verticalCenter
                        width: 13
                        height: 13
                        color: Theme.textMuted
                    }

                    Text {
                        id: videoCount
                        anchors.verticalCenter: parent.verticalCenter
                        text: App.mediaModel.videoCount
                        color: Theme.textMuted
                        font.pixelSize: 12
                    }
                }
            }
        }
    }
}
