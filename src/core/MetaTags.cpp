#include "core/MetaTags.h"

#include <QDateTime>
#include <QSet>
#include <QStringList>

#include <algorithm>

namespace mc::metaTags {

namespace {

QString groupForKind(MetaTagKind kind)
{
    switch (kind) {
    case MetaTagKind::Year:
        return QStringLiteral("Годы");
    case MetaTagKind::Season:
        return QStringLiteral("Времена года");
    case MetaTagKind::Type:
        return QStringLiteral("Тип файла");
    case MetaTagKind::Untagged:
        break;
    }
    return QStringLiteral("Прочее");
}

QString typeNameForKind(MediaKind kind)
{
    return kind == MediaKind::Video ? QStringLiteral("видео") : QStringLiteral("фото");
}

/// Год по локальному времени — как `new Date(ms).getFullYear()` в JS.
int localYear(qint64 timestampMs)
{
    return QDateTime::fromMSecsSinceEpoch(timestampMs).date().year();
}

/// Месяц по локальному времени (0..11) — как `new Date(ms).getMonth()` в JS.
int localMonth(qint64 timestampMs)
{
    return QDateTime::fromMSecsSinceEpoch(timestampMs).date().month() - 1;
}

} // namespace

QString untaggedId()
{
    return QStringLiteral("meta:untagged");
}

MetaTag untagged()
{
    MetaTag tag;
    tag.id = untaggedId();
    tag.name = QStringLiteral("без тега");
    tag.kind = MetaTagKind::Untagged;
    tag.group = groupForKind(MetaTagKind::Untagged);
    return tag;
}

const QList<MetaTag> &groups()
{
    static const QList<MetaTag> value = {
        MetaTag{QStringLiteral("meta:year"), QString(), MetaTagKind::Year, groupForKind(MetaTagKind::Year)},
        MetaTag{QStringLiteral("meta:season"), QString(), MetaTagKind::Season, groupForKind(MetaTagKind::Season)},
        MetaTag{QStringLiteral("meta:type"), QString(), MetaTagKind::Type, groupForKind(MetaTagKind::Type)},
        untagged(),
    };
    return value;
}

QString seasonForMonth(int month)
{
    switch (month) {
    case 11: // декабрь
    case 0:  // январь
    case 1:  // февраль
        return QStringLiteral("зима");
    case 2: // март
    case 3: // апрель
    case 4: // май
        return QStringLiteral("весна");
    case 5: // июнь
    case 6: // июль
    case 7: // август
        return QStringLiteral("лето");
    case 8:  // сентябрь
    case 9:  // октябрь
    case 10: // ноябрь
        return QStringLiteral("осень");
    default:
        return QString();
    }
}

bool metaTagById(const QString &metaTagId, MetaTag *out)
{
    if (metaTagId == untaggedId()) {
        if (out) {
            *out = untagged();
        }
        return true;
    }
    if (!metaTagId.startsWith(Prefix)) {
        return false;
    }

    // Формат: `meta:<kind>:<name>` — ровно три части.
    const QStringList parts = metaTagId.split(QLatin1Char(':'));
    if (parts.size() != 3) {
        return false;
    }

    const QString kindName = parts.at(1);
    const QString name = parts.at(2);
    if (name.isEmpty()) {
        return false;
    }

    MetaTagKind kind = MetaTagKind::Year;
    if (kindName == QLatin1String("year")) {
        kind = MetaTagKind::Year;
    } else if (kindName == QLatin1String("season")) {
        kind = MetaTagKind::Season;
    } else if (kindName == QLatin1String("type")) {
        kind = MetaTagKind::Type;
    } else {
        return false;
    }

    if (out) {
        out->id = metaTagId;
        out->name = name;
        out->kind = kind;
        out->group = groupForKind(kind);
    }
    return true;
}

bool isMetaTagId(const QString &tagId)
{
    return tagId.startsWith(Prefix);
}

QList<MetaTag> forFile(const MediaFile &file)
{
    const int year = localYear(file.modifiedAt);
    const QString season = seasonForMonth(localMonth(file.modifiedAt));
    const QString typeName = typeNameForKind(file.kind);

    return {
        MetaTag{QStringLiteral("meta:year:%1").arg(year), QString::number(year), MetaTagKind::Year, groupForKind(MetaTagKind::Year)},
        MetaTag{QStringLiteral("meta:season:%1").arg(season), season, MetaTagKind::Season, groupForKind(MetaTagKind::Season)},
        MetaTag{QStringLiteral("meta:type:%1").arg(typeName), typeName, MetaTagKind::Type, groupForKind(MetaTagKind::Type)},
    };
}

bool matches(const MediaFile &file, const QString &metaTagId)
{
    const QList<MetaTag> tags = forFile(file);
    for (const MetaTag &tag : tags) {
        if (tag.id == metaTagId) {
            return true;
        }
    }
    return false;
}

QList<MetaTag> all(const QList<MediaFile> &files)
{
    QSet<QString> yearNames;
    QSet<QString> seasonNames;
    QSet<QString> typeNames;

    for (const MediaFile &file : files) {
        for (const MetaTag &tag : forFile(file)) {
            switch (tag.kind) {
            case MetaTagKind::Year:
                yearNames.insert(tag.name);
                break;
            case MetaTagKind::Season:
                seasonNames.insert(tag.name);
                break;
            case MetaTagKind::Type:
                typeNames.insert(tag.name);
                break;
            case MetaTagKind::Untagged:
                break;
            }
        }
    }

    QList<MetaTag> result;

    // Годы — по убыванию.
    QStringList years = yearNames.values();
    std::sort(years.begin(), years.end(), [](const QString &a, const QString &b) {
        return a.toInt() > b.toInt();
    });
    for (const QString &year : years) {
        result.append(MetaTag{QStringLiteral("meta:year:%1").arg(year), year, MetaTagKind::Year, groupForKind(MetaTagKind::Year)});
    }

    // Времена года — в календарном порядке.
    const QStringList seasonOrder = {QStringLiteral("зима"), QStringLiteral("весна"), QStringLiteral("лето"), QStringLiteral("осень")};
    for (const QString &season : seasonOrder) {
        if (seasonNames.contains(season)) {
            result.append(MetaTag{QStringLiteral("meta:season:%1").arg(season), season, MetaTagKind::Season, groupForKind(MetaTagKind::Season)});
        }
    }

    // Типы — фото, видео.
    const QStringList typeOrder = {QStringLiteral("фото"), QStringLiteral("видео")};
    for (const QString &type : typeOrder) {
        if (typeNames.contains(type)) {
            result.append(MetaTag{QStringLiteral("meta:type:%1").arg(type), type, MetaTagKind::Type, groupForKind(MetaTagKind::Type)});
        }
    }

    // «Без тега» доступен всегда.
    result.append(untagged());

    return result;
}

} // namespace mc::metaTags
