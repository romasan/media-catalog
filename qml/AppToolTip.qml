import QtQuick
import QtQuick.Controls
import MediaCatalog

// Подсказка с непрозрачным фоном.
//
// У стандартного ToolTip фон полупрозрачный и просвечивает содержимое под ним,
// поэтому здесь задан собственный фон и рамка.
ToolTip {
    id: control

    delay: 500
    padding: 0

    contentItem: Text {
        text: control.text
        color: Theme.text
        font.pixelSize: Theme.fontSizeSmall
        wrapMode: Text.WordWrap
        width: Math.min(implicitWidth, 320)
    }

    background: Rectangle {
        color: Theme.surfaceAlt
        border.color: Theme.border
        radius: 4
    }

    // Стандартные отступы вокруг текста.
    leftPadding: Theme.spacing
    rightPadding: Theme.spacing
    topPadding: 5
    bottomPadding: 5
}
