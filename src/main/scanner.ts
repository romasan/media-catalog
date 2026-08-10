import fs from 'fs';
import path from 'path';
import { randomUUID } from 'crypto';
import type { Catalog, MediaFile, ScanResult } from '../shared/types';
import { Database } from './database';
import { ThumbnailGenerator, getFileType, isSupportedFile } from './thumbnails';

export class CatalogScanner {
  private database: Database;
  private thumbnailGenerator: ThumbnailGenerator;

  constructor(database: Database, thumbnailGenerator: ThumbnailGenerator) {
    this.database = database;
    this.thumbnailGenerator = thumbnailGenerator;
  }

  async scan(): Promise<ScanResult> {
    const catalogs = this.database.getCatalogs();

    // Собираем все существующие файлы из каталогов
    const foundFiles: Map<string, { path: string; catalogId: string }> = new Map();

    for (const catalog of catalogs) {
      if (!fs.existsSync(catalog.path)) {
        continue;
      }
      this.walkDirectory(catalog.path, catalog, foundFiles);
    }

    const existingMedia = this.database.getMediaFiles();
    const existingByPath = new Map(existingMedia.map((m) => [m.path, m]));

    // Новые файлы, которых нет в базе
    const newMedia: MediaFile[] = [];
    const newMediaPaths: string[] = [];

    for (const [filePath, found] of foundFiles) {
      const existing = existingByPath.get(filePath);
      if (!existing) {
        try {
          const stats = fs.statSync(filePath);
          const type = getFileType(filePath);
          const media: MediaFile = {
            id: randomUUID(),
            path: filePath,
            name: path.basename(filePath),
            type,
            size: stats.size,
            createdAt: this.getFileCreationDate(filePath),
            modifiedAt: stats.mtime.getTime(),
            catalogId: found.catalogId,
            thumbnailPath: '',
            thumbnailRetries: 0,
          };
          newMedia.push(media);
          newMediaPaths.push(filePath);
        } catch (error) {
          console.error(`Ошибка чтения файла ${filePath}:`, error);
        }
      } else {
        // Проверяем, не изменился ли файл
        try {
          const stats = fs.statSync(filePath);
          if (stats.mtime.getTime() !== existing.modifiedAt || stats.size !== existing.size) {
            existing.modifiedAt = stats.mtime.getTime();
            existing.size = stats.size;
            existing.createdAt = this.getFileCreationDate(filePath);
          }
        } catch {
          // Файл недоступен — пропускаем
        }
      }
    }

    // Удалённые файлы
    const removedPaths = new Set<string>();
    const existingPaths = new Set(existingMedia.map((m) => m.path));
    for (const existingPath of existingPaths) {
      if (!foundFiles.has(existingPath)) {
        removedPaths.add(existingPath);
      }
    }

    if (newMedia.length > 0) {
      this.database.upsertMediaFiles(newMedia);
    }

    let removedFiles = 0;
    if (removedPaths.size > 0) {
      // Удаляем превью удалённых файлов
      const removedMedia = this.database.getMediaFilesByPaths(removedPaths);
      this.thumbnailGenerator.deleteThumbnailsForFiles(removedMedia);
      removedFiles = this.database.removeMediaByPaths(removedPaths).length;
    }

    // Восстанавливаем очередь превью: добавляем новые файлы и файлы,
    // оставшиеся без превью после прошлого запуска (приложение могло быть
    // остановлено во время генерации). Дедупликация выполняется внутри
    // ThumbnailGenerator, поэтому повторная постановка безопасна.
    const filesNeedingThumbnails = [
      ...newMedia,
      ...this.database.getMediaWithoutThumbnail(),
    ];
    if (filesNeedingThumbnails.length > 0) {
      this.thumbnailGenerator.queueThumbnails(filesNeedingThumbnails);
    }

    return {
      addedFiles: newMedia.length,
      removedFiles,
      addedFolders: 0,
      removedFolders: 0,
      addedMedia: newMediaPaths,
      removedMedia: Array.from(removedPaths),
    };
  }

  private walkDirectory(
    dirPath: string,
    catalog: Catalog,
    foundFiles: Map<string, { path: string; catalogId: string }>,
  ): void {
    let entries: fs.Dirent[];
    try {
      entries = fs.readdirSync(dirPath, { withFileTypes: true });
    } catch {
      return;
    }

    for (const entry of entries) {
      const fullPath = path.join(dirPath, entry.name);

      // Пропускаем скрытые папки и системные
      if (entry.name.startsWith('.') || entry.name === '@eaDir' || entry.name === 'System Volume Information') {
        continue;
      }

      if (entry.isDirectory()) {
        this.walkDirectory(fullPath, catalog, foundFiles);
      } else if (entry.isFile()) {
        if (isSupportedFile(fullPath)) {
          // Проверяем, не перекрывается ли файл другим каталогом
          // Если файл уже найден в другом каталоге — это дубликат по пути
          // Такое возможно при вложенных каталогах. Оставляем первый найденный.
          if (!foundFiles.has(fullPath)) {
            foundFiles.set(fullPath, { path: fullPath, catalogId: catalog.id });
          }
        }
      }
    }
  }

  private getFileCreationDate(filePath: string): number {
    try {
      const stats = fs.statSync(filePath);
      const birthtime = stats.birthtime.getTime();
      return birthtime > 0 ? birthtime : stats.mtime.getTime();
    } catch {
      return Date.now();
    }
  }

  /**
   * Обновляет запись медиафайла после генерации превью
   */
  updateThumbnailInDb(mediaId: string, thumbnailPath: string): void {
    this.database.updateThumbnail(mediaId, thumbnailPath);
  }

  /**
   * Получить все медиафайлы
   */
  getAllMedia(): MediaFile[] {
    return this.database.getMediaFiles();
  }

  /**
   * Получить все каталоги
   */
  getCatalogs(): Catalog[] {
    return this.database.getCatalogs();
  }
}