import { observer } from 'mobx-react-lite';
import React from 'react';
import { useApp } from '../../store/AppStore';
import styles from './FilterBar.module.css';

export const FilterBar = observer(function FilterBar(): React.ReactElement {
  const { filter, tags, metaTags, mediaItems, removeTagFromFilter, setFilterMode } = useApp();

  const selectedTags = tags.filter((t) => filter.tagIds.includes(t.tag.id));
  const selectedMetaTags = metaTags.filter((m) => filter.tagIds.includes(m.metaTag.id));

  // Количество фото и видео в текущей выдаче (mediaItems уже отражает активный фильтр;
  // при пустом фильтре — это общее количество записей в каталоге).
  let photoCount = 0;
  let videoCount = 0;
  for (const media of mediaItems) {
    if (media.type === 'photo') {
      photoCount += 1;
    } else {
      videoCount += 1;
    }
  }

  const countTitle =
    filter.tagIds.length === 0
      ? 'Общее количество файлов в каталоге'
      : 'Количество файлов с учётом активного фильтра';

  const countBlock = (
    <div className={styles['filter-count']} title={countTitle}>
      <span>📷 {photoCount}</span>
      <span>🎬 {videoCount}</span>
    </div>
  );

  return (
    <div className={styles['filter-bar']}>
      {filter.tagIds.length === 0 ? (
        <div className={styles['filter-row']}>{countBlock}</div>
      ) : (
        <>
          {filter.tagIds.length > 1 && (
            <div className={styles['filter-mode-toggle']}>
              <button
                className={filter.mode === 'AND' ? styles.active : ''}
                onClick={() => setFilterMode('AND')}
                title="Показывать только файлы со всеми выбранными тегами"
              >
                все
              </button>
              <button
                className={filter.mode === 'OR' ? styles.active : ''}
                onClick={() => setFilterMode('OR')}
                title="Показывать файлы хотя бы с одним из выбранных тегов"
              >
                любой
              </button>
            </div>
          )}
          <div className={styles['filter-row']}>
            <div className={styles['filter-tags']}>
              {selectedMetaTags.map(({ metaTag }) => (
                <div className={`${styles['filter-tag-chip']} ${styles['filter-meta-tag-chip']}`} key={metaTag.id}>
                  <span className={styles['filter-tag-name']}>{metaTag.name}</span>
                  <button
                    className={styles['filter-tag-remove']}
                    onClick={() => removeTagFromFilter(metaTag.id)}
                    title="Удалить метатег из фильтра"
                  >
                    <svg viewBox="0 0 24 24" width="12" height="12" fill="none" stroke="currentColor" strokeWidth="2">
                      <line x1="4" y1="4" x2="20" y2="20" />
                      <line x1="20" y1="4" x2="4" y2="20" />
                    </svg>
                  </button>
                </div>
              ))}
              {selectedTags.map(({ tag }) => (
                <div className={styles['filter-tag-chip']} key={tag.id}>
                  <span className={styles['filter-tag-name']}>{tag.name}</span>
                  <button
                    className={styles['filter-tag-remove']}
                    onClick={() => removeTagFromFilter(tag.id)}
                    title="Удалить тег из фильтра"
                  >
                    <svg viewBox="0 0 24 24" width="12" height="12" fill="none" stroke="currentColor" strokeWidth="2">
                      <line x1="4" y1="4" x2="20" y2="20" />
                      <line x1="20" y1="4" x2="4" y2="20" />
                    </svg>
                  </button>
                </div>
              ))}
            </div>
            {countBlock}
          </div>
        </>
      )}
    </div>
  );
});
