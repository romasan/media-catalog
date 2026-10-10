import QtQuick

// Значок «видео»: стилизованная кинокамера.
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
        ctx.fillStyle = color;
        ctx.lineJoin = "round";

        // Корпус.
        ctx.strokeRect(3, 7, 12, 10);

        // Объектив-треугольник.
        ctx.beginPath();
        ctx.moveTo(16, 11);
        ctx.lineTo(21, 8);
        ctx.lineTo(21, 16);
        ctx.lineTo(16, 13);
        ctx.closePath();
        ctx.fill();
    }
}
