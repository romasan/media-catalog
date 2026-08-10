import { contextBridge, ipcRenderer } from 'electron';
import { IPC, Api } from '../shared/ipc';
import type { Catalog, CatalogStats, FilterCondition, ImportExportData, MediaFile, ScanResult, Tag, TagSearchResult, ThumbnailProgress } from '../shared/types';

const api: Api = {
  // Каталоги
  getCatalogs: () => ipcRenderer.invoke(IPC.GetCatalogs),
  addCatalog: () => ipcRenderer.invoke(IPC.AddCatalog),
  removeCatalog: (catalogId: string) => ipcRenderer.invoke(IPC.RemoveCatalog, catalogId),
  getCatalogStats: () => ipcRenderer.invoke(IPC.GetCatalogStats),

  // Сканирование
  startScan: () => ipcRenderer.invoke(IPC.StartScan),
  onScanComplete: (callback: (result: ScanResult) => void) => {
    const listener = (_event: unknown, result: ScanResult) => callback(result);
    ipcRenderer.on(IPC.OnScanComplete, listener);
    return () => {
      ipcRenderer.removeListener(IPC.OnScanComplete, listener);
    };
  },
  onThumbnailProgress: (callback: (progress: ThumbnailProgress) => void) => {
    const listener = (_event: unknown, progress: ThumbnailProgress) => callback(progress);
    ipcRenderer.on(IPC.OnThumbnailProgress, listener);
    return () => {
      ipcRenderer.removeListener(IPC.OnThumbnailProgress, listener);
    };
  },
  onThumbnailReady: (callback: (event: { mediaId: string; thumbnailPath: string }) => void) => {
    const listener = (_event: unknown, data: { mediaId: string; thumbnailPath: string }) => callback(data);
    ipcRenderer.on(IPC.OnThumbnailReady, listener);
    return () => {
      ipcRenderer.removeListener(IPC.OnThumbnailReady, listener);
    };
  },

  // Медиа
  getMedia: (filters) => ipcRenderer.invoke(IPC.GetMedia, filters),
  getMediaTags: (mediaId: string) => ipcRenderer.invoke(IPC.GetMediaTags, mediaId),
  getMediaStreamUrl: (request) => ipcRenderer.invoke(IPC.GetMediaStreamUrl, request),

  // Теги
  getTags: () => ipcRenderer.invoke(IPC.GetTags),
  searchTags: (query: string) => ipcRenderer.invoke(IPC.SearchTags, query),
  createTag: (name: string) => ipcRenderer.invoke(IPC.CreateTag, name),
  deleteTag: (tagId: string) => ipcRenderer.invoke(IPC.DeleteTag, tagId),
  applyTag: (mediaId: string, tagId: string) => ipcRenderer.invoke(IPC.ApplyTag, mediaId, tagId),
  removeTagFromMedia: (mediaId: string, tagId: string) => ipcRenderer.invoke(IPC.RemoveTagFromMedia, mediaId, tagId),

  // Метатеги
  getMetaTags: () => ipcRenderer.invoke(IPC.GetMetaTags),
  getMediaMetaTags: (mediaId: string) => ipcRenderer.invoke(IPC.GetMediaMetaTags, mediaId),

  // Экспорт/импорт
  exportData: () => ipcRenderer.invoke(IPC.ExportData),
  importData: () => ipcRenderer.invoke(IPC.ImportData),
};

contextBridge.exposeInMainWorld('api', api);

export type ApiType = Api;