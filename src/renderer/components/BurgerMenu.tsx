import React, { useState } from 'react';
import { CatalogManagerPopup } from './CatalogManagerPopup';
import { TagManagerPopup } from './TagManagerPopup';

type ActivePopup = 'catalogs' | 'tags' | null;

export function BurgerMenu(): React.ReactElement {
  const [isOpen, setIsOpen] = useState(false);
  const [activePopup, setActivePopup] = useState<ActivePopup>(null);

  const handleMenuItemClick = (popup: Exclude<ActivePopup, null>) => {
    setActivePopup(popup);
    setIsOpen(false);
  };

  const closePopup = () => {
    setActivePopup(null);
  };

  return (
    <>
      <div className="burger-menu">
        <button
          className="burger-button"
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
            className="menu-backdrop"
            onClick={() => setIsOpen(false)}
          />
          <div className="burger-menu-popup">
            <button
              className="burger-menu-item"
              onClick={() => handleMenuItemClick('catalogs')}
            >
              📁 Управление каталогами
            </button>
            <button
              className="burger-menu-item"
              onClick={() => handleMenuItemClick('tags')}
            >
              🏷️ Управление тегами
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
}