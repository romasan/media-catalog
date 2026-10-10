import QtQuick
import QtQuick.Controls
import MediaCatalog

// Круглая кнопка навигации в полноэкранном просмотре.
Item {
    id: root

    /// Направление: "left" или "right".
    property string direction: "left"

    signal clicked()

    implicitWidth: 48
    implicitHeight: 48

    Rectangle {
        anchors.fill: parent
        radius: width / 2
        color: pressArea.containsMouse ? "#cc2b3037" : "#991d2026"
        border.color: Theme.border
        opacity: root.enabled ? 1.0 : 0.35

        // Шеврон, нарисованный вектором.
        Canvas {
            anchors.centerIn: parent
            width: 20
            height: 20
            antialiasing: true

            onPaint: draw()

            function draw() {
                const ctx = getContext("2d");
                ctx.reset();
                ctx.lineWidth = 2;
                ctx.lineCap = "round";
                ctx.lineJoin = "round";
                ctx.strokeStyle = root.enabled ? "#ffffff" : Theme.textDim;

                const left = root.direction === "left";
                ctx.beginPath();
                if (left) {
                    ctx.moveTo(12, 4);
                    ctx.lineTo(6, 10);
                    ctx.lineTo(12, 16);
                } else {
                    ctx.moveTo(8, 4);
                    ctx.lineTo(14, 10);
                    ctx.lineTo(8, 16);
                }
                ctx.stroke();
            }
        }

        MouseArea {
            id: pressArea
            anchors.fill: parent
            hoverEnabled: true
            enabled: root.enabled
            cursorShape: Qt.PointingHandCursor
            onClicked: root.clicked()
        }

        ToolTip.text: root.direction === "left" ? "Предыдущее (←)" : "Следующее (→)"
        ToolTip.visible: pressArea.containsMouse
        ToolTip.delay: 500
    }
}
