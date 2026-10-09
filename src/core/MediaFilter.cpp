#include "core/MediaFilter.h"

#include "core/MetaTags.h"

#include <algorithm>

namespace mc {

bool mediaMatchesFilter(const MediaFile &media,
                        const FilterCondition &filter,
                        const QSet<QString> &taggedMediaIds,
                        const QHash<QString, QSet<QString>> &mediaIdsByTag)
{
    if (filter.isEmpty()) {
        return true;
    }

    const auto matchesOne = [&](const QString &tagId) {
        if (tagId == metaTags::untaggedId()) {
            return !taggedMediaIds.contains(media.id);
        }
        if (metaTags::isMetaTagId(tagId)) {
            return metaTags::matches(media, tagId);
        }
        return mediaIdsByTag.value(tagId).contains(media.id);
    };

    if (filter.mode == FilterMode::All) {
        return std::all_of(filter.tagIds.cbegin(), filter.tagIds.cend(), matchesOne);
    }
    return std::any_of(filter.tagIds.cbegin(), filter.tagIds.cend(), matchesOne);
}

QList<MediaFile> filterMedia(const QList<MediaFile> &media,
                             const FilterCondition &filter,
                             const QSet<QString> &taggedMediaIds)
{
    if (filter.isEmpty()) {
        return media;
    }

    QList<MediaFile> result;
    result.reserve(media.size());
    for (const MediaFile &file : media) {
        if (mediaMatchesFilter(file, filter, taggedMediaIds, {})) {
            result.append(file);
        }
    }
    return result;
}

} // namespace mc
