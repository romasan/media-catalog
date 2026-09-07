import { observer } from 'mobx-react-lite';
import React, { useState } from 'react';
import { CatalogManagerPopup } from '../CatalogManagerPopup/CatalogManagerPopup';
import { TagManagerPopup } from '../TagManagerPopup/TagManagerPopup';
import { useApp } from '../../store/AppStore';
import styles from './BurgerMenu.module.css';

type ActivePopup = 'catalogs' | 'tags' | null;

export const BurgerMenu = observer(function BurgerMenu(): React.ReactElement {
  const { exportData, importData } = useApp();
  const [isOpen, setIsOpen] = useState(false);
  const [activePopup, setActivePopup] = useState<ActivePopup>(null);

  const handleMenuItemClick = (popup: Exclude<ActivePopup, null>) => {
    setActivePopup(popup);
    setIsOpen(false);
  };

  const closePopup = () => {
    setActivePopup(null);
  };

  const handleExport = () => {
    setIsOpen(false);
    exportData();
  };

  const handleImport = () => {
    setIsOpen(false);
    importData();
  };

  return (
    <>
      <div className={styles['burger-menu']}>
        <button
          className={styles['burger-button']}
          onClick={() => setIsOpen(!isOpen)}
          title="Меню"
        >
          <svg viewBox="0 0 24 24" width="24" height="24" fill="currentColor">
            <rect x="3" y="5" width="18" height="2" rx="1" />
            <rect x="3" y="11" width="18" height="2" rx="1" />
            <rect x="3" y="17" width="18" height="2" rx="1" />
          </svg>
        </button>
      </div>

      {isOpen && (
        <>
          <div
            className={styles['menu-backdrop']}
            onClick={() => setIsOpen(false)}
          />
          <div className={styles['burger-menu-popup']}>
            <button
              className={styles['burger-menu-item']}
              onClick={() => handleMenuItemClick('catalogs')}
            >
              📁 Управление каталогами
            </button>
            <button
              className={styles['burger-menu-item']}
              onClick={() => handleMenuItemClick('tags')}
            >
              🏷️ Управление тегами
            </button>
            <div className={styles['burger-menu-separator']} />
            <button className={styles['burger-menu-item']} onClick={handleExport}>
              💾 Экспорт
            </button>
            <button className={styles['burger-menu-item']} onClick={handleImport}>
              📥 Импорт
            </button>
          </div>
        </>
      )}

      {activePopup === 'catalogs' && (
        <CatalogManagerPopup onClose={closePopup} />
      )}
      {activePopup === 'tags' && (
        <TagManagerPopup onClose={closePopup} />
      )}
    </>
  );
});
