import QtQuick

// Значок «фото»: рамка кадра с горой и солнцем.
// Нарисован вектором вместо символа-эмодзи, поэтому одинаково выглядит
// на всех платформах и наследует цвет интерфейса.
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
        ctx.lineCap = "round";

        // Рамка кадра.
        ctx.strokeRect(3, 4, 18, 16);

        // Гора.
        ctx.beginPath();
        ctx.moveTo(6, 17);
        ctx.lineTo(11, 11);
        ctx.lineTo(15, 15);
        ctx.lineTo(18, 12);
        ctx.lineTo(21, 17);
        ctx.stroke();

        // Солнце.
        ctx.beginPath();
        ctx.arc(9, 8, 1.6, 0, Math.PI * 2);
        ctx.fill();
    }
}
