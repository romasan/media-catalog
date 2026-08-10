import React, { useCallback, useEffect, useMemo, useRef, useState } from 'react';
import { useApp } from '../store/AppContext';

export function BatchTagBar(): React.ReactElement | null {
  const { selectedMediaIds, tags, applyTagToMedia, clearSelection } = useApp();
  const [query, setQuery] = useState('');
  const [activeIndex, setActiveIndex] = useState(0);
  const inputRef = useRef<HTMLInputElement>(null);

  const visible = selectedMediaIds.length > 0;

  useEffect(() => {
    if (visible) {
      inputRef.current?.focus();
    } else {
      setQuery('');
      setActiveIndex(0);
    }
  }, [visible]);

  const suggestions = useMemo(() => {
    const q = query.trim().toLowerCase();
    if (!q) {
      return tags.slice(0, 8);
    }
    return tags.filter((t) => t.tag.name.toLowerCase().includes(q)).slice(0, 8);
  }, [query, tags]);

  const applyTag = useCallback(
    async (tagId: string) => {
      await applyTagToMedia(selectedMediaIds, tagId);
      setQuery('');
      setActiveIndex(0);
    },
    [applyTagToMedia, selectedMediaIds],
  );

  const handleCreateTag = useCallback(
    async (name: string) => {
      const newTags = await window.api.createTag(name);
      const normalized = name.trim().toLowerCase();
      const newTag = newTags.find((t) => t.name === normalized) ?? newTags[newTags.length - 1];
      if (newTag) {
        await applyTag(newTag.id);
      }
    },
    [applyTag],
  );

  const handleKeyDown = useCallback(
    (e: React.KeyboardEvent<HTMLInputElement>) => {
      if (e.key === 'ArrowDown') {
        e.preventDefault();
        setActiveIndex((prev) => Math.min(prev + 1, suggestions.length - 1));
      } else if (e.key === 'ArrowUp') {
        e.preventDefault();
        setActiveIndex((prev) => Math.max(prev - 1, 0));
      } else if (e.key === 'Enter') {
        e.preventDefault();
        const q = query.trim();
        if (suggestions.length > 0 && activeIndex >= 0 && activeIndex < suggestions.length) {
          applyTag(suggestions[activeIndex].tag.id);
        } else if (q) {
          handleCreateTag(q);
        }
      } else if (e.key === 'Escape') {
        clearSelection();
      }
    },
    [suggestions, activeIndex, query, applyTag, handleCreateTag, clearSelection],
  );

  if (!visible) {
    return null;
  }

  return (
    <div className="batch-tag-bar">
      <div className="batch-tag-bar-input-container">
        {suggestions.length > 0 && (
          <div className="batch-tag-suggestions">
            {suggestions.map((tag, i) => (
              <div
                key={tag.tag.id}
                className={`batch-tag-suggestion ${i === activeIndex ? 'active' : ''}`}
                onMouseEnter={() => setActiveIndex(i)}
                onClick={() => applyTag(tag.tag.id)}
              >
                {tag.tag.name}
                {tag.count !== undefined && <span className="batch-tag-count">{tag.count}</span>}
              </div>
            ))}
          </div>
        )}
        <input
          ref={inputRef}
          className="batch-tag-input"
          value={query}
          placeholder={`Добавить тег к ${selectedMediaIds.length} файл(ам)...`}
          onChange={(e) => {
            setQuery(e.target.value);
            setActiveIndex(0);
          }}
          onKeyDown={handleKeyDown}
        />
      </div>
    </div>
  );
}