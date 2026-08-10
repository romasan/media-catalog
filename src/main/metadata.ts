import fs from 'fs';
import ffmpegStatic from 'ffmpeg-static';
import { spawn } from 'child_process';
import exifr from 'exifr';
import { isVideoFile } from './thumbnails';

/**
 * Пытается извлечь дату съёмки из метаданных файла.
 * Для фото — EXIF (DateTimeOriginal), для видео — creation_time контейнера.
 * Возвращает timestamp (мс), либо null, если дату определить не удалось.
 */
export async function readCaptureDate(filePath: string, fileType: 'photo' | 'video'): Promise<number | null> {
  try {
    if (!fs.existsSync(filePath)) {
      return null;
    }

    if (fileType === 'video' || isVideoFile(filePath)) {
      return await readVideoCreationTime(filePath);
    }

    return await readPhotoDateTimeOriginal(filePath);
  } catch (error) {
    console.error(`Ошибка чтения метаданных ${filePath}:`, error);
    return null;
  }
}

async function readPhotoDateTimeOriginal(filePath: string): Promise<number | null> {
  try {
    // exifr умеет читать EXIF из JPEG/TIFF/HEIC/WebP и возвращает дату
    // в виде объекта Date (по UTC), либо строку в зависимости от формата.
    const data = await exifr.parse(filePath, ['DateTimeOriginal']);

    if (data && data.DateTimeOriginal) {
      const date = new Date(data.DateTimeOriginal);
      if (!isNaN(date.getTime())) {
        return date.getTime();
      }
    }

    // Запасной вариант: пробуем CreateDate / ModifyDate
    const fallback = await exifr.parse(filePath, ['CreateDate', 'ModifyDate']);
    for (const key of ['CreateDate', 'ModifyDate']) {
      const value = fallback?.[key];
      if (value) {
        const date = new Date(value);
        if (!isNaN(date.getTime())) {
          return date.getTime();
        }
      }
    }

    return null;
  } catch {
    // Не все форматы поддерживаются exifr — молча возвращаем null
    return null;
  }
}

function readVideoCreationTime(filePath: string): Promise<number | null> {
  return new Promise((resolve) => {
    const ffmpegPath = ffmpegStatic?.replace('app.asar', 'app.asar.unpacked');
    if (!ffmpegPath) {
      resolve(null);
      return;
    }

    // Выводим метаданные контейнера в stdout (игнорируя stderr)
    const args = [
      '-hide_banner',
      '-i', filePath,
      '-f', 'null', '-',
    ];

    const child = spawn(ffmpegPath, args, { stdio: ['ignore', 'pipe', 'ignore'] });
    let output = '';

    child.stdout.on('data', (chunk: Buffer) => {
      output += chunk.toString('utf-8');
    });

    child.on('error', () => {
      resolve(null);
    });

    child.on('close', () => {
      const date = parseFfmpegCreationTime(output);
      resolve(date);
    });
  });
}

/**
 * Парсит creation_time из вывода ffmpeg.
 * Поддерживает форматы:
 *   - "creation_time   : 2023-07-15T10:20:30.000000Z" (ISO 8601 UTC)
 *   - "creation_time   : 2023-07-15T10:20:30" (ISO без зоны — трактуем как UTC)
 *   - "creation_time   : 2023-07-15 10:20:30" (локальный формат — трактуем как UTC)
 */
function parseFfmpegCreationTime(output: string): number | null {
  const match = output.match(/creation_time\s*:\s*([^\r\n]+)/i);
  if (!match) {
    return null;
  }

  const raw = match[1].trim();
  if (!raw) {
    return null;
  }

  // ISO 8601 с Z или с часовым поясом — парсим как есть
  if (/^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}(\.\d+)?(Z|[+-]\d{2}:\d{2})?$/i.test(raw)) {
    const date = new Date(raw);
    if (!isNaN(date.getTime())) {
      return date.getTime();
    }
  }

  // Формат "YYYY-MM-DD HH:MM:SS" — трактуем как UTC
  const spaceMatch = raw.match(/^(\d{4})-(\d{2})-(\d{2})\s+(\d{2}):(\d{2}):(\d{2})/);
  if (spaceMatch) {
    const [, year, month, day, hour, minute, second] = spaceMatch.map(Number);
    const date = new Date(Date.UTC(year, month - 1, day, hour, minute, second));
    if (!isNaN(date.getTime())) {
      return date.getTime();
    }
  }

  return null;
}