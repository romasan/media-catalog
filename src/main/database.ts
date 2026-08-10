import fs from 'fs';
import path from 'path';
import type { Catalog, MediaFile, MediaTagRelation, Tag } from '../shared/types';
import { randomUUID } from 'crypto';

export interface DatabaseSchema {
  catalogs: Catalog[];
  mediaFiles: MediaFile[];
  tags: Tag[];
  mediaTags: MediaTagRelation[];
}

export class Database {
  private data: DatabaseSchema;
  private dbPath: string;
  private saveTimer: NodeJS.Timeout | null = null;

  constructor(dbPath: string) {
    this.dbPath = dbPath;
    this.data = this.load();
  }

  private load(): DatabaseSchema {
    try {
      if (fs.existsSync(this.dbPath)) {
        const raw = fs.readFileSync(this.dbPath, 'utf-8');
        const parsed = JSON.parse(raw);
        if (
          Array.isArray(parsed.catalogs) &&
          Array.isArray(parsed.mediaFiles) &&
          Array.isArray(parsed.tags) &&
          Array.isArray(parsed.mediaTags)
        ) {
          const data = parsed as DatabaseSchema;
          this.normalizeTagNames(data);
          return data;
        }
      }
    } catch (error) {
      console.error('Ошибка загрузки базы данных:', error);
    }
    return {
      catalogs: [],
      mediaFiles: [],
      tags: [],
      mediaTags: [],
    };
  }

  save(): void {
    // Дебаунс сохранения, чтобы не писать на диск слишком часто
    if (this.saveTimer) {
      clearTimeout(this.saveTimer);
    }
    this.saveTimer = setTimeout(() => {
      try {
        const dir = path.dirname(this.dbPath);
        fs.mkdirSync(dir, { recursive: true });
        const tempPath = `${this.dbPath}.tmp`;
        fs.writeFileSync(tempPath, JSON.stringify(this.data, null, 2), 'utf-8');
        fs.renameSync(tempPath, this.dbPath);
      } catch (error) {
        console.error('Ошибка сохранения базы данных:', error);
      }
    }, 300);
  }

  // ==== Каталоги ====

  getCatalogs(): Catalog[] {
    return [...this.data.catalogs];
  }

  addCatalog(catalogPath: string): Catalog {
    const normalizedPath = path.resolve(catalogPath);
    const existing = this.data.catalogs.find(
      (c) => path.resolve(c.path) === normalizedPath,
    );
    if (existing) {
      return existing;
    }
    const catalog: Catalog = {
      id: randomUUID(),
      path: normalizedPath,
    };
    this.data.catalogs.push(catalog);
    this.save();
    return catalog;
  }

  removeCatalog(catalogId: string): void {
    this.data.catalogs = this.data.catalogs.filter(
      (c) => c.id !== catalogId,
    );
    const removedMediaIds = new Set(
      this.data.mediaFiles
        .filter((m) => m.catalogId === catalogId)
        .map((m) => m.id),
    );
    this.data.mediaFiles = this.data.mediaFiles.filter(
      (m) => m.catalogId !== catalogId,
    );
    this.data.mediaTags = this.data.mediaTags.filter(
      (r) => !removedMediaIds.has(r.mediaId),
    );
    this.cleanupOrphanTags();
    this.save();
  }

  // ==== Медиафайлы ====

  getMediaFiles(): MediaFile[] {
    return [...this.data.mediaFiles];
  }

  getMediaFilesByCatalog(catalogId: string): MediaFile[] {
    return this.data.mediaFiles.filter((m) => m.catalogId === catalogId);
  }

  getMediaFileById(mediaId: string): MediaFile | undefined {
    return this.data.mediaFiles.find((m) => m.id === mediaId);
  }

  getMediaFilesByPaths(paths: Set<string>): MediaFile[] {
    return this.data.mediaFiles.filter((m) => paths.has(m.path));
  }

  upsertMediaFiles(files: MediaFile[]): void {
    const byPath = new Map(
      this.data.mediaFiles.map((m) => [m.path, m]),
    );
    for (const file of files) {
      byPath.set(file.path, file);
    }
    this.data.mediaFiles = Array.from(byPath.values());
    this.save();
  }

  removeMediaByPaths(paths: Set<string>): string[] {
    const removedIds: string[] = [];
    this.data.mediaFiles = this.data.mediaFiles.filter((m) => {
      if (paths.has(m.path)) {
        removedIds.push(m.id);
        return false;
      }
      return true;
    });
    this.data.mediaTags = this.data.mediaTags.filter(
      (r) => !removedIds.includes(r.mediaId),
    );
    this.cleanupOrphanTags();
    this.save();
    return removedIds;
  }

  updateThumbnail(mediaId: string, thumbnailPath: string): void {
    const file = this.data.mediaFiles.find((m) => m.id === mediaId);
    if (file) {
      file.thumbnailPath = thumbnailPath;
      this.save();
    }
  }

  /**
   * Сохраняет дату съёмки, извлечённую из метаданных файла.
   */
  updateCapturedAt(mediaId: string, capturedAt: number): boolean {
    const file = this.data.mediaFiles.find((m) => m.id === mediaId);
    if (file) {
      if (file.capturedAt !== capturedAt) {
        file.capturedAt = capturedAt;
        this.save();
      }
      return true;
    }
    return false;
  }

  // ==== Теги ====

  getTags(): Tag[] {
    return [...this.data.tags];
  }

  findTagByName(name: string): Tag | undefined {
    const normalized = name.trim().toLowerCase();
    return this.data.tags.find(
      (t) => t.name.trim().toLowerCase() === normalized,
    );
  }

  createTag(name: string): Tag {
    const normalizedName = name.trim().toLowerCase();
    const existing = this.findTagByName(normalizedName);
    if (existing) {
      return existing;
    }
    const tag: Tag = {
      id: randomUUID(),
      name: normalizedName,
      lastUsedAt: 0,
    };
    this.data.tags.push(tag);
    this.save();
    return tag;
  }

  deleteTag(tagId: string): void {
    this.data.tags = this.data.tags.filter((t) => t.id !== tagId);
    this.data.mediaTags = this.data.mediaTags.filter(
      (r) => r.tagId !== tagId,
    );
    this.save();
  }

  getMediaTags(mediaId: string): Tag[] {
    const tagIds = new Set(
      this.data.mediaTags
        .filter((r) => r.mediaId === mediaId)
        .map((r) => r.tagId),
    );
    return this.data.tags.filter((t) => tagIds.has(t.id));
  }

  applyTag(mediaId: string, tagId: string): void {
    this.applyTagToMedia([mediaId], tagId);
  }

  applyTagToMedia(mediaIds: string[], tagId: string): void {
    const existing = new Set(
      this.data.mediaTags
        .filter((r) => r.tagId === tagId)
        .map((r) => r.mediaId),
    );
    for (const mediaId of mediaIds) {
      if (!existing.has(mediaId)) {
        this.data.mediaTags.push({ mediaId, tagId });
      }
    }
    const tag = this.data.tags.find((t) => t.id === tagId);
    if (tag) {
      tag.lastUsedAt = Date.now();
    }
    this.save();
  }

  removeTagFromMedia(mediaId: string, tagId: string): void {
    this.data.mediaTags = this.data.mediaTags.filter(
      (r) => !(r.mediaId === mediaId && r.tagId === tagId),
    );
    if (!this.isTagInUse(tagId)) {
      const tag = this.data.tags.find((t) => t.id === tagId);
      if (tag && tag.lastUsedAt > 0) {
        // Тег больше не используется — обновим время последнего использования
        // Оставим тег, но обнулим lastUsedAt
        tag.lastUsedAt = 0;
      }
    }
    this.save();
  }

  getTagUsageCount(tagId: string): number {
    return this.data.mediaTags.filter((r) => r.tagId === tagId).length;
  }

  getMediaIdsByTag(tagId: string): Set<string> {
    return new Set(
      this.data.mediaTags
        .filter((r) => r.tagId === tagId)
        .map((r) => r.mediaId),
    );
  }

  getTaggedMediaIds(): Set<string> {
    return new Set(this.data.mediaTags.map((r) => r.mediaId));
  }

  getMediaTagRelations(): MediaTagRelation[] {
    return [...this.data.mediaTags];
  }

  setMediaTagRelations(relations: MediaTagRelation[]): void {
    this.data.mediaTags = relations;
    this.save();
  }

  // ==== Вспомогательные ====

  /**
   * Приводит имена всех тегов к нижнему регистру и объединяет
   * теги-дубликаты (например, «foo», «Foo» и «fOo»), перенаправляя
   * связи mediaTags на первый тег с данным именем.
   * Выполняется при загрузке базы.
   */
  private normalizeTagNames(data: DatabaseSchema): void {
    const nameToTag = new Map<string, Tag>();
    const mergedIds = new Set<string>();

    for (const tag of data.tags) {
      const normalized = tag.name.trim().toLowerCase();
      const existing = nameToTag.get(normalized);
      if (existing) {
        existing.lastUsedAt = Math.max(existing.lastUsedAt, tag.lastUsedAt);
        for (const relation of data.mediaTags) {
          if (relation.tagId === tag.id) {
            relation.tagId = existing.id;
          }
        }
        mergedIds.add(tag.id);
      } else {
        tag.name = normalized;
        nameToTag.set(normalized, tag);
      }
    }

    if (mergedIds.size > 0) {
      data.tags = data.tags.filter((t) => !mergedIds.has(t.id));
      data.mediaTags = data.mediaTags.filter((r) => !mergedIds.has(r.tagId));
      this.save();
    }
  }

  private isTagInUse(tagId: string): boolean {
    return this.data.mediaTags.some((r) => r.tagId === tagId);
  }

  private cleanupOrphanTags(): void {
    const usedTagIds = new Set(this.data.mediaTags.map((r) => r.tagId));
    this.data.tags = this.data.tags.filter((t) => usedTagIds.has(t.id) || t.lastUsedAt === 0);
  }

  getThumbnailPaths(): string[] {
    return this.data.mediaFiles
      .map((m) => m.thumbnailPath)
      .filter((p) => p && p.length > 0);
  }

  /**
   * Возвращает медиафайлы, для которых ещё не записан путь к превью.
   * Это файлы, не обработанные из-за остановки приложения
   * или недавно добавленные сканированием.
   * Файлы, превысившие лимит неудачных попыток, не включаются.
   */
  getMediaWithoutThumbnail(): MediaFile[] {
    return this.data.mediaFiles.filter(
      (m) => !m.thumbnailPath && (m.thumbnailRetries ?? 0) <= 1,
    );
  }

  /**
   * Фиксирует неудачную попытку генерации превью.
   * Возвращает true, если файл ещё можно ретраить (не превышен лимит).
   */
  incrementThumbnailRetries(mediaId: string): boolean {
    const file = this.data.mediaFiles.find((m) => m.id === mediaId);
    if (!file) {
      return false;
    }
    file.thumbnailRetries = (file.thumbnailRetries ?? 0) + 1;
    this.save();
    return file.thumbnailRetries <= 1;
  }
}