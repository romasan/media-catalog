import { app, BrowserWindow, dialog, ipcMain, net, protocol, shell } from 'electron';
import path from 'path';
import fs from 'fs';
import { pathToFileURL } from 'url';
import { IPC } from '../shared/ipc';
import type { ScanResult } from '../shared/types';
import { Database } from './database';
import { CatalogScanner } from './scanner';
import { ThumbnailGenerator } from './thumbnails';
import { registerIpcHandlers } from './ipc-handlers';

const isDev = process.env.NODE_ENV === 'development';

// Регистрируем протокол media-stream как привилегированный до готовности приложения
protocol.registerSchemesAsPrivileged([
  {
    scheme: 'media-stream',
    privileges: {
      standard: true,
      secure: true,
      supportFetchAPI: true,
      stream: true,
    },
  },
]);

let mainWindow: BrowserWindow | null = null;
let database: Database;
let scanner: CatalogScanner;
let thumbnailGenerator: ThumbnailGenerator;
let isScanning = false;
let pendingScan = false;
let lastScanResult: ScanResult | null = null;

function getDataDir(): string {
  // Храним данные в userData (в dev — рядом с проектом, в production — в папке пользователя)
  const dataDir = path.join(app.getPath('userData'), 'media-catalog-data');
  fs.mkdirSync(dataDir, { recursive: true });
  return dataDir;
}

function getDatabasePath(): string {
  return path.join(getDataDir(), 'catalog.db');
}

function getThumbnailsDir(): string {
  const dir = path.join(getDataDir(), 'thumbnails');
  fs.mkdirSync(dir, { recursive: true });
  return dir;
}

function createWindow(): void {
  mainWindow = new BrowserWindow({
    width: 1200,
    height: 800,
    minWidth: 800,
    minHeight: 600,
    title: 'Медиа Каталог',
    icon: path.join(__dirname, '../../build/icons/icon-512.png'),
    autoHideMenuBar: true,
    backgroundColor: '#1a1a1a',
    webPreferences: {
      preload: path.join(__dirname, '../preload/index.js'),
      contextIsolation: true,
      nodeIntegration: false,
      sandbox: false,
    },
  });

  const devServer = process.env.VITE_DEV_SERVER_URL;
  if (isDev && devServer) {
    mainWindow.loadURL(devServer);
    mainWindow.webContents.openDevTools({ mode: 'detach' });
  } else {
    mainWindow.loadFile(path.join(__dirname, '../renderer/index.html'));
  }

  // Открывать внешние ссылки в браузере
  mainWindow.webContents.setWindowOpenHandler(({ url }) => {
    shell.openExternal(url);
    return { action: 'deny' };
  });

  mainWindow.on('closed', () => {
    mainWindow = null;
  });
}

async function runScan(): Promise<ScanResult> {
  if (isScanning) {
    pendingScan = true;
    return lastScanResult || {
      addedFiles: 0,
      removedFiles: 0,
      addedFolders: 0,
      removedFolders: 0,
      addedMedia: [],
      removedMedia: [],
    };
  }

  isScanning = true;
  try {
    do {
      pendingScan = false;
      const result: ScanResult = await scanner.scan();
      lastScanResult = result;
      if (mainWindow && !mainWindow.isDestroyed()) {
        mainWindow.webContents.send(IPC.OnScanComplete, result);
      }
    } while (pendingScan);
    return lastScanResult!;
  } finally {
    isScanning = false;
  }
}

app.whenReady().then(() => {
  const dataDir = getDataDir();
  const thumbnailsDir = getThumbnailsDir();

  database = new Database(getDatabasePath());
  thumbnailGenerator = new ThumbnailGenerator(
    thumbnailsDir,
    (progress) => {
      if (mainWindow && !mainWindow.isDestroyed()) {
        mainWindow.webContents.send(IPC.OnThumbnailProgress, progress);
      }
    },
    (media, thumbnailPath) => {
      // Сохраняем путь к превью в базе
      database.updateThumbnail(media.id, thumbnailPath);
      // Уведомляем рендерер, чтобы обновить превью в сетке
      if (mainWindow && !mainWindow.isDestroyed()) {
        mainWindow.webContents.send(IPC.OnThumbnailReady, { mediaId: media.id, thumbnailPath });
      }
    },
    (media) => {
      // Фиксируем неудачную попытку. Максимум 1 ретрай:
      // первая ошибка — сразу одна неудачная попытка, ретрай — вторая.
      const canRetry = database.incrementThumbnailRetries(media.id);
      return canRetry;
    },
  );
  scanner = new CatalogScanner(database, thumbnailGenerator);

  populateMediaTagsAfterImport();

  // Регистрируем протокол media-stream для стриминга медиафайлов
  protocol.handle('media-stream', (request) => {
    try {
      const url = new URL(request.url);
      const encodedPath = url.hostname === 'local' ? decodeURIComponent(url.pathname).slice(1) : '';
      if (encodedPath) {
        const filePath = Buffer.from(encodedPath, 'base64url').toString('utf-8');
        if (fs.existsSync(filePath)) {
          return net.fetch(pathToFileURL(filePath).toString());
        }
      }
    } catch (error) {
      console.error('Ошибка media-stream протокола:', error);
    }
    return new Response('Not Found', { status: 404 });
  });

  registerIpcHandlers({
    ipcMain,
    database,
    scanner,
    thumbnailGenerator,
    readFile: dialog.showOpenDialog,
    writeFileDialog: dialog.showSaveDialog,
    getMainWindow: () => mainWindow,
    runScan,
  });

  createWindow();

  // Восстанавливаем обработку превью, прерванную при прошлом запуске:
  // файлы с пустым thumbnailPath добавляем в очередь генерации.
  // Это позволяет продолжить с того места, где приложение было остановлено,
  // и сразу показать прогрессбар в рендерере.
  // Ждём полной загрузки окна, чтобы рендерер успел подписаться
  // на события прогресса — иначе первое событие будет потеряно.
  mainWindow?.webContents.once('did-finish-load', () => {
    thumbnailGenerator.queueThumbnails(database.getMediaWithoutThumbnail());
  });

  // Фоновое сканирование при старте
  setTimeout(() => {
    runScan();
  }, 1000);

  app.on('activate', () => {
    if (BrowserWindow.getAllWindows().length === 0) {
      createWindow();
    }
  });
});

// Восстановление связей тегов с файлами после импорта данных
function populateMediaTagsAfterImport(): void {
  const pendingMediaTags: Array<{ tagId: string; filePath: string }> = (global as any).__pendingImportMediaTags;
  const pendingFilePaths: string[] = (global as any).__pendingImportFilePaths;

  if (!pendingMediaTags || !pendingFilePaths) {
    return;
  }

  try {
    const pathToId = new Map<string, string>();
    for (const media of database.getMediaFiles()) {
      // Пытаемся сопоставить и по исходному, и по нормализованному пути
      pathToId.set(path.normalize(media.path), media.id);
      pathToId.set(path.resolve(media.path), media.id);
    }

    for (const rel of pendingMediaTags) {
      let mediaId = pathToId.get(path.normalize(rel.filePath));
      if (!mediaId) {
        mediaId = pathToId.get(path.resolve(rel.filePath));
      }
      if (mediaId) {
        database.applyTag(mediaId, rel.tagId);
      }
    }
  } catch (error) {
    console.error('Ошибка восстановления тегов после импорта:', error);
  } finally {
    delete (global as any).__pendingImportMediaTags;
    delete (global as any).__pendingImportFilePaths;
  }
}

app.on('window-all-closed', () => {
  if (process.platform !== 'darwin') {
    app.quit();
  }
});