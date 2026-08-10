import type {
  Catalog,
  CatalogStats,
  FilterCondition,
  MediaFile,
  MetaTag,
  MetaTagSearchResult,
  ScanResult,
  Tag,
  TagSearchResult,
  ThumbnailProgress,
} from './types';

export const IPC = {
  // Каталоги
  GetCatalogs: 'catalogs:get',
  AddCatalog: 'catalogs:add',
  RemoveCatalog: 'catalogs:remove',
  GetCatalogStats: 'catalogs:stats',

  // Сканирование
  StartScan: 'scan:start',
  OnScanComplete: 'scan:complete',
  OnScanProgress: 'scan:progress',

  // Прогресс генерации превью
  OnThumbnailProgress: 'thumbnails:progress',
  OnThumbnailReady: 'thumbnails:ready',

  // Медиа
  GetMedia: 'media:get',
  GetMediaTags: 'media:tags:get',
  GetMediaStreamUrl: 'media:stream:url',

  // Теги
  GetTags: 'tags:get',
  SearchTags: 'tags:search',
  CreateTag: 'tags:create',
  DeleteTag: 'tags:delete',
  ApplyTag: 'tags:apply',
  ApplyTagToMedia: 'tags:apply-bulk',
  RemoveTagFromMedia: 'tags:remove-from-media',

  // Метатеги
  GetMetaTags: 'meta-tags:get',
  GetMediaMetaTags: 'meta-tags:media:get',

  // Экспорт/импорт
  ExportData: 'data:export',
  ImportData: 'data:import',
} as const;

export interface MediaStreamRequest {
  filePath: string;
  type: 'photo' | 'video';
}

export interface MediaFilters {
  filter?: FilterCondition;
  limit?: number;
  offset?: number;
}

export interface Api {
  getCatalogs(): Promise<Catalog[]>;
  addCatalog(): Promise<Catalog[]>;
  removeCatalog(catalogId: string): Promise<void>;
  getCatalogStats(): Promise<CatalogStats[]>;

  startScan(): Promise<ScanResult>;
  onScanComplete(callback: (result: ScanResult) => void): () => void;
  onThumbnailProgress(callback: (progress: ThumbnailProgress) => void): () => void;
  onThumbnailReady(callback: (event: { mediaId: string; thumbnailPath: string }) => void): () => void;

  getMedia(filters: MediaFilters): Promise<{ items: MediaFile[]; total: number }>;
  getMediaTags(mediaId: string): Promise<Tag[]>;
  getMediaStreamUrl(request: MediaStreamRequest): Promise<string>;

  getTags(): Promise<TagSearchResult[]>;
  searchTags(query: string): Promise<TagSearchResult[]>;
  createTag(name: string): Promise<Tag[]>;
  deleteTag(tagId: string): Promise<void>;
  applyTag(mediaId: string, tagId: string): Promise<void>;
  applyTagToMedia(mediaIds: string[], tagId: string): Promise<void>;
  removeTagFromMedia(mediaId: string, tagId: string): Promise<void>;

  getMetaTags(): Promise<MetaTagSearchResult[]>;
  getMediaMetaTags(mediaId: string): Promise<MetaTag[]>;

  exportData(): Promise<void>;
  importData(): Promise<ScanResult | null>;
}