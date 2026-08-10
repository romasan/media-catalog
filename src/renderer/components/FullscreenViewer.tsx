import React, { useCallback, useEffect, useMemo, useRef, useState } from 'react';
import type { MediaFile, MetaTag, Tag, TagSearchResult } from '../../shared/types';
import { useApp } from '../store/AppContext';

interface FullscreenViewerProps {
  media: MediaFile;
  onClose: () => void;
  onNavigate: (direction: number) => void;
  onAddTagToFilter: (tagId: string) => void;
}

interface ActiveTagInput {
  id: number;
  value: string;
  selectedIndex: number;
}

function formatFileSize(size: number): string {
  if (size < 1024) {
    return `${size} Б`;
  }
  if (size < 1024 * 1024) {
    return `${(size / 1024).toFixed(1)} КБ`;
  }
  if (size < 1024 * 1024 * 1024) {
    return `${(size / (1024 * 1024)).toFixed(1)} МБ`;
  }
  return `${(size / (1024 * 1024 * 1024)).toFixed(2)} ГБ`;
}

function formatDate(timestamp: number): string {
  return new Date(timestamp).toLocaleString('ru-RU', {
    year: 'numeric',
    month: '2-digit',
    day: '2-digit',
    hour: '2-digit',
    minute: '2-digit',
  });
}

export function FullscreenViewer({
  media,
  onClose,
  onNavigate,
  onAddTagToFilter,
}: FullscreenViewerProps): React.ReactElement {
  const { loadTags } = useApp();
  const [mediaUrl, setMediaUrl] = useState<string>('');
  const [thumbUrl, setThumbUrl] = useState<string>('');
  const [tags, setTags] = useState<Tag[]>([]);
  const [metaTags, setMetaTags] = useState<MetaTag[]>([]);
  const [allTags, setAllTags] = useState<TagSearchResult[]>([]);
  const [activeInput, setActiveInput] = useState<ActiveTagInput | null>(null);
  const [displayedMedia, setDisplayedMedia] = useState<MediaFile>(media);
  const inputRef = useRef<HTMLInputElement>(null);
  const nextInputId = useRef(1);

  const loadMedia = useCallback(async (mediaItem: MediaFile) => {
    setDisplayedMedia(mediaItem);
    setMediaUrl('');
    setThumbUrl('');
    setActiveInput(null);

    try {
      const mediaTags = await window.api.getMediaTags(mediaItem.id);
      setTags(mediaTags);
    } catch (error) {
      console.error('Ошибка загрузки тегов:', error);
      setTags([]);
    }

    try {
      const meta = await window.api.getMediaMetaTags(mediaItem.id);
      setMetaTags(meta);
    } catch (error) {
      console.error('Ошибка загрузки метатегов:', error);
      setMetaTags([]);
    }

    try {
      const url = await window.api.getMediaStreamUrl({
        filePath: mediaItem.path,
        type: mediaItem.type,
      });
      setMediaUrl(url);
    } catch (error) {
      console.error('Ошибка загрузки медиа:', error);
      setMediaUrl('');
    }

    if (mediaItem.type === 'video' && mediaItem.thumbnailPath) {
      try {
        const url = await window.api.getMediaStreamUrl({
          filePath: mediaItem.thumbnailPath,
          type: 'photo',
        });
        setThumbUrl(url);
      } catch (error) {
        console.error('Ошибка загрузки превью видео:', error);
        setThumbUrl('');
      }
    }
  }, []);

  const loadAllTags = useCallback(async () => {
    try {
      const result = await window.api.searchTags('');
      setAllTags(result);
    } catch (error) {
      console.error('Ошибка загрузки всех тегов:', error);
      setAllTags([]);
    }
  }, []);

  useEffect(() => {
    loadMedia(media);
    loadAllTags();
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [media]);

  useEffect(() => {
    if (activeInput) {
      setTimeout(() => {
        inputRef.current?.focus();
      }, 0);
    }
  }, [activeInput]);

  const getSuggestions = useCallback(
    (value: string) => {
      const q = value.trim().toLowerCase();
      if (!q) {
        return [];
      }
      const addedTagIds = new Set(tags.map((t) => t.id));
      return allTags
        .filter(({ tag }) => tag.name.toLowerCase().includes(q) && !addedTagIds.has(tag.id))
        .slice(0, 10);
    },
    [allTags, tags],
  );

  const suggestions = useMemo(() => {
    if (!activeInput) {
      return [];
    }
    return getSuggestions(activeInput.value);
  }, [activeInput, getSuggestions]);

  // Глобальная обработка клавиш: Escape, стрелки навигации
  useEffect(() => {
    const handleKeyDown = (e: KeyboardEvent) => {
      if (e.key === 'Escape') {
        if (activeInput) {
          setActiveInput(null);
        } else {
          onClose();
        }
        return;
      }

      // Когда ввод тега активен — стрелки не переключают медиа
      if (activeInput) {
        return;
      }

      if (e.key === 'ArrowLeft') {
        e.preventDefault();
        onNavigate(-1);
      } else if (e.key === 'ArrowRight') {
        e.preventDefault();
        onNavigate(1);
      }
    };

    document.addEventListener('keydown', handleKeyDown);
    return () => document.removeEventListener('keydown', handleKeyDown);
  }, [activeInput, onClose, onNavigate]);

  const handleApplyTag = useCallback(
    async (tagId: string) => {
      if (!displayedMedia) {
        return;
      }
      try {
        await window.api.applyTag(displayedMedia.id, tagId);
        const mediaTags = await window.api.getMediaTags(displayedMedia.id);
        setTags(mediaTags);
        setActiveInput(null);
        await loadTags();
      } catch (error) {
        console.error('Ошибка применения тега:', error);
      }
    },
    [displayedMedia, loadTags],
  );

  const handleCreateAndApplyTag = useCallback(async () => {
    if (!displayedMedia || !activeInput) {
      return;
    }
    const name = activeInput.value.trim();
    if (!name) {
      return;
    }
    try {
      await window.api.createTag(name);
      const results = await window.api.searchTags(name);
      const match = results.find(({ tag }) => tag.name.toLowerCase() === name.toLowerCase());
      if (match) {
        await window.api.applyTag(displayedMedia.id, match.tag.id);
      }
      const mediaTags = await window.api.getMediaTags(displayedMedia.id);
      setTags(mediaTags);
      setActiveInput(null);
      await loadAllTags();
      await loadTags();
    } catch (error) {
      console.error('Ошибка создания тега:', error);
    }
  }, [activeInput, displayedMedia, loadAllTags, loadTags]);

  const handleRemoveTag = useCallback(
    async (tagId: string) => {
      if (!displayedMedia) {
        return;
      }
      try {
        await window.api.removeTagFromMedia(displayedMedia.id, tagId);
        const mediaTags = await window.api.getMediaTags(displayedMedia.id);
        setTags(mediaTags);
        await loadTags();
      } catch (error) {
        console.error('Ошибка удаления тега:', error);
      }
    },
    [displayedMedia, loadTags],
  );

  const handleMediaContextMenu = useCallback(
    (e: React.MouseEvent<HTMLImageElement | HTMLVideoElement>) => {
      // ПКМ только по самому медиафайлу — вызываем нативное контекстное меню «Открыть в проводнике»
      e.preventDefault();
      e.stopPropagation();
      window.api.showItemInFolder({
        filePath: displayedMedia.path,
        x: e.clientX,
        y: e.clientY,
      });
    },
    [displayedMedia.path],
  );

  const handleTagInputKeyDown = (e: React.KeyboardEvent<HTMLInputElement>) => {
    if (!activeInput) {
      return;
    }
    if (e.key === 'ArrowDown') {
      e.preventDefault();
      if (suggestions.length > 0) {
        setActiveInput({
          ...activeInput,
          selectedIndex: Math.min(activeInput.selectedIndex + 1, suggestions.length - 1),
        });
      }
      return;
    }
    if (e.key === 'ArrowUp') {
      e.preventDefault();
      setActiveInput({
        ...activeInput,
        selectedIndex: Math.max(activeInput.selectedIndex - 1, 0),
      });
      return;
    }
    if (e.key === 'Enter') {
      e.preventDefault();
      if (suggestions.length > 0 && activeInput.selectedIndex >= 0) {
        const selected = suggestions[Math.min(activeInput.selectedIndex, suggestions.length - 1)];
        handleApplyTag(selected.tag.id);
      } else {
        handleCreateAndApplyTag();
      }
    }
  };

  const handleTagInputChange = (value: string) => {
    if (!activeInput) {
      return;
    }
    setActiveInput({
      ...activeInput,
      value,
      selectedIndex: 0,
    });
  };

  return (
    <div className="fullscreen-viewer">
      <div className="fullscreen-media-container">
        {displayedMedia.type === 'video' ? (
          mediaUrl ? (
            <video
              src={mediaUrl}
              poster={thumbUrl || undefined}
              controls
              autoPlay
              className="fullscreen-media"
              onContextMenu={handleMediaContextMenu}
            />
          ) : (
            <div className="fullscreen-placeholder">Не удалось загрузить видео</div>
          )
        ) : mediaUrl ? (
          <img
            src={mediaUrl}
            alt={displayedMedia.name}
            className="fullscreen-media"
            onContextMenu={handleMediaContextMenu}
          />
        ) : (
          <div className="fullscreen-placeholder">Не удалось загрузить изображение</div>
        )}

        <button
          className="fullscreen-nav fullscreen-nav-left"
          onClick={() => onNavigate(-1)}
          title="Предыдущее (←)"
          aria-label="Предыдущее"
        >
          ◀
        </button>
        <button
          className="fullscreen-nav fullscreen-nav-right"
          onClick={() => onNavigate(1)}
          title="Следующее (→)"
          aria-label="Следующее"
        >
          ▶
        </button>
      </div>

      <div className="fullscreen-info-bar">
        <div className="fullscreen-file-path" title={displayedMedia.path}>
          {displayedMedia.path}
        </div>
        <div className="fullscreen-file-meta">
          <span>📅 {formatDate(displayedMedia.createdAt)}</span>
          <span>💾 {formatFileSize(displayedMedia.size)}</span>
        </div>
        <div className="fullscreen-tags">
          {metaTags.map((metaTag) => (
            <div className="fullscreen-tag fullscreen-meta-tag" key={metaTag.id}>
              <button
                className="fullscreen-tag-name"
                onClick={() => onAddTagToFilter(metaTag.id)}
                title={`Добавить «${metaTag.name}» в фильтр`}
              >
                {metaTag.name}
              </button>
            </div>
          ))}
          {tags.map((tag) => (
            <div className="fullscreen-tag" key={tag.id}>
              <button
                className="fullscreen-tag-name"
                onClick={() => onAddTagToFilter(tag.id)}
                title={`Добавить «${tag.name}» в фильтр`}
              >
                {tag.name}
              </button>
              <button
                className="fullscreen-tag-remove"
                onClick={() => handleRemoveTag(tag.id)}
                title="Удалить тег"
              >
                ✕
              </button>
            </div>
          ))}
          {activeInput ? (
            <div className="add-tag-input-container">
              <input
                ref={inputRef}
                className="add-tag-input"
                type="text"
                placeholder="Новый тег..."
                value={activeInput.value}
                onChange={(e) => handleTagInputChange(e.target.value)}
                onKeyDown={handleTagInputKeyDown}
                onBlur={() => {
                  setTimeout(() => setActiveInput(null), 150);
                }}
                autoFocus
              />
              {suggestions.length > 0 && (
                <div className="tag-suggestions">
                  {suggestions.map(({ tag }, index) => (
                    <div
                      className={`tag-suggestion ${index === activeInput.selectedIndex ? 'selected' : ''}`}
                      key={tag.id}
                      onMouseDown={(e) => e.preventDefault()}
                      onClick={() => handleApplyTag(tag.id)}
                    >
                      {tag.name}
                    </div>
                  ))}
                </div>
              )}
            </div>
          ) : (
            <button
              className="add-tag-button"
              onClick={() =>
                setActiveInput({
                  id: nextInputId.current++,
                  value: '',
                  selectedIndex: 0,
                })
              }
              title="Добавить тег"
            >
              +
            </button>
          )}
        </div>
      </div>

      <button className="fullscreen-close" onClick={onClose} title="Закрыть (Esc)">
        ✕
      </button>
    </div>
  );
}