import React, { useEffect, useRef, useState } from 'react';
import type { ThumbnailProgress } from '../../shared/types';

export function ThumbnailProgressBar(): React.ReactElement | null {
  const [progress, setProgress] = useState<ThumbnailProgress | null>(null);
  const [visible, setVisible] = useState(false);
  const hideTimerRef = useRef<ReturnType<typeof setTimeout> | null>(null);

  useEffect(() => {
    const unsubscribe = window.api.onThumbnailProgress((p) => {
      setProgress(p);

      // Сбрасываем таймер скрытия
      if (hideTimerRef.current) {
        clearTimeout(hideTimerRef.current);
        hideTimerRef.current = null;
      }

      if (p.total > 0) {
        if (p.processed < p.total) {
          setVisible(true);
        } else if (p.processed >= p.total) {
          // Завершено — скрываем через 300мс
          hideTimerRef.current = setTimeout(() => {
            setVisible(false);
            setProgress(null);
            hideTimerRef.current = null;
          }, 300);
        }
      }
    });
    return () => {
      unsubscribe();
      if (hideTimerRef.current) {
        clearTimeout(hideTimerRef.current);
      }
    };
  }, []);

  if (!progress || !visible || progress.total === 0) {
    return null;
  }

  const percent = Math.min(100, Math.max(0, Math.round((progress.processed / progress.total) * 100)));

  return (
    <div className="thumbnail-progress-container" title={`Генерация превью: ${progress.processed} из ${progress.total}`}>
      <div className="thumbnail-progress-bar">
        <div className="thumbnail-progress-fill" style={{ width: `${percent}%` }} />
      </div>
      <span className="thumbnail-progress-text">
        {percent}% · {progress.processed}/{progress.total}
      </span>
    </div>
  );
}