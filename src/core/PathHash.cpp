#include "core/PathHash.h"

#include <QChar>

#include <cstdint>

namespace mc {

QString thumbnailHash(const QString &input)
{
    // uint32_t воспроизводит переполнение знакового 32-битного целого в JS:
    // `hash << 5` и вычитание выполняются в 32 битах.
    std::uint32_t hash = 0;
    for (const QChar ch : input) {
        hash = hash * 31u + static_cast<std::uint32_t>(ch.unicode());
    }

    // `hash | 0` в JS трактует значение как знаковое; Math.abs снимает знак.
    // Для INT32_MIN JS возвращает 2147483648 — задаём явно, так как
    // std::abs(INT32_MIN) переполняется (UB).
    const auto signedHash = static_cast<std::int32_t>(hash);
    const std::uint32_t magnitude =
        (signedHash == INT32_MIN)
            ? 2147483648u
            : static_cast<std::uint32_t>(signedHash < 0 ? -signedHash : signedHash);

    return QString::number(magnitude, 36);
}

} // namespace mc
