import { ipcMain, IpcMain, BrowserWindow, OpenDialogOptions, SaveDialogOptions } from 'electron';
import fs from 'fs';
import path from 'path';
import { IPC, MediaFilters, MediaStreamRequest } from '../shared/ipc';
import type { Catalog, CatalogStats, ImportExportData, MediaFile, MetaTag, MetaTagSearchResult, ScanResult, Tag, TagSearchResult } from '../shared/types';
import { getAllMetaTags, getMediaMatchesMetaTag, getMetaTagsForFile, isMetaTagId } from '../shared/metaTags';
import { Database } from './database';
import { CatalogScanner } from './scanner';
import { ThumbnailGenerator } from './thumbnails';

export interface IpcHandlerContext {
  ipcMain: IpcMain;
  database: Database;
  scanner: CatalogScanner;
  thumbnailGenerator: ThumbnailGenerator;
  readFile: (options: OpenDialogOptions) => Promise<{ canceled: boolean; filePaths: string[] }>;
  writeFileDialog: (options: SaveDialogOptions) => Promise<{ canceled: boolean; filePath?: string }>;
  getMainWindow: () => BrowserWindow | null;
  runScan: () => Promise<ScanResult>;
}

export function registerIpcHandlers(context: IpcHandlerContext): void {
  const { ipcMain, database, scanner, thumbnailGenerator, readFile, writeFileDialog, runScan } = context;

  // ==== Каталоги ====

  ipcMain.handle(IPC.GetCatalogs, (): Catalog[] => {
    return database.getCatalogs();
  });

  ipcMain.handle(IPC.AddCatalog, async (): Promise<Catalog[]> => {
    const result = await readFile({
      title: 'Выберите папку для сканирования',
      properties: ['openDirectory', 'createDirectory'],
    });

    if (result.canceled || result.filePaths.length === 0) {
      return database.getCatalogs();
    }

    const dirPath = result.filePaths[0];
    database.addCatalog(dirPath);

    // Запускаем фоновое сканирование
    setTimeout(() => {
      runScan();
    }, 100);

    return database.getCatalogs();
  });

  ipcMain.handle(IPC.RemoveCatalog, (event, catalogId: string): void => {
    // Удаляем превью файлов этого каталога
    const mediaFiles = database.getMediaFilesByCatalog(catalogId);
    thumbnailGenerator.deleteThumbnailsForFiles(mediaFiles);
    database.removeCatalog(catalogId);
  });

  ipcMain.handle(IPC.GetCatalogStats, (): CatalogStats[] => {
    const catalogs = database.getCatalogs();
    const mediaFiles = database.getMediaFiles();
    const stats: CatalogStats[] = [];

    for (const catalog of catalogs) {
      const files = mediaFiles.filter((m) => m.catalogId === catalog.id);
      stats.push({
        catalogId: catalog.id,
        photoCount: files.filter((m) => m.type === 'photo').length,
        videoCount: files.filter((m) => m.type === 'video').length,
      });
    }

    return stats;
  });

  // ==== Сканирование ====

  ipcMain.handle(IPC.StartScan, async (): Promise<ScanResult> => {
    const result = await runScan();
    // Если результат обнулён (не найдено каталогов), возвращаем пустой
    return result;
  });

  // ==== Медиа ====

  ipcMain.handle(
    IPC.GetMedia,
    (event, filters: MediaFilters): { items: MediaFile[]; total: number } => {
      let media = database.getMediaFiles();

      // Применяем фильтр по тегам
      if (filters.filter && filters.filter.tagIds.length > 0) {
        const { tagIds, mode } = filters.filter;
        const mediaIdsByTag: Array<Set<string>> = tagIds.map((tagId) =>
          database.getMediaIdsByTag(tagId),
        );

        media = media.filter((m) => {
          const matches = (tagId: string, index: number): boolean => {
            if (isMetaTagId(tagId)) {
              return getMediaMatchesMetaTag(m, tagId);
            }
            return mediaIdsByTag[index].has(m.id);
          };

          if (mode === 'AND') {
            return tagIds.every((tagId, index) => matches(tagId, index));
          } else {
            return tagIds.some((tagId, index) => matches(tagId, index));
          }
        });
      }

      // Сортировка: по дате создания, новые сверху
      media.sort((a, b) => b.createdAt - a.createdAt);

      const total = media.length;
      const { limit, offset } = filters;
      if (typeof limit === 'number' && typeof offset === 'number') {
        media = media.slice(offset, offset + limit);
      }

      return { items: media, total };
    },
  );

  ipcMain.handle(IPC.GetMediaTags, (event, mediaId: string): Tag[] => {
    return database.getMediaTags(mediaId);
  });

  // Стриминг медиафайлов через protocol
  ipcMain.handle(IPC.GetMediaStreamUrl, (event, request: MediaStreamRequest): string => {
    const encodedPath = Buffer.from(request.filePath).toString('base64url');
    return `media-stream://local/${encodedPath}?type=${request.type}`;
  });

  // ==== Теги ====

  ipcMain.handle(IPC.GetTags, (): TagSearchResult[] => {
    const tags = database.getTags().map((tag) => ({
      tag,
      count: database.getTagUsageCount(tag.id),
    }));
    // Сортировка по последней дате применения (пустые — в конце)
    tags.sort((a, b) => {
      if (b.tag.lastUsedAt === a.tag.lastUsedAt) {
        return a.tag.name.localeCompare(b.tag.name);
      }
      return b.tag.lastUsedAt - a.tag.lastUsedAt;
    });
    return tags;
  });

  ipcMain.handle(IPC.SearchTags, (event, query: string): TagSearchResult[] => {
    const normalizedQuery = query.trim().toLowerCase();
    const tags = database
      .getTags()
      .filter((t) => t.name.toLowerCase().includes(normalizedQuery))
      .map((tag) => ({
        tag,
        count: database.getTagUsageCount(tag.id),
      }));
    tags.sort((a, b) => {
      if (b.tag.lastUsedAt === a.tag.lastUsedAt) {
        return a.tag.name.localeCompare(b.tag.name);
      }
      return b.tag.lastUsedAt - a.tag.lastUsedAt;
    });
    return tags.slice(0, 30);
  });

  ipcMain.handle(IPC.CreateTag, (event, name: string): Tag[] => {
    database.createTag(name);
    return database.getTags();
  });

  ipcMain.handle(IPC.DeleteTag, (event, tagId: string): void => {
    database.deleteTag(tagId);
  });

  ipcMain.handle(IPC.ApplyTag, (event, mediaId: string, tagId: string): void => {
    database.applyTag(mediaId, tagId);
  });

  ipcMain.handle(IPC.RemoveTagFromMedia, (event, mediaId: string, tagId: string): void => {
    database.removeTagFromMedia(mediaId, tagId);
  });

  // ==== Метатеги ====

  ipcMain.handle(IPC.GetMetaTags, (): MetaTagSearchResult[] => {
    const files = database.getMediaFiles();
    const metaTags = getAllMetaTags(files);
    return metaTags.map((metaTag) => ({
      metaTag,
      count: files.filter((file) => getMediaMatchesMetaTag(file, metaTag.id)).length,
    }));
  });

  ipcMain.handle(IPC.GetMediaMetaTags, (event, mediaId: string): MetaTag[] => {
    const file = database.getMediaFileById(mediaId);
    if (!file) {
      return [];
    }
    return getMetaTagsForFile(file);
  });

  // ==== Экспорт/импорт ====

  ipcMain.handle(IPC.ExportData, async (event, filePath: string): Promise<void> => {
    const mediaFiles = database.getMediaFiles();
    const catalogs = database.getCatalogs();
    const tags = database.getTags();
    const mediaTags = database.getMediaTagRelations();

    const data: ImportExportData = {
      version: 1,
      exportedAt: Date.now(),
      catalogs,
      tags,
      mediaTags,
      files: mediaFiles.map((m) => ({
        path: m.path,
        createdAt: m.createdAt,
        modifiedAt: m.modifiedAt,
        size: m.size,
      })),
    };

    // Если путь не указан — показываем диалог
    let outputPath = filePath;
    if (!outputPath) {
      const result = await writeFileDialog({
        title: 'Экспорт данных',
        defaultPath: `media-catalog-export-${Date.now()}.json`,
        filters: [{ name: 'JSON', extensions: ['json'] }],
      });
      if (result.canceled || !result.filePath) {
        return;
      }
      outputPath = result.filePath;
    }

    fs.writeFileSync(outputPath, JSON.stringify(data, null, 2), 'utf-8');
  });

  ipcMain.handle(IPC.ImportData, async (event, filePath: string): Promise<ScanResult | null> => {
    let importPath = filePath;

    if (!importPath) {
      const result = await readFile({
        title: 'Импорт данных',
        properties: ['openFile'],
        filters: [{ name: 'JSON', extensions: ['json'] }],
      });
      if (result.canceled || result.filePaths.length === 0) {
        return null;
      }
      importPath = result.filePaths[0];
    }

    try {
      const raw = fs.readFileSync(importPath, 'utf-8');
      const data = JSON.parse(raw) as ImportExportData;

      if (!data.catalogs || !Array.isArray(data.catalogs)) {
        throw new Error('Неверный формат файла импорта');
      }

      // Создаём каталоги
      for (const catalog of data.catalogs) {
        database.addCatalog(catalog.path);
      }

      // Восстанавливаем теги
      for (const tag of data.tags) {
        const existing = database.findTagByName(tag.name);
        if (existing) {
          existing.lastUsedAt = Math.max(existing.lastUsedAt, tag.lastUsedAt);
        } else {
          const newTag = database.createTag(tag.name);
          newTag.lastUsedAt = tag.lastUsedAt;
        }
      }

      // Восстанавливаем связи тегов с файлами (после сканирования)
      const savedMediaTags = data.mediaTags || [];
      const normalizedPaths = new Map<string, MediaFile>();
      for (const file of data.files) {
        const fullPath = path.resolve(file.path);
        normalizedPaths.set(fullPath, {
          id: file.path,
          path: fullPath,
          name: path.basename(fullPath),
          type: 'photo',
          size: file.size,
          createdAt: file.createdAt,
          modifiedAt: file.modifiedAt,
          catalogId: '',
          thumbnailPath: '',
        });
      }

      // Сохраняем временные связи: они будут применены после сканирования
      // Для этого сохраняем во временную таблицу в самом объекте данных
      const tagByName = new Map(data.tags.map((t) => [t.name.toLowerCase(), t]));

      // После сканирования восстановим связи по путям
      // Пока что просто сохраняем информацию для последующего применения
      (global as any).__pendingImportMediaTags = data.mediaTags;
      (global as any).__pendingImportFilePaths = data.files.map((f) => f.path);

      // Запускаем сканирование, которое добавит новые файлы
      const scanResult = await runScan();

      return scanResult;
    } catch (error) {
      console.error('Ошибка импорта данных:', error);
      throw error;
    }
  });
}