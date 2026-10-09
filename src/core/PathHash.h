#pragma once

#include <QString>

namespace mc {

/// Хэш пути для имени файла превью — точное повторение алгоритма
/// Electron-версии (`src/main/thumbnails.ts`, `hashString`):
///
/// ```js
/// let hash = 0;
/// for (let i = 0; i < input.length; i++) {
///   hash = ((hash << 5) - hash) + input.charCodeAt(i);
///   hash |= 0;
/// }
/// return Math.abs(hash).toString(36);
/// ```
///
/// Семантика JS, которую нужно воспроизвести:
///   - `charCodeAt` возвращает **кодовую единицу UTF-16**, а не Unicode-символ;
///   - `hash | 0` усекает значение до знакового 32-битного;
///   - `Math.abs` снимает знак, причём для INT32_MIN возвращает 2^31.
///
/// Благодаря точному совпадению имена превью совпадают с Electron-версией,
/// и уже сгенерированный кэш превью можно переиспользовать.
QString thumbnailHash(const QString &input);

} // namespace mc
