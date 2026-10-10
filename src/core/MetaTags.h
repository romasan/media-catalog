#pragma once

#include "core/MediaTypes.h"

#include <QList>
#include <QString>

namespace mc::metaTags {

/// Префикс идентификаторов метатегов. Совпадает с версией 1.x, чтобы
/// фильтры и файлы экспорта оставались совместимыми.
inline constexpr QLatin1String Prefix("meta:");

/// Идентификатор метатега «без тега» — `meta:untagged`.
QString untaggedId();

/// Метатег «без тега».
MetaTag untagged();

/// Группы метатегов в порядке отображения.
const QList<MetaTag> &groups();

/// Название времени года по номеру месяца (0 = январь .. 11 = декабрь).
QString seasonForMonth(int month);

/// Строит метатег по его идентификатору. Возвращает false, если формат
/// идентификатора не распознан.
bool metaTagById(const QString &metaTagId, MetaTag *out);

bool isMetaTagId(const QString &tagId);

/// Метатеги конкретного файла: год, время года и тип файла.
QList<MetaTag> forFile(const MediaFile &file);

/// Проверяет, удовлетворяет ли файл метатегу. Для неизвестного идентификатора
/// возвращает false.
bool matches(const MediaFile &file, const QString &metaTagId);

/// Список всех метатегов, встречающихся в наборе файлов, в порядке:
/// годы по убыванию, времена года в календарном порядке, типы, «без тега».
QList<MetaTag> all(const QList<MediaFile> &files);

} // namespace mc::metaTags
