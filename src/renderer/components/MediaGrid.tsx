import React, { useCallback, useEffect, useMemo, useRef, useState } from 'react';
import { useApp } from '../store/AppContext';
import type { MediaFile } from '../../shared/types';

interface MediaGridProps {
  onOpenFullscreen: (media: MediaFile) => void;
}

interface GridState {
  itemSize: number;
  columns: number;
  rows: number;
  visibleStart: number;
  visibleEnd: number;
}

const MIN_ITEM_SIZE = 100;
const OVERSCAN_ROWS = 3;

function useElementSize(ref: React.RefObject<HTMLDivElement>): { width: number; height: number } {
  const [size, setSize] = useState({ width: 0, height: 0 });

  useEffect(() => {
    const element = ref.current;
    if (!element) {
      return;
    }

    const observer = new ResizeObserver((entries) => {
      const entry = entries[0];
      if (entry) {
        setSize({
          width: entry.contentRect.width,
          height: entry.contentRect.height,
        });
      }
    });

    observer.observe(element);
    return () => observer.disconnect();
  }, [ref]);

  return size;
}

export function MediaGrid({ onOpenFullscreen }: MediaGridProps): React.ReactElement {
  const { mediaItems, isLoadingMedia } = useApp();
  const containerRef = useRef<HTMLDivElement>(null);
  const { width: containerWidth, height: containerHeight } = useElementSize(containerRef);
  const [scrollTop, setScrollTop] = useState(0);

  // Вычисляем размер сетки
  const grid = useMemo<GridState>(() => {
    if (containerWidth <= 0) {
      return {
        itemSize: 200,
        columns: 1,
        rows: 0,
        visibleStart: 0,
        visibleEnd: 0,
      };
    }

    // Максимальное число колонок, при котором размер ячейки не меньше MIN_ITEM_SIZE (100px).
    // Карточки заполняют всю ширину контейнера, а размер остаётся в диапазоне [100, 200].
    const columns = Math.max(1, Math.floor(containerWidth / MIN_ITEM_SIZE));
    const itemSize = containerWidth / columns;

    const rows = Math.ceil(mediaItems.length / columns);

    // Вычисляем видимые строки
    const viewportHeight = containerHeight;
    const visibleRows = Math.ceil(viewportHeight / itemSize) + OVERSCAN_ROWS * 2;
    const startRow = Math.max(0, Math.floor(scrollTop / itemSize) - OVERSCAN_ROWS);
    const endRow = Math.min(rows, startRow + visibleRows);

    return {
      itemSize,
      columns,
      rows,
      visibleStart: startRow * columns,
      visibleEnd: endRow * columns,
    };
  }, [containerWidth, containerHeight, mediaItems.length, scrollTop]);

  const handleScroll = useCallback((e: React.UIEvent<HTMLDivElement>) => {
    setScrollTop(e.currentTarget.scrollTop);
  }, []);

  const visibleItems = useMemo(() => {
    return mediaItems.slice(grid.visibleStart, grid.visibleEnd);
  }, [mediaItems, grid.visibleStart, grid.visibleEnd]);

  const totalHeight = grid.rows * grid.itemSize;

  return (
    <div className="media-grid-container" ref={containerRef} onScroll={handleScroll}>
      {containerWidth > 0 && (
        <div
          className="media-grid"
          style={{
            width: containerWidth,
            height: totalHeight,
            position: 'relative',
          }}
        >
          {visibleItems.map((media, index) => {
            const absoluteIndex = grid.visibleStart + index;
            const row = Math.floor(absoluteIndex / grid.columns);
            const col = absoluteIndex % grid.columns;
            return (
              <MediaCard
                key={media.id}
                media={media}
                style={{
                  position: 'absolute',
                  top: row * grid.itemSize,
                  left: col * grid.itemSize,
                  width: grid.itemSize,
                  height: grid.itemSize,
                }}
                onClick={() => onOpenFullscreen(media)}
              />
            );
          })}
        </div>
      )}
      {isLoadingMedia && <div className="loading-overlay">Загрузка...</div>}
      {!isLoadingMedia && mediaItems.length === 0 && (
        <div className="empty-state">
          Каталог пуст. Добавьте папки через меню (☰).
        </div>
      )}
    </div>
  );
}

interface MediaCardProps {
  media: MediaFile;
  style: React.CSSProperties;
  onClick: () => void;
}

function MediaCard({ media, style, onClick }: MediaCardProps): React.ReactElement {
  const [thumbUrl, setThumbUrl] = useState<string>('');

  useEffect(() => {
    let cancelled = false;

    if (media.thumbnailPath && media.thumbnailPath.length > 0) {
      // Превью уже сгенерировано — используем его через стрим-протокол
      window.api.getMediaStreamUrl({ filePath: media.thumbnailPath, type: 'photo' })
        .then((url) => {
          if (!cancelled) {
            setThumbUrl(url);
          }
        })
        .catch(() => {
          if (!cancelled) {
            setThumbUrl('');
          }
        });
    }

    return () => {
      cancelled = true;
    };
  }, [media.thumbnailPath]);

  const isVideo = media.type === 'video';

  return (
    <div
      className={`media-card ${isVideo ? 'media-card-video' : ''}`}
      style={style}
      onClick={onClick}
      title={media.name}
    >
      {thumbUrl ? (
        <img src={thumbUrl} alt={media.name} loading="lazy" />
      ) : (
        <div className="media-card-placeholder">
          {isVideo ? '🎬' : '🖼️'}
        </div>
      )}
      {isVideo && <div className="video-badge">▶</div>}
    </div>
  );
}