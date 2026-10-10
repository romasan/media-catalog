# Changelog

## 2026-10-10 — Fix the fullscreen viewer reading the model row
- The viewer asked the model for `get(index)`, which did not exist, so opening a file failed with a QML type error. `MediaListModel::get()` now returns the row as a map keyed by role names (`mediaId`, `fileUrl`, `isVideo`, `sizeText`, …), and a test asserts those keys and the out-of-range behaviour.

## 2026-10-10 — Fix popups not opening and not closing
- Popups never appeared: `ApplicationWindow` accepts a single visual child, and the catalog and tag managers were declared as extra children with `anchors.fill`. Everything now lives under one root item, so the popups open, receive input and close.
- The tag manager opened from the batch tagging bar was a child of that 46 px bar and was clipped by it; the bar now emits a signal and the popup belongs to the window.
- TagManagerPopup gained an `open()` entry point for consistency with the catalog manager.
- Added a `--add-catalog <path>` launch option that adds a catalog and starts scanning without the dialog, useful for scripts and for verifying the core; verified against a real folder of 222 files (222 previews generated, reusing the existing thumbnail cache).

## 2026-10-10 — Fix catalog scanning, popup closing and restore the 1.x interface
- Adding a folder did nothing: Qt dialogs return a URL (`file:///…`) and it was passed straight into the path, so `QFileInfo` resolved it against the working directory (`…/file:/Users/…`), the folder was never found and the scan finished with zero files. Dialogs now convert the URL with `urlToLocalFile`, and `Paths::toLocalPath` accepts both forms as a safety net.
- The burger button is back in the bottom-right corner (a 56 px circle) instead of the window header, and the filter moved from a full-width top bar to a compact panel in the bottom-left, matching the 1.x layout.
- Popups now close: the close cross works, Esc closes them, clicking the dimmed backdrop closes them, and the backdrop also stops clicks from reaching the grid underneath.
- Emoji are gone from the interface: the burger, close cross, play badge, resize corner and the photo/video counter now use vector icons drawn with `Canvas`, which look the same on every platform and follow the theme colour.
- The filter panel matches the 1.x behaviour again: the "все / любой" toggle appears only with two or more tags, chips use the original colours and the counter shows photo and video counts for the current result.

## 2026-10-10 — Give every thread its own SQLite connection
- `QSqlDatabase` was shared between the main thread and the background scan thread, which is not allowed and caused intermittent segfaults inside SQLite when the scan prepared a statement; `CatalogDatabase` now hands out one lazily opened connection per thread via `QThreadStorage`, with WAL allowing concurrent reads and writes.
- Added an explicit `MediaLibrary::shutdown()` (also called from the destructor) that disconnects worker objects, waits for scanning and metadata reads, stops the preview pool while the database is still alive, and discards already queued signals.
- Verified with repeated test runs: 12 consecutive model runs and 6 core runs pass (53 and 107 checks), and the application still scans and generates previews end to end.

## 2026-10-10 — Deploy into a separate directory instead of the build tree
- The `deploy` target now copies the app bundle into `<build>/deploy` before running `macdeployqt` or `windeployqt`, so the development build stays untouched; deploying in place made macOS load two sets of Qt binaries and the app failed to start with "Could not load the Qt platform plugin".
- Added a `deploy-run` target that launches the deployed package (macOS, Linux), and CPack now packages files from the deploy directory.
- `docs/BUILD.md` documents the new layout and the duplicate-Qt symptom with its fix.

## 2026-10-10 — Keep the data directory one level deep
- The default data directory is now built from the OS generic data location plus the application name instead of `QStandardPaths::AppDataLocation`, which also appends the organization name and produced a nested path like `…/Application Support/MediaCatalog/MediaCatalog`.

## 2026-10-10 — Fix duplicate preview generation and catalog statistics for QML
- Unified media path normalization to an absolute path without resolving symbolic links: resolving turned `/tmp` into `/private/tmp` on macOS, so the preview file name (a hash of the path) no longer matched the path the app looked up and every preview was regenerated on each start.
- Added a regression check that a catalog added through a symbolic link produces previews exactly once and that the stored preview path equals the path derived from the file path.
- Exposed `catalogStats` to QML and added the missing `Q_INVOKABLE` markers, so the catalog manager shows photo and video counts instead of QML type errors.

## 2026-10-10 — Native rewrite: C++20 + Qt 6 instead of Electron + React
- The application is rewritten as native software for macOS, Windows and Linux in C++20 with Qt Quick; a single CMake build replaces the Electron, React and MobX stack.
- Data moved from the hand-rolled JSON file to SQLite in WAL mode with a versioned schema, indexes and referential cleanup; export/import still transfers catalogs, tags and links by path and tag name.
- Preview file names reproduce the 1.x path hash exactly, so an existing thumbnail cache is reused instead of regenerated; ffmpeg still runs as a process pool with one process per core.
- Photo capture dates are read by a built-in EXIF parser and video dates by ffprobe, so exifr and ffmpeg-static are no longer dependencies.
- Added 107 core checks and 36 model checks (`ctest`) covering hashing, EXIF, storage, scanning, previews, both filter modes, selection and data transfer.
- Documentation rewritten for the new architecture: `README.md`, `docs/ARCHITECTURE.md`, `docs/DATA.md`, `docs/WORKFLOW.md` and the new `docs/BUILD.md`.

## 2026-09-13 — Show date range of cards in the visible grid area
- Added a `DateRangeLabel` pill in the top-right corner with the date range (`modifiedAt`) of the cards currently visible in the grid; it updates while scrolling.
- Format adapts to the range length: same day `22 апреля 2025`, same month `1 - 9 мая 2025`, same year `1 июня - 30 июля 2025`, different years `20 декабря 2025 - 7 января 2026`.
- `MediaGrid` now also tracks the actual on-screen index range (without overscan); `ThumbnailProgressBar` moved to `top: 56px` so it stacks below the label.
- Docs (`WORKFLOW`, `ARCHITECTURE`, `README`) updated accordingly.

## 2026-09-07 — Show photo/video counts in the filter bar
- Filter bar is now always visible and shows a photo/video counter (📷 N 🎬 M) to the right of the selected tags; it reflects the active filter, or the total records when the filter is empty.

## 2026-09-07 — Put each component into its own folder under `src/renderer/components/<Name>/`
- Moved each `<Name>.tsx` + `<Name>.module.css` pair into a dedicated `components/<Name>/` subfolder; imports in `App.tsx` and cross-component imports updated.

## 2026-09-07 — Move renderer styles to per-component CSS Modules
- Replaced the single monolithic `src/renderer/styles.css` (imported once in `App.tsx`) with per-component **CSS Modules** plus a plain global stylesheet.
- Global/base styles (reset, `html/body/#root`, `body`, `button`, `.app`, scrollbar) moved to `src/renderer/App.css`, imported from `App.tsx`.
- Each component now owns a sibling `<Name>.module.css` imported directly from the component:
  `FilterBar`, `MediaGrid`, `BurgerMenu`, `BatchTagBar`, `FullscreenViewer`, `ToastContainer`, `ThumbnailProgressBar`, `DraggableResizable`, `CatalogManagerPopup`, `TagManagerPopup`.
- All `className` usages were converted to locally scoped `styles.*` references (literal, combined and conditional/template class strings). Dead/unused CSS classes were dropped; the DOM/class structure is otherwise unchanged.
- Deleted `src/renderer/styles.css`.
- `README.md` (project structure) updated accordingly.

## 2026-08-10 — Replace React context store with MobX
- Replaced `AppContext.tsx` (React Context + reducer) with a MobX store `src/renderer/store/AppStore.ts`.
- `AppStore` is a singleton (`makeAutoObservable`) exposing reactive state (catalogs, tags, meta tags, filter, media items, selection, toasts) and actions (loaders, filters, selection, export/import, toasts).
- Components that read the store are wrapped in `observer()` from `mobx-react-lite`; they re-render only when the observed state actually changes.
- `useApp()` hook now returns the single `appStore` instance instead of reading a Context.
- Deleted `AppContext.tsx`; all imports updated from `store/AppContext` to `store/AppStore`.
- Added `mobx` and `mobx-react-lite` dependencies.
- Docs (`ARCHITECTURE`) updated accordingly.

## 2026-08-10 — Show capture date from metadata in fullscreen viewer
- The fullscreen viewer now lazily reads the capture date from file metadata (EXIF `DateTimeOriginal`/`CreateDate` for photos via exifr, `creation_time` for videos via ffmpeg) and shows it in the file info when it differs from the file modification date.
- The capture date is cached in the database as the optional `capturedAt` field on `MediaFile` (new IPC channel `GetMediaCaptureDate` / `media:capture-date:get`).

## 2026-08-10 — App icon from favicon.ico
- Replaced the default app icon with `/Users/r.bauer/Downloads/favicon.ico` (scaled via nearest-neighbor, no blur) for the window, Windows installer (.ico), macOS bundle (.icns) and Linux (AppImage).

## 2026-08-10 — Use modification date for year/season meta tags
- Year and season meta tags are now computed from the file modification date (`modifiedAt`) instead of creation date (`createdAt`), so editing a file moves it to the correct year/season in filters.
- Media list is now sorted by `modifiedAt` (newest first) instead of `createdAt` in `ipc-handlers.ts`.
- Fullscreen preview now shows the modification date (`modifiedAt`) as the displayed date.
- Docs (`WORKFLOW`, `ARCHITECTURE`, `DATA`) updated accordingly.

## 2026-08-10 — Native context menu in fullscreen preview
- Right-clicking on the media file in the fullscreen preview now opens a native system context menu with a single item «Открыть в проводнике».
- Clicking the item reveals the file in the OS file manager: `shell.showItemInFolder` works on both Windows (Explorer) and macOS (Finder) by highlighting the file.
- The menu is rendered as a native Electron `Menu` popup at the cursor position (built in the main process), so it looks and behaves like the OS-native context menu.
- New IPC channel `ShowItemInFolder` (`media:show-in-folder`) with `ShowItemInFolderRequest` (file path + cursor x/y).
- The context menu is triggered only when right-clicking on the media itself (image/video), not on empty areas of the preview.

## 2026-08-10 — Parallel thumbnail generation
- Thumbnail queue is now processed in parallel: ffmpeg processes run simultaneously, one per logical CPU core (`os.availableParallelism()`, fallback `os.cpus().length`).
- Each ffmpeg process runs with `-threads 1`, so parallelism comes from a pool of single-threaded processes occupying one core each — all cores are utilized without overloading the system.
- Docs (`WORKFLOW`, `ARCHITECTURE`, `DATA`) updated accordingly.

## 2026-08-10 — Resume thumbnail generation after restart
- If the app is stopped while thumbnails are being generated, the next launch resumes processing: all files with an empty `thumbnailPath` are re-queued on `did-finish-load` (`database.getMediaWithoutThumbnail()`), and the progress bar immediately shows the remaining files.
- `Database.getMediaWithoutThumbnail()` returns all media files whose `thumbnailPath` is empty.
- `ThumbnailGenerator.queueThumbnails` now deduplicates by file path (`pendingPaths`/`queuedPaths`).
- The scanner re-queues files with an empty `thumbnailPath` on the next scan (e.g. after an ffmpeg failure), so failed thumbnails are retried.
- Failed thumbnails now have a retry limit: the new `thumbnailRetries` field on `MediaFile` is incremented on each failure (`Database.incrementThumbnailRetries`), and at most 1 retry is attempted. Files that exceed the limit are skipped on subsequent launches/scans, so a permanently broken file can't block the queue forever.
- Fixed a bug where the retry logic in `ThumbnailGenerator.processQueue` (`.finally` after `.catch`) could corrupt the queue state: on retry, the item was re-queued but `processing`/`processedCount` were also reset by `.finally`, causing the next item to start processing concurrently. Processing completion is now tracked explicitly via `finishProcessingItem`.
- Docs (`WORKFLOW`, `ARCHITECTURE`, `DATA`) updated accordingly.

## 2026-08-10 — Auto-hide the window menu bar
- The window menu bar (File/Edit/View) is now hidden by default on Windows (`autoHideMenuBar: true`); it can still be shown temporarily with the Alt key.

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
