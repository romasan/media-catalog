import { observer } from 'mobx-react-lite';
import React from 'react';
import { useApp } from '../store/AppStore';

export const FilterBar = observer(function FilterBar(): React.ReactElement | null {
  const { filter, tags, metaTags, removeTagFromFilter, setFilterMode } = useApp();

  const selectedTags = tags.filter((t) => filter.tagIds.includes(t.tag.id));
  const selectedMetaTags = metaTags.filter((m) => filter.tagIds.includes(m.metaTag.id));

  if (filter.tagIds.length === 0) {
    return null;
  }

  return (
    <div className="filter-bar">
      {filter.tagIds.length > 1 && (
        <div className="filter-mode-toggle">
          <button
            className={`filter-toggle-btn ${filter.mode === 'AND' ? 'active' : ''}`}
            onClick={() => setFilterMode('AND')}
            title="Показывать только файлы со всеми выбранными тегами"
          >
            все
          </button>
          <button
            className={`filter-toggle-btn ${filter.mode === 'OR' ? 'active' : ''}`}
            onClick={() => setFilterMode('OR')}
            title="Показывать файлы хотя бы с одним из выбранных тегов"
          >
            любой
          </button>
        </div>
      )}
      <div className="filter-tags">
        {selectedMetaTags.map(({ metaTag }) => (
          <div className="filter-tag-chip filter-meta-tag-chip" key={metaTag.id}>
            <span className="filter-tag-name">{metaTag.name}</span>
            <button
              className="filter-tag-remove"
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
          <div className="filter-tag-chip" key={tag.id}>
            <span className="filter-tag-name">{tag.name}</span>
            <button
              className="filter-tag-remove"
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
    </div>
  );
});
