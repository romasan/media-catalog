import fs from 'fs';
import path from 'path';
import { randomUUID } from 'crypto';
import ffmpegStatic from 'ffmpeg-static';
import { spawn } from 'child_process';
import type { MediaFile, ThumbnailProgress } from '../shared/types';

const SUPPORTED_FORMATS = new Set([
  '.jpg', '.jpeg', '.png', '.gif', '.webp', '.bmp', '.tiff', '.tif', '.heic',
  '.mp4', '.mov', '.avi', '.mkv', '.webm', '.m4v',
]);

export function isSupportedFile(filePath: string): boolean {
  const ext = path.extname(filePath).toLowerCase();
  return SUPPORTED_FORMATS.has(ext);
}

export function isVideoFile(filePath: string): boolean {
  const ext = path.extname(filePath).toLowerCase();
  return ['.mp4', '.mov', '.avi', '.mkv', '.webm', '.m4v'].includes(ext);
}

export function getFileType(filePath: string): 'photo' | 'video' {
  return isVideoFile(filePath) ? 'video' : 'photo';
}

function getFileCreationDate(filePath: string): number {
  try {
    const stats = fs.statSync(filePath);
    // Используем birthtime если оно валидное, иначе mtime
    const birthtime = stats.birthtime.getTime();
    return birthtime > 0 ? birthtime : stats.mtime.getTime();
  } catch {
    return Date.now();
  }
}

export class ThumbnailGenerator {
  private thumbnailsDir: string;
  private queue: Array<{ media: MediaFile; filePath: string }> = [];
  private processing = false;
  private totalQueued = 0;
  private processedCount = 0;
  private onProgress?: (progress: ThumbnailProgress) => void;
  private onThumbnailReady?: (media: MediaFile, thumbnailPath: string) => void;
  private progressTimer: NodeJS.Timeout | null = null;

  constructor(
    thumbnailsDir: string,
    onProgress?: (progress: ThumbnailProgress) => void,
    onThumbnailReady?: (media: MediaFile, thumbnailPath: string) => void,
  ) {
    this.thumbnailsDir = thumbnailsDir;
    this.onProgress = onProgress;
    this.onThumbnailReady = onThumbnailReady;
    fs.mkdirSync(this.thumbnailsDir, { recursive: true });
  }

  queueThumbnails(files: MediaFile[]): void {
    let addedCount = 0;
    for (const file of files) {
      if (!file.thumbnailPath) {
        this.queue.push({ media: file, filePath: file.path });
        addedCount++;
      }
    }
    if (addedCount === 0) {
      return;
    }
    if (!this.processing) {
      // Сбрасываем счётчики только когда нет активной обработки
      this.totalQueued = this.queue.length;
      this.processedCount = 0;
    } else {
      // Если идёт обработка — добавляем новые к общему количеству
      this.totalQueued += addedCount;
    }
    this.emitProgress();
    this.processQueue();
  }

  private emitProgress(): void {
    if (!this.onProgress) {
      return;
    }
    this.onProgress({
      total: this.totalQueued,
      processed: this.processedCount,
    });
  }

  private emitProgressThrottled(): void {
    if (!this.onProgress || this.progressTimer) {
      return;
    }
    // Троттлинг: отправляем не чаще 1 раза в 200мс
    this.progressTimer = setTimeout(() => {
      this.progressTimer = null;
      this.emitProgress();
    }, 200);
  }

  private processQueue(): void {
    if (this.processing || this.queue.length === 0) {
      if (this.queue.length === 0 && !this.processing && this.totalQueued > 0) {
        // Очередь опустела — отправляем финальный прогресс и сбрасываем
        this.totalQueued = 0;
        this.processedCount = 0;
        this.emitProgress();
      }
      return;
    }
    this.processing = true;

    const next = this.queue.shift();
    if (!next) {
      this.processing = false;
      return;
    }

    const current = next;
    this.generateThumbnail(current.media)
      .then((thumbnailPath) => {
        // Сообщаем о готовности превью (например, для обновления БД)
        this.onThumbnailReady?.(current.media, thumbnailPath);
      })
      .catch((error) => {
        console.error(`Ошибка генерации превью для ${current.media.path}:`, error);
      })
      .finally(() => {
        this.processing = false;
        this.processedCount++;
        this.emitProgressThrottled();
        this.processQueue();
      });
  }

  private getThumbnailCachePath(filePath: string): string {
    // Имя превью: хэш пути файла, чтобы превью не конфликтовали
    const hash = this.hashString(filePath);
    const ext = path.extname(filePath).toLowerCase();
    return path.join(this.thumbnailsDir, `${hash}${ext}.jpg`);
  }

  private hashString(input: string): string {
    let hash = 0;
    for (let i = 0; i < input.length; i++) {
      const char = input.charCodeAt(i);
      hash = ((hash << 5) - hash) + char;
      hash |= 0;
    }
    return Math.abs(hash).toString(36);
  }

  private async generateThumbnail(media: MediaFile): Promise<string> {
    const cachePath = this.getThumbnailCachePath(media.path);

    // Если превью уже существует — используем его
    if (fs.existsSync(cachePath)) {
      return cachePath;
    }

    // Проверяем, существует ли исходный файл
    if (!fs.existsSync(media.path)) {
      throw new Error(`Файл не найден: ${media.path}`);
    }

    if (media.type === 'video') {
      await this.generateVideoThumbnail(media.path, cachePath);
    } else {
      await this.generatePhotoThumbnail(media.path, cachePath);
    }

    return cachePath;
  }

  private generateViaFfmpeg(inputPath: string, outputPath: string, extraArgs: string[]): Promise<void> {
    return new Promise((resolve, reject) => {
      // В production-сборке бинарник ffmpeg распаковывается в app.asar.unpacked
      const ffmpegPath = ffmpegStatic?.replace('app.asar', 'app.asar.unpacked');
      if (!ffmpegPath) {
        reject(new Error('ffmpeg-static не найден'));
        return;
      }

      const args = [
        '-hide_banner',
        '-loglevel', 'error',
        '-y',
        '-i', inputPath,
        ...extraArgs,
        '-vf', 'scale=400:400:force_original_aspect_ratio=increase,crop=400:400',
        '-frames:v', '1',
        outputPath,
      ];

      const child = spawn(ffmpegPath, args, { stdio: 'ignore' });
      child.on('error', (error) => {
        reject(new Error(`Ошибка ffmpeg: ${error.message}`));
      });
      child.on('close', (code) => {
        if (code === 0) {
          resolve();
        } else {
          reject(new Error(`ffmpeg завершился с кодом ${code}`));
        }
      });
    });
  }

  private async generateVideoThumbnail(inputPath: string, outputPath: string): Promise<void> {
    // Извлекаем кадр на 1-й секунде
    await this.generateViaFfmpeg(inputPath, outputPath, ['-ss', '0.5']);
  }

  private async generatePhotoThumbnail(inputPath: string, outputPath: string): Promise<void> {
    await this.generateViaFfmpeg(inputPath, outputPath, []);
  }

  /**
   * Удаляет превью-файлы для указанных медиафайлов.
   * Удаляет и по сохранённому thumbnailPath, и по вычисленному хэшу пути,
   * чтобы гарантированно удалить превью даже если thumbnailPath не был сохранён в БД.
   */
  deleteThumbnailsForFiles(files: MediaFile[]): void {
    for (const media of files) {
      // 1. Удаляем по сохранённому пути (если есть)
      if (media.thumbnailPath && fs.existsSync(media.thumbnailPath)) {
        try {
          fs.unlinkSync(media.thumbnailPath);
        } catch {
          // Игнорируем ошибку
        }
      }
      // 2. Удаляем по вычисленному хэшу пути (превью, которые генерировались ранее)
      const cachePath = this.getThumbnailCachePath(media.path);
      if (fs.existsSync(cachePath)) {
        try {
          fs.unlinkSync(cachePath);
        } catch {
          // Игнорируем ошибку
        }
      }
    }
  }
}