import { autorun, makeAutoObservable, runInAction } from 'mobx';
import type {
  Catalog,
  CatalogStats,
  FilterCondition,
  MediaFile,
  MetaTagSearchResult,
  ScanResult,
  TagSearchResult,
} from '../../shared/types';
import type { MediaFilters } from '../../shared/ipc';
import { UNTAGGED_META_TAG_ID, isMetaTagId } from '../../shared/metaTags';

export interface Toast {
  id: number;
  message: string;
  type: 'info' | 'success' | 'error';
}

export class AppStore {
  catalogs: Catalog[] = [];
  catalogStats: CatalogStats[] = [];
  tags: TagSearchResult[] = [];
  metaTags: MetaTagSearchResult[] = [];
  filter: FilterCondition = { tagIds: [], mode: 'OR' };
  mediaItems: MediaFile[] = [];
  mediaTotal = 0;
  isLoadingMedia = false;
  toasts: Toast[] = [];
  selectedMediaIds: string[] = [];

  constructor() {
    // deep: false — поля хранятся как обычные объекты/массивы (не Proxy).
    // Все обновления заменяют ссылки целиком, так что реактивность сохраняется,
    // а объекты можно безопасно передавать в IPC (structured clone) без toJS().
    // autoBind: true — методы привязаны к экземпляру, поэтому их можно безопасно
    // деструктурировать из стора (например, setSelectedMediaIds, clearSelection).
    makeAutoObservable(this, {}, { deep: false, autoBind: true });

    // При изменении фильтра автоматически перезагружаем список медиа —
    // аналог прежнего useEffect(() => { loadMedia() }, [filter]) в AppContext.
    // Первый запуск (при создании стора) выполняет первичную загрузку.
    autorun(() => {
      void this.loadMedia();
    });

    this.init();
  }

  setSelectedMediaIds(ids: string[] | ((prev: string[]) => string[])): void {
    if (typeof ids === 'function') {
      this.selectedMediaIds = (ids as (prev: string[]) => string[])(this.selectedMediaIds);
    } else {
      this.selectedMediaIds = ids;
    }
  }

  clearSelection(): void {
    this.selectedMediaIds = [];
  }

  showToast(message: string, type: 'info' | 'success' | 'error' = 'info'): void {
    const id = Date.now() + Math.random();
    // Иммутабельно: при deep: false мутации push не отслеживаются MobX
    this.toasts = [...this.toasts, { id, message, type }];
    setTimeout(() => {
      this.dismissToast(id);
    }, 4000);
  }

  dismissToast(id: number): void {
    this.toasts = this.toasts.filter((t) => t.id !== id);
  }

  async loadCatalogs(): Promise<void> {
    const items = await window.api.getCatalogs();
    runInAction(() => {
      this.catalogs = items;
    });
  }

  async loadCatalogStats(): Promise<void> {
    const stats = await window.api.getCatalogStats();
    runInAction(() => {
      this.catalogStats = stats;
    });
  }

  async loadTags(): Promise<void> {
    const items = await window.api.getTags();
    runInAction(() => {
      this.tags = items;
    });
  }

  async loadMetaTags(): Promise<void> {
    const items = await window.api.getMetaTags();
    runInAction(() => {
      this.metaTags = items;
    });
  }

  async loadMedia(): Promise<void> {
    this.isLoadingMedia = true;
    try {
      const filters: MediaFilters = { filter: this.filter };
      const result = await window.api.getMedia(filters);
      runInAction(() => {
        this.mediaItems = result.items;
        this.mediaTotal = result.total;
      });
    } finally {
      this.isLoadingMedia = false;
    }
  }

  async runScan(): Promise<void> {
    try {
      // Результат сканирования приходит через событие onScanComplete,
      // где показывается тост — здесь показывать не нужно, чтобы не дублировать
      await window.api.startScan();
    } catch (error) {
      console.error('Ошибка сканирования:', error);
      this.showToast('Ошибка сканирования каталогов', 'error');
    }
    await this.loadCatalogs();
    await this.loadCatalogStats();
    await this.loadMetaTags();
    await this.loadMedia();
  }

  async exportData(): Promise<void> {
    try {
      await window.api.exportData();
      this.showToast('Данные экспортированы', 'success');
    } catch (error) {
      console.error('Ошибка экспорта данных:', error);
      this.showToast('Ошибка экспорта данных', 'error');
    }
  }

  async importData(): Promise<void> {
    try {
      const result = await window.api.importData();
      if (result === null) {
        return;
      }
      await this.loadCatalogs();
      await this.loadCatalogStats();
      await this.loadTags();
      await this.loadMetaTags();
      await this.loadMedia();
      this.showToast('Данные импортированы', 'success');
    } catch (error) {
      console.error('Ошибка импорта данных:', error);
      this.showToast('Ошибка импорта данных', 'error');
    }
  }

  async addCatalog(): Promise<void> {
    await window.api.addCatalog();
    await this.loadCatalogs();
    await this.loadCatalogStats();
    await this.runScan();
  }

  async removeCatalog(catalogId: string): Promise<void> {
    await window.api.removeCatalog(catalogId);
    await this.loadCatalogs();
    await this.loadCatalogStats();
    await this.loadMedia();
  }

  setFilter(filter: FilterCondition): void {
    this.filter = filter;
  }

  addTagToFilter(tagId: string): void {
    if (this.filter.tagIds.includes(tagId)) {
      return;
    }

    // Если добавляем метатег «без тега» — убираем из фильтра все обычные теги
    if (tagId === UNTAGGED_META_TAG_ID) {
      this.filter = {
        ...this.filter,
        tagIds: [...this.filter.tagIds.filter((id) => isMetaTagId(id)), tagId],
      };
      return;
    }

    // Если добавляем обычный тег (или другой метатег) — убираем метатег «без тега»
    if (this.filter.tagIds.includes(UNTAGGED_META_TAG_ID)) {
      this.filter = {
        ...this.filter,
        tagIds: [...this.filter.tagIds.filter((id) => id !== UNTAGGED_META_TAG_ID), tagId],
      };
      return;
    }

    this.filter = { ...this.filter, tagIds: [...this.filter.tagIds, tagId] };
  }

  removeTagFromFilter(tagId: string): void {
    this.filter = {
      ...this.filter,
      tagIds: this.filter.tagIds.filter((id) => id !== tagId),
    };
  }

  setFilterMode(mode: 'AND' | 'OR'): void {
    this.filter = { ...this.filter, mode };
  }

  async applyTagToMedia(mediaIds: string[], tagId: string): Promise<void> {
    try {
      await window.api.applyTagToMedia(mediaIds, tagId);
      await this.loadTags();
    } catch (error) {
      console.error('Ошибка применения тега:', error);
      this.showToast('Ошибка применения тега', 'error');
    }
  }

  private init(): void {
    // Подписка на событие готовности превью
    window.api.onThumbnailReady(({ mediaId, thumbnailPath }) => {
      runInAction(() => {
        this.mediaItems = this.mediaItems.map((m) =>
          m.id === mediaId ? { ...m, thumbnailPath } : m,
        );
      });
    });

    // Подписка на события сканирования
    window.api.onScanComplete((result: ScanResult) => {
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
        this.showToast(messages.join(', '), 'success');
      }
      void this.loadCatalogs();
      void this.loadCatalogStats();
      void this.loadMetaTags();
      void this.loadMedia();
    });

    // Первичная загрузка (медиа загружается через autorun при создании стора)
    void this.loadCatalogs();
    void this.loadCatalogStats();
    void this.loadTags();
    void this.loadMetaTags();
  }
}

export const appStore = new AppStore();

export function useApp(): AppStore {
  return appStore;
}