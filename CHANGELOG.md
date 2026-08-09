# Changelog

## 2026-08-09

Initial implementation of Media Catalog — an Electron + React + TypeScript app for cataloging photos and videos with tags, folder scanning and ffmpeg thumbnails.

## 2026-08-09 — Responsive media grid

- Media cards now fill the full window width with sizes in the 100–200 px range.

## 2026-08-09 — Export/Import menu items
- Added "Export" and "Import" items to the burger menu, separated from catalog/tag management.
- Export calls existing `window.api.exportData()` (JSON save dialog); import calls `window.api.importData()` (open dialog) and reloads catalogs, stats, tags and media.
- `AppContext` now exposes `exportData`/`importData` actions with success/error toasts.

## 2026-08-09 — Fix Windows build

- Fixed `npm run dist:win` crashing with `ERR_REQUIRE_ESM` on Windows: `app-builder-lib` (26.15.x) requires `@noble/hashes/blake2.js` via CommonJS `require()`, but version 2.x of `@noble/hashes` is ESM-only.
- Added an `overrides` entry pinning `@noble/hashes` to `1.8.0` — the last 1.x release that provides `blake2.js` with CommonJS support.
- `npm start` continues to work as before.
