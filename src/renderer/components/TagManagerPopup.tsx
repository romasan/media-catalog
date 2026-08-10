import React, { useEffect, useMemo, useState } from 'react';
import { createPortal } from 'react-dom';
import { useApp } from '../store/AppContext';
import { DraggableResizable } from './DraggableResizable';
import { META_TAG_GROUPS } from '../../shared/metaTags';
import type { TagSearchResult } from '../../shared/types';

interface TagManagerPopupProps {
  onClose: () => void;
}

function pluralize(count: number, one: string, few: string, many: string): string {
  const mod10 = count % 10;
  const mod100 = count % 100;
  if (mod10 === 1 && mod100 !== 11) {
    return one;
  }
  if (mod10 >= 2 && mod10 <= 4 && (mod100 < 10 || mod100 >= 20)) {
    return few;
  }
  return many;
}

export function TagManagerPopup({ onClose }: TagManagerPopupProps): React.ReactElement {
  const { tags, metaTags, loadTags, addTagToFilter } = useApp();
  const [query, setQuery] = useState('');
  const [tagPendingDelete, setTagPendingDelete] = useState<TagSearchResult | null>(null);

  const filteredTags = useMemo(() => {
    const q = query.trim().toLowerCase();
    let result = [...tags];
    if (q) {
      result = result.filter(({ tag }) => tag.name.toLowerCase().includes(q));
    }
    return result.slice(0, 30);
  }, [tags, query]);

  const metaTagsByGroup = useMemo(() => {
    return META_TAG_GROUPS.map(({ kind, group }) => ({
      group,
      items: metaTags
        .filter(({ metaTag }) => metaTag.kind === kind)
        .map(({ metaTag }) => metaTag),
    })).filter((g) => g.items.length > 0);
  }, [metaTags]);

  const handleCreateNew = async () => {
    const name = query.trim();
    if (!name) {
      return;
    }
    try {
      await window.api.createTag(name);
      setQuery('');
      await loadTags();
    } catch (error) {
      console.error('Ошибка создания тега:', error);
    }
  };

  const handleDelete = async (tagId: string) => {
    try {
      await window.api.deleteTag(tagId);
      await loadTags();
    } catch (error) {
      console.error('Ошибка удаления тега:', error);
    }
  };

  const handleDeleteClick = (tag: TagSearchResult, event: React.MouseEvent) => {
    event.stopPropagation();
    if (tag.count > 0) {
      setTagPendingDelete(tag);
      return;
    }
    void handleDelete(tag.tag.id);
  };

  const handleConfirmDelete = async () => {
    if (!tagPendingDelete) {
      return;
    }
    const tagId = tagPendingDelete.tag.id;
    setTagPendingDelete(null);
    await handleDelete(tagId);
  };

  const handleCancelDelete = () => {
    setTagPendingDelete(null);
  };

  // Пока открыто подтверждение удаления, Esc закрывает только его, а не весь попап.
  useEffect(() => {
    if (!tagPendingDelete) {
      return;
    }
    const handleKeyDown = (e: KeyboardEvent) => {
      if (e.key === 'Escape') {
        e.stopPropagation();
        e.stopImmediatePropagation();
        setTagPendingDelete(null);
      }
    };
    // Capture-фаза: перехватываем Esc раньше, чем DraggableResizable (bubbling) закроет попап.
    document.addEventListener('keydown', handleKeyDown, true);
    return () => document.removeEventListener('keydown', handleKeyDown, true);
  }, [tagPendingDelete]);

  const handleTagClick = (tagId: string) => {
    addTagToFilter(tagId);
  };

  const handleKeyDown = (e: React.KeyboardEvent<HTMLInputElement>) => {
    if (e.key === 'Enter') {
      handleCreateNew();
    }
  };

  const exists = tags.some(({ tag }) => tag.name.toLowerCase() === query.trim().toLowerCase());

  return (
    <DraggableResizable
      title="Управление тегами"
      onClose={onClose}
      defaultWidth={460}
      defaultHeight={500}
      minWidth={360}
      minHeight={300}
    >
      <div className="popup-content tag-manager">
        {metaTagsByGroup.length > 0 && (
          <div className="meta-tags-section">
            {metaTagsByGroup.map(({ group, items }) => (
              <div className="meta-tags-group" key={group}>
                <div className="meta-tags-group-title">{group}</div>
                <div className="meta-tags-row">
                  {items.map((metaTag) => (
                    <button
                      className="meta-tag-chip"
                      key={metaTag.id}
                      onClick={() => handleTagClick(metaTag.id)}
                      title={`Добавить «${metaTag.name}» в фильтр`}
                    >
                      {metaTag.name}
                    </button>
                  ))}
                </div>
              </div>
            ))}
          </div>
        )}

        <div className="tag-search-row">
          <input
            className="tag-search-input"
            type="text"
            placeholder="Поиск или создание тега..."
            value={query}
            onChange={(e) => setQuery(e.target.value)}
            onKeyDown={handleKeyDown}
            autoFocus
          />
          {query.trim() && !exists && (
            <button className="tag-create-button" onClick={handleCreateNew}>
              Создать
            </button>
          )}
        </div>

        <div className="tag-list">
          {filteredTags.length === 0 && (
            <div className="tag-empty">
              {query.trim() && !exists
                ? 'Ничего не найдено. Нажмите Enter, чтобы создать тег.'
                : 'Тегов пока нет. Создайте первый тег.'}
            </div>
          )}
          {filteredTags.map(({ tag, count }) => (
            <div
              className="tag-item"
              key={tag.id}
              onClick={() => handleTagClick(tag.id)}
              title={`Добавить «${tag.name}» в фильтр`}
            >
              <span className="tag-name">{tag.name}</span>
              <span className="tag-count">{count} файлов</span>
              <button
                className="icon-button delete-button"
                onClick={(e) => handleDeleteClick({ tag, count }, e)}
                title="Удалить тег"
              >
                <svg viewBox="0 0 24 24" width="16" height="16" fill="none" stroke="currentColor" strokeWidth="2">
                  <path d="M3 6h18M8 6V4a2 2 0 0 1 2-2h4a2 2 0 0 1 2 2v2m3 0v14a2 2 0 0 1-2 2H7a2 2 0 0 1-2-2V6h14z" />
                  <line x1="10" y1="11" x2="10" y2="17" />
                  <line x1="14" y1="11" x2="14" y2="17" />
                </svg>
              </button>
            </div>
          ))}
        </div>

        <div className="tag-total">Всего тегов: {tags.length}</div>

        {tagPendingDelete &&
          createPortal(
            <div className="tag-delete-modal" onClick={handleCancelDelete}>
              <div
                className="tag-delete-modal-window"
                onClick={(e) => e.stopPropagation()}
              >
                <div className="tag-delete-modal-title">Удалить тег?</div>
                <div className="tag-delete-modal-text">
                  Тег «{tagPendingDelete.tag.name}» присвоен {tagPendingDelete.count}{' '}
                  {pluralize(tagPendingDelete.count, 'файлу', 'файлам', 'файлам')}. Связи с
                  файлами будут удалены.
                </div>
                <div className="tag-delete-modal-actions">
                  <button className="tag-delete-modal-cancel" onClick={handleCancelDelete}>
                    Отмена
                  </button>
                  <button className="tag-delete-modal-submit" onClick={handleConfirmDelete}>
                    Удалить
                  </button>
                </div>
              </div>
            </div>,
            document.body,
          )}
      </div>
    </DraggableResizable>
  );
}
