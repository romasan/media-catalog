import QtQuick

// Крестик закрытия.
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

        ctx.beginPath();
        ctx.moveTo(6, 6);
        ctx.lineTo(18, 18);
        ctx.moveTo(18, 6);
        ctx.lineTo(6, 18);
        ctx.stroke();
    }
}
