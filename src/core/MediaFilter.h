#pragma once

#include "core/MediaTypes.h"

#include <QSet>

namespace mc {

/// Фильтрация списка медиафайлов по набору тегов и метатегов.
///
/// Метатеги (год, время года, тип, «без тега») в базе не хранятся —
/// они вычисляются из свойств файла, поэтому фильтрация выполняется
/// в C++, а не в SQL. Так же устроена Electron-версия.
///
/// Режимы:
///   - FilterMode::All (И)  — файл должен удовлетворять всем условиям;
///   - FilterMode::Any (ИЛИ) — достаточно одного условия.
QList<MediaFile> filterMedia(const QList<MediaFile> &media,
                             const FilterCondition &filter,
                             const QSet<QString> &taggedMediaIds);

/// Проверяет один файл против набора тегов.
/// `mediaIdsByTag` заранее подготовлен для обычных тегов.
bool mediaMatchesFilter(const MediaFile &media,
                        const FilterCondition &filter,
                        const QSet<QString> &taggedMediaIds,
                        const QHash<QString, QSet<QString>> &mediaIdsByTag);

} // namespace mc
