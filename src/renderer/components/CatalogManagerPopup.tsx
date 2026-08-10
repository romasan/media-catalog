import { observer } from 'mobx-react-lite';
import React from 'react';
import { useApp } from '../store/AppStore';
import { DraggableResizable } from './DraggableResizable';

interface CatalogManagerPopupProps {
  onClose: () => void;
}

export const CatalogManagerPopup = observer(function CatalogManagerPopup({ onClose }: CatalogManagerPopupProps): React.ReactElement {
  const { catalogs, catalogStats, addCatalog, removeCatalog, runScan } = useApp();

  const handleAddCatalog = async () => {
    await addCatalog();
  };

  const handleRemoveCatalog = async (catalogId: string) => {
    await removeCatalog(catalogId);
    await runScan();
  };

  return (
    <DraggableResizable
      title="Управление каталогами"
      onClose={onClose}
      defaultWidth={520}
      defaultHeight={400}
      minWidth={400}
      minHeight={300}
    >
      <div className="popup-content catalog-manager">
        <button className="add-catalog-button" onClick={handleAddCatalog}>
          <span className="add-icon">+</span>
          Добавить папку для сканирования
        </button>

        <div className="catalog-list">
          {catalogs.length === 0 && (
            <div className="catalog-empty">
              Нет добавленных каталогов. Нажмите «Добавить папку», чтобы начать.
            </div>
          )}
          {catalogs.map((catalog) => {
            const stats = catalogStats.find((s) => s.catalogId === catalog.id);
            return (
              <div className="catalog-item" key={catalog.id}>
                <div className="catalog-info">
                  <div className="catalog-path">{catalog.path}</div>
                  <div className="catalog-counts">
                    {stats ? (
                      <>
                        <span>📷 {stats.photoCount}</span>
                        <span>🎬 {stats.videoCount}</span>
                      </>
                    ) : (
                      <span>—</span>
                    )}
                  </div>
                </div>
                <button
                  className="icon-button delete-button"
                  onClick={() => handleRemoveCatalog(catalog.id)}
                  title="Удалить каталог"
                >
                  <svg viewBox="0 0 24 24" width="16" height="16" fill="none" stroke="currentColor" strokeWidth="2">
                    <path d="M3 6h18M8 6V4a2 2 0 0 1 2-2h4a2 2 0 0 1 2 2v2m3 0v14a2 2 0 0 1-2 2H7a2 2 0 0 1-2-2V6h14z" />
                    <line x1="10" y1="11" x2="10" y2="17" />
                    <line x1="14" y1="11" x2="14" y2="17" />
                  </svg>
                </button>
              </div>
            );
          })}
        </div>
      </div>
    </DraggableResizable>
  );
});
