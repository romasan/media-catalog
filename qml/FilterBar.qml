import QtQuick
import QtQuick.Controls
import MediaCatalog

// Панель фильтра: выбранные теги, режим «все/любой», счётчик файлов.
Item {
    id: root

    implicitHeight: 40

    Rectangle {
        anchors.fill: parent
        color: Theme.surface
        border.color: Theme.border
        border.width: 1
    }

    Row {
        anchors.left: parent.left
        anchors.leftMargin: Theme.spacing * 1.5
        anchors.verticalCenter: parent.verticalCenter
        spacing: Theme.spacing

        // Счётчик найденных файлов.
        Text {
            anchors.verticalCenter: parent.verticalCenter
            text: App.mediaModel.count + " / " + App.library.mediaCount
            color: Theme.textMuted
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeSmall
        }

        // Метка фильтра.
        Text {
            anchors.verticalCenter: parent.verticalCenter
            visible: filterChips.count > 0
            text: "Фильтр:"
            color: Theme.textDim
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeSmall
        }

        Repeater {
            id: filterChips
            model: App.library.filterTagIds()

            delegate: Rectangle {
                required property string modelData

                readonly property bool isMeta: modelData.indexOf("meta:") === 0
                readonly property string label: App.library.tagName(modelData)

                width: chipText.implicitWidth + 2 * Theme.spacing + 14
                height: 24
                radius: 12
                color: isMeta ? Theme.surfaceAlt : Theme.accentSoft
                border.color: isMeta ? Theme.border : Theme.accent

                Text {
                    id: chipText
                    anchors.left: parent.left
                    anchors.leftMargin: Theme.spacing
                    anchors.verticalCenter: parent.verticalCenter
                    text: parent.label
                    color: parent.isMeta ? Theme.textMuted : Theme.text
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeSmall
                }

                Text {
                    anchors.right: parent.right
                    anchors.rightMargin: 6
                    anchors.verticalCenter: parent.verticalCenter
                    text: "✕"
                    color: Theme.textDim
                    font.pixelSize: 10
                }

                MouseArea {
                    anchors.fill: parent
                    onClicked: App.library.removeTagFromFilter(modelData)
                }
            }
        }
    }

    Row {
        anchors.right: parent.right
        anchors.rightMargin: Theme.spacing * 1.5
        anchors.verticalCenter: parent.verticalCenter
        spacing: Theme.spacing

        // Режим фильтрации: все условия или любое.
        Button {
            height: 26
            flat: true
            text: App.library.filterMode === "AND" ? "все" : "любой"
            onClicked: App.library.setFilterMode(App.library.filterMode === "AND" ? "OR" : "AND")
            ToolTip.text: "Режим фильтра: все условия или любое из них"
            ToolTip.visible: hovered
        }

        Button {
            height: 26
            flat: true
            visible: App.library.filterTagIds().length > 0
            text: "Сбросить"
            onClicked: App.library.clearFilter()
        }
    }
}
