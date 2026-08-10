import React, { createContext, useCallback, useContext, useEffect, useMemo, useState } from 'react';
import type { Catalog, CatalogStats, FilterCondition, MediaFile, MetaTagSearchResult, ScanResult, TagSearchResult } from '../../shared/types';
import type { MediaFilters } from '../../shared/ipc';

interface Toast {
  id: number;
  message: string;
  type: 'info' | 'success' | 'error';
}

interface AppState {
  catalogs: Catalog[];
  catalogStats: CatalogStats[];
  tags: TagSearchResult[];
  metaTags: MetaTagSearchResult[];
  filter: FilterCondition;
  mediaItems: MediaFile[];
  mediaTotal: number;
  isLoadingMedia: boolean;
  toasts: Toast[];

  // Действия
  loadCatalogs: () => Promise<void>;
  loadCatalogStats: () => Promise<void>;
  loadTags: () => Promise<void>;
  loadMetaTags: () => Promise<void>;
  addCatalog: () => Promise<void>;
  removeCatalog: (catalogId: string) => Promise<void>;
  loadMedia: () => Promise<void>;
  setFilter: (filter: FilterCondition) => void;
  addTagToFilter: (tagId: string) => void;
  removeTagFromFilter: (tagId: string) => void;
  setFilterMode: (mode: 'AND' | 'OR') => void;
  showToast: (message: string, type?: 'info' | 'success' | 'error') => void;
  dismissToast: (id: number) => void;
  runScan: () => Promise<void>;
  exportData: () => Promise<void>;
  importData: () => Promise<void>;
}

const AppContext = createContext<AppState | null>(null);

export function AppProvider({ children }: { children: React.ReactNode }): React.ReactElement {
  const [catalogs, setCatalogs] = useState<Catalog[]>([]);
  const [catalogStats, setCatalogStats] = useState<CatalogStats[]>([]);
  const [tags, setTags] = useState<TagSearchResult[]>([]);
  const [metaTags, setMetaTags] = useState<MetaTagSearchResult[]>([]);
  const [filter, setFilterState] = useState<FilterCondition>({ tagIds: [], mode: 'OR' });
  const [mediaItems, setMediaItems] = useState<MediaFile[]>([]);
  const [mediaTotal, setMediaTotal] = useState(0);
  const [isLoadingMedia, setIsLoadingMedia] = useState(false);
  const [toasts, setToasts] = useState<Toast[]>([]);

  const showToast = useCallback((message: string, type: 'info' | 'success' | 'error' = 'info') => {
    const id = Date.now() + Math.random();
    setToasts((prev) => [...prev, { id, message, type }]);
    setTimeout(() => {
      setToasts((prev) => prev.filter((t) => t.id !== id));
    }, 4000);
  }, []);

  const dismissToast = useCallback((id: number) => {
    setToasts((prev) => prev.filter((t) => t.id !== id));
  }, []);

  const loadCatalogs = useCallback(async () => {
    const items = await window.api.getCatalogs();
    setCatalogs(items);
  }, []);

  const loadCatalogStats = useCallback(async () => {
    const stats = await window.api.getCatalogStats();
    setCatalogStats(stats);
  }, []);

  const loadTags = useCallback(async () => {
    const items = await window.api.getTags();
    setTags(items);
  }, []);

  const loadMetaTags = useCallback(async () => {
    const items = await window.api.getMetaTags();
    setMetaTags(items);
  }, []);

  const loadMedia = useCallback(async () => {
    setIsLoadingMedia(true);
    try {
      const filters: MediaFilters = { filter };
      const result = await window.api.getMedia(filters);
      setMediaItems(result.items);
      setMediaTotal(result.total);
    } finally {
      setIsLoadingMedia(false);
    }
  }, [filter]);

  const runScan = useCallback(async () => {
    try {
      // Результат сканирования приходит через событие onScanComplete,
      // где показывается тост — здесь показывать не нужно, чтобы не дублировать
      await window.api.startScan();
    } catch (error) {
      console.error('Ошибка сканирования:', error);
      showToast('Ошибка сканирования каталогов', 'error');
    }
    await loadCatalogs();
    await loadCatalogStats();
    await loadMetaTags();
    await loadMedia();
  }, [showToast, loadCatalogs, loadCatalogStats, loadMetaTags, loadMedia]);

  const exportData = useCallback(async () => {
    try {
      await window.api.exportData();
      showToast('Данные экспортированы', 'success');
    } catch (error) {
      console.error('Ошибка экспорта данных:', error);
      showToast('Ошибка экспорта данных', 'error');
    }
  }, [showToast]);

  const importData = useCallback(async () => {
    try {
      const result = await window.api.importData();
      if (result === null) {
        return;
      }
      await loadCatalogs();
      await loadCatalogStats();
      await loadTags();
      await loadMetaTags();
      await loadMedia();
      showToast('Данные импортированы', 'success');
    } catch (error) {
      console.error('Ошибка импорта данных:', error);
      showToast('Ошибка импорта данных', 'error');
    }
  }, [showToast, loadCatalogs, loadCatalogStats, loadTags, loadMetaTags, loadMedia]);

  const addCatalog = useCallback(async () => {
    await window.api.addCatalog();
    await loadCatalogs();
    await loadCatalogStats();
    await runScan();
  }, [loadCatalogs, loadCatalogStats, runScan]);

  const removeCatalog = useCallback(async (catalogId: string) => {
    await window.api.removeCatalog(catalogId);
    await loadCatalogs();
    await loadCatalogStats();
    await loadMedia();
  }, [loadCatalogs, loadCatalogStats, loadMedia]);

  const setFilter = useCallback((newFilter: FilterCondition) => {
    setFilterState(newFilter);
  }, []);

  const addTagToFilter = useCallback((tagId: string) => {
    setFilterState((prev) => {
      if (prev.tagIds.includes(tagId)) {
        return prev;
      }
      return { ...prev, tagIds: [...prev.tagIds, tagId] };
    });
  }, []);

  const removeTagFromFilter = useCallback((tagId: string) => {
    setFilterState((prev) => ({
      ...prev,
      tagIds: prev.tagIds.filter((id) => id !== tagId),
    }));
  }, []);

  const setFilterMode = useCallback((mode: 'AND' | 'OR') => {
    setFilterState((prev) => ({ ...prev, mode }));
  }, []);

  // Подписка на событие готовности превью
  useEffect(() => {
    const unsubscribe = window.api.onThumbnailReady(({ mediaId, thumbnailPath }) => {
      setMediaItems((prev) =>
        prev.map((m) => (m.id === mediaId ? { ...m, thumbnailPath } : m)),
      );
    });
    return unsubscribe;
  }, []);

  // Подписка на события сканирования
  useEffect(() => {
    const unsubscribe = window.api.onScanComplete((result: ScanResult) => {
      const messages: string[] = [];
      if (result.addedFiles > 0) {
        messages.push(`Добавлено файлов: ${result.addedFiles}`);
      }
      if (result.removedFiles > 0) {
        messages.push(`Удалено файлов: ${result.removedFiles}`);
      }
      if (result.addedFolders > 0) {
        messages.push(`Добавлено папок: ${result.addedFolders}`);
      }
      if (result.removedFolders > 0) {
        messages.push(`Удалено папок: ${result.removedFolders}`);
      }
      if (messages.length > 0) {
        showToast(messages.join(', '), 'success');
      }
      loadCatalogs();
      loadCatalogStats();
      loadMetaTags();
      loadMedia();
    });
    return unsubscribe;
  }, [showToast, loadCatalogs, loadCatalogStats, loadMetaTags, loadMedia]);

  // Первичная загрузка
  useEffect(() => {
    loadCatalogs();
    loadCatalogStats();
    loadTags();
    loadMetaTags();
    loadMedia();
  }, [loadCatalogs, loadCatalogStats, loadTags, loadMetaTags, loadMedia]);

  const value = useMemo<AppState>(
    () => ({
      catalogs,
      catalogStats,
      tags,
      metaTags,
      filter,
      mediaItems,
      mediaTotal,
      isLoadingMedia,
      toasts,
      loadCatalogs,
      loadCatalogStats,
      loadTags,
      loadMetaTags,
      addCatalog,
      removeCatalog,
      loadMedia,
      setFilter,
      addTagToFilter,
      removeTagFromFilter,
      setFilterMode,
      showToast,
      dismissToast,
      runScan,
      exportData,
      importData,
    }),
    [
      catalogs,
      catalogStats,
      tags,
      metaTags,
      filter,
      mediaItems,
      mediaTotal,
      isLoadingMedia,
      toasts,
      loadCatalogs,
      loadCatalogStats,
      loadTags,
      loadMetaTags,
      addCatalog,
      removeCatalog,
      loadMedia,
      setFilter,
      addTagToFilter,
      removeTagFromFilter,
      setFilterMode,
      showToast,
      dismissToast,
      runScan,
      exportData,
      importData,
    ],
  );

  return <AppContext.Provider value={value}>{children}</AppContext.Provider>;
}

export function useApp(): AppState {
  const context = useContext(AppContext);
  if (!context) {
    throw new Error('useApp должен использоваться внутри AppProvider');
  }
  return context;
}