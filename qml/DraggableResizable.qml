import QtQuick
import QtQuick.Controls
import MediaCatalog

// Перемещаемое и изменяемое по размеру окно для попапов.
//
// Заголовок перетаскивается за окно целиком; правый нижний угол меняет
// размер в заданных пределах.
Rectangle {
    id: root

    default property alias content: contentArea.data

    property string title: ""
    property int minContentWidth: 240
    property int minContentHeight: 160
    property bool resizable: true

    /// Закрытие окна попапа.
    signal closed()

    function close() {
        visible = false;
        closed();
    }

    /// Куда вернуть окно, если пользователь переместил его за пределы экрана.
    function clampToParent() {
        if (!parent)
            return;
        x = Math.max(0, Math.min(x, parent.width - width));
        y = Math.max(0, Math.min(y, parent.height - height));
    }

    function centerInParent() {
        if (!parent)
            return;
        x = (parent.width - width) / 2;
        y = (parent.height - height) / 2;
    }

    color: Theme.surface
    radius: Theme.radius
    border.color: Theme.border
    border.width: 1
    clip: true

    // Заголовок с названием и кнопкой закрытия.
    Rectangle {
        id: titleBar
        width: parent.width
        height: 34
        color: Theme.surfaceAlt
        radius: parent.radius

        // Нижние углы заголовка прямые, иначе виден зазор.
        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: parent.radius
            color: parent.color
        }

        Text {
            anchors.left: parent.left
            anchors.leftMargin: Theme.spacing * 1.5
            anchors.verticalCenter: parent.verticalCenter
            text: root.title
            color: Theme.text
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeNormal
            font.bold: true
        }

        // Простое перетаскивание мышью — надёжнее обработчика жестов.
        MouseArea {
            anchors.fill: parent
            acceptedButtons: Qt.LeftButton
            cursorShape: Qt.OpenHandCursor
            property real pressX: 0
            property real pressY: 0

            onPressed: function (mouse) {
                pressX = mouse.x;
                pressY = mouse.y;
            }
            onPositionChanged: function (mouse) {
                if (!pressed)
                    return;
                root.x += mouse.x - pressX;
                root.y += mouse.y - pressY;
                root.clampToParent();
            }
        }
        // Кнопка закрытия: крестик, нарисованный вектором.
        Item {
            id: closeButton
            objectName: "closeButton"
            anchors.right: parent.right
            anchors.rightMargin: Theme.spacing / 2
            anchors.verticalCenter: parent.verticalCenter
            width: 26
            height: 26

            CloseGlyph {
                anchors.centerIn: parent
                width: 12
                height: 12
                color: closeArea.containsMouse ? Theme.danger : Theme.textMuted
            }

            MouseArea {
                id: closeArea
                anchors.fill: parent
                hoverEnabled: true
                onClicked: root.close()
            }

            ToolTip.text: "Закрыть"
            ToolTip.visible: closeArea.containsMouse
            ToolTip.delay: 500
        }

    }

    Item {
        id: contentArea
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: titleBar.bottom
        anchors.bottom: parent.bottom
        anchors.margins: Theme.spacing
    }

    // Уголок для изменения размера.
    Rectangle {
        visible: root.resizable
        width: 16
        height: 16
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        color: "transparent"

        // Уголок изменения размера: две диагональные линии.
        Canvas {
            anchors.fill: parent
            onPaint: draw()

            function draw() {
                const ctx = getContext("2d");
                ctx.reset();
                ctx.strokeStyle = Theme.textDim;
                ctx.lineWidth = 1.5;
                ctx.lineCap = "round";
                for (const offset of [10, 14]) {
                    ctx.beginPath();
                    ctx.moveTo(width - offset, height - 2);
                    ctx.lineTo(width - 2, height - offset);
                    ctx.stroke();
                }
            }
        }

        MouseArea {
            anchors.fill: parent
            cursorShape: Qt.SizeFDiagCursor

            onPositionChanged: function (mouse) {
                if (!pressed || !root.parent)
                    return;
                // Координаты указателя в системе родителя — из них и считаем
                // новый размер окна.
                const point = mapToItem(root.parent, mouse.x, mouse.y);
                root.width = Math.max(root.minContentWidth + 2 * Theme.spacing,
                                      point.x - root.x);
                root.height = Math.max(root.minContentHeight + 34 + 2 * Theme.spacing,
                                       point.y - root.y);
                root.clampToParent();
            }
        }
    }
}
