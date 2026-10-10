import QtQuick
import QtQuick.Controls
import MediaCatalog

// Подсказка с непрозрачным фоном.
//
// Объявляется как обычный дочерний элемент, а не через присоединённые
// свойства: `AppToolTip.text` у собственного компонента не работает — у него
// нет присоединённых свойств, и вместо этого создаётся стандартный ToolTip с
// полупрозрачным фоном.
//
// Обычно достаточно указать владельца подсказки и текст:
//
// ```qml
// AppToolTip {
//     target: cardMouseArea
//     text: "Показать карточку"
// }
// ```
//
// Подсказка сама следит за наведением и учитывает задержку.
ToolTip {
    id: root

    /// Область наведения, при наведении на которую показывается подсказка.
    property Item target: null

    delay: 500
    visible: target !== null && target.containsMouse !== undefined
             && target.containsMouse && text.length > 0

    padding: 0
    leftPadding: Theme.spacing
    rightPadding: Theme.spacing
    topPadding: 5
    bottomPadding: 5

    background: Rectangle {
        color: Theme.surfaceAlt
        border.color: Theme.border
        radius: 4
    }

    contentItem: Text {
        text: root.text
        color: Theme.text
        font.pixelSize: Theme.fontSizeSmall
        wrapMode: Text.WordWrap
        width: Math.min(implicitWidth, 320)
    }
}
