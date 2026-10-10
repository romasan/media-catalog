import QtQuick

// Значок удаления: корзина.
Canvas {
    id: root

    property color color: Theme.textMuted
    property real lineWidth: 2

    implicitWidth: 24
    implicitHeight: 24
    antialiasing: true

    onPaint: draw()

    function draw() {
        const s = Math.min(width, height) / 24;
        const ctx = getContext("2d");
        ctx.reset();
        ctx.scale(s, s);
        ctx.lineWidth = lineWidth;
        ctx.strokeStyle = color;
        ctx.lineCap = "round";
        ctx.lineJoin = "round";

        // Крышка и ручка.
        ctx.beginPath();
        ctx.moveTo(4, 7);
        ctx.lineTo(20, 7);
        ctx.moveTo(9, 7);
        ctx.lineTo(9, 4);
        ctx.lineTo(15, 4);
        ctx.lineTo(15, 7);
        ctx.stroke();

        // Корпус.
        ctx.beginPath();
        ctx.moveTo(6, 7);
        ctx.lineTo(7, 20);
        ctx.lineTo(17, 20);
        ctx.lineTo(18, 7);
        ctx.stroke();

        // Прутья.
        ctx.beginPath();
        ctx.moveTo(10, 11);
        ctx.lineTo(10, 17);
        ctx.moveTo(14, 11);
        ctx.lineTo(14, 17);
        ctx.stroke();
    }
}
