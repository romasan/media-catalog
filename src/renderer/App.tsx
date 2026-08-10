import { observer } from 'mobx-react-lite';
import React, { useCallback, useState } from 'react';
import './styles.css';
import { useApp } from './store/AppStore';
import { MediaGrid } from './components/MediaGrid';
import { FilterBar } from './components/FilterBar';
import { BurgerMenu } from './components/BurgerMenu';
import { BatchTagBar } from './components/BatchTagBar';
import { FullscreenViewer } from './components/FullscreenViewer';
import { ToastContainer } from './components/ToastContainer';
import { ThumbnailProgressBar } from './components/ThumbnailProgressBar';
import type { MediaFile } from '../shared/types';

const AppContent = observer(function AppContent(): React.ReactElement {
  const { mediaItems, addTagToFilter } = useApp();
  const [fullscreenIndex, setFullscreenIndex] = useState<number | null>(null);

  const handleNavigate = useCallback(
    (direction: number) => {
      setFullscreenIndex((prev) => {
        if (prev === null || mediaItems.length === 0) {
          return prev;
        }
        const next = (prev + direction + mediaItems.length) % mediaItems.length;
        return next;
      });
    },
    [mediaItems.length],
  );

  const handleOpenFullscreen = useCallback(
    (media: MediaFile) => {
      const index = mediaItems.findIndex((m) => m.id === media.id);
      if (index >= 0) {
        setFullscreenIndex(index);
      }
    },
    [mediaItems],
  );

  const fullscreenMedia = fullscreenIndex !== null ? mediaItems[fullscreenIndex] : null;

  return (
    <div className="app">
      <FilterBar />
      <MediaGrid onOpenFullscreen={handleOpenFullscreen} />
      <BatchTagBar />
      <BurgerMenu />
      <ThumbnailProgressBar />
      <ToastContainer />
      {fullscreenMedia && fullscreenIndex !== null && (
        <FullscreenViewer
          media={fullscreenMedia}
          onClose={() => setFullscreenIndex(null)}
          onNavigate={handleNavigate}
          onAddTagToFilter={addTagToFilter}
        />
      )}
    </div>
  );
});

export default function App(): React.ReactElement {
  return <AppContent />;
}
