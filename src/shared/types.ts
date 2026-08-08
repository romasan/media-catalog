export interface MediaFile {
  id: string;
  path: string;
  name: string;
  type: 'photo' | 'video';
  size: number;
  createdAt: number;
  modifiedAt: number;
  catalogId: string;
  thumbnailPath: string;
}

export interface Catalog {
  id: string;
  path: string;
}

export interface Tag {
  id: string;
  name: string;
  lastUsedAt: number;
}

export interface MediaTagRelation {
  mediaId: string;
  tagId: string;
}

export interface CatalogStats {
  catalogId: string;
  photoCount: number;
  videoCount: number;
}

export interface ScanResult {
  addedFiles: number;
  removedFiles: number;
  addedFolders: number;
  removedFolders: number;
  addedMedia: string[];
  removedMedia: string[];
}

export interface ThumbnailProgress {
  total: number;
  processed: number;
}

export interface TagSearchResult {
  tag: Tag;
  count: number;
}

export interface FilterCondition {
  tagIds: string[];
  mode: 'AND' | 'OR';
}

export interface ImportExportData {
  version: 1;
  exportedAt: number;
  catalogs: Catalog[];
  tags: Tag[];
  mediaTags: MediaTagRelation[];
  files: Array<{
    path: string;
    createdAt: number;
    modifiedAt: number;
    size: number;
  }>;
}