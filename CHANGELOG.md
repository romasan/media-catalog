# Changelog

## 2026-08-10 — Show total tag count in tag manager
- The tag manager popup now shows the total number of tags («Всего тегов: N») below the tag list.

## 2026-08-10 — Tag names are normalized to lowercase
- Tag names are now stored in lowercase: `Database.createTag` lowercases the trimmed name, so «foo», «Foo» and «fOo» are treated as the same tag.
- On database load, existing tags are normalized to lowercase and duplicate tags differing only in case are merged (their `mediaTags` relations are redirected to the surviving tag).
- `BatchTagBar` now finds the newly created tag case-insensitively; docs (`WORKFLOW`, `ARCHITECTURE`, `DATA`) updated accordingly.

## 2026-08-10 — Move filter bar to bottom-left
- The filter bar (selected tags + «все»/«любой» toggle) now appears in the bottom-left corner of the window instead of the top-left.

## 2026-08-10 — Rename filter toggle labels
- Filter toggle labels in FilterBar: AND → «все», OR → «любой»; docs (README, WORKFLOW, ARCHITECTURE) updated accordingly.

## 2026-08-10 — Confirm tag deletion in tag manager
- Deleting a tag that is assigned to at least one file now shows a modal confirmation dialog (tag name + file count) with «Удалить»/«Отмена» buttons.
- The dialog is rendered via a portal into `document.body`, so it always appears above the popup regardless of list scroll; it can be closed with «Отмена», a click on the backdrop, or Esc.
- Tags with no files are still deleted immediately.

## 2026-08-10 — Fix season periods in docs
- Fixed season date ranges in `docs/DATA.md` and `docs/WORKFLOW.md`: spring is Mar 1 – May 31, summer is Jun 1 – Aug 31 (matching `src/shared/metaTags.ts`).

## 2026-08-10 — Meta tags above tag search
- Moved the meta-tag chips (years, seasons, file types, «без тега») in the tag manager popup above the tag search input, so they are visible without scrolling.
- Tag list still grows below the search row; popup layout and drag/resize behavior are unchanged.

## 2026-08-10 — "Без тега" meta tag
- Added a special `untagged` meta tag (`meta:untagged`, shown as «без тега») in the "Прочее" group of the tag manager.
- The tag matches only files that have no regular (non-meta) tags, allowing the filter to show all untagged files at once.
- When «без тега» is added to the filter, any regular tags are removed from the filter; conversely, adding a regular tag removes «без тега» from the filter.
- `Database.getTaggedMediaIds()` and `UNTAGGED_META_TAG_ID` support the new meta tag in both filtering and count display.

## 2026-08-10 — Batch tagging
- Added multi-select in the media grid: hold `Ctrl`/`Cmd` and click cards to toggle selection; hold `Shift` and click another card to select the whole range between.
- When at least one card is selected, a batch-tag input bar appears at the bottom center of the screen.
- Typing in the bar shows tag suggestions (like in fullscreen tag-creation preview); navigate with ArrowUp/ArrowDown and confirm with Enter, or click a suggestion.
- Pressing Enter with no matching suggestion creates a new tag and applies it to all selected files at once.
- `window.api.applyTagToMedia(mediaIds, tagId)` batch IPC: applies a tag to multiple media files in a single transaction.
- Added auto-computed meta tags (year, season, file type) that don't persist, export or can be deleted; shown in gray, filterable, grouped in TagManagerPopup.

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
