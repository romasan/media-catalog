# Сборка и упаковка

Приложение собирается из одного кода для macOS, Windows и Linux. Требования
одинаковы везде: **CMake 3.21+**, **компилятор с C++20** и **Qt 6.5+** с модулями
Core, Gui, Qml, Quick, QuickControls2, Svg, Sql, Multimedia.

Для превью и чтения метаданных видео нужен **ffmpeg** (и `ffprobe` в его составе).
Приложение ищет их в `PATH`, а также в типовых местах (`/opt/homebrew/bin`,
`/usr/local/bin`, `/usr/bin`). Путь можно задать явно переменными окружения
`MEDIACATALOG_FFMPEG` и `MEDIACATALOG_FFPROBE`.

## macOS

```bash
brew install cmake ninja qt ffmpeg

cmake -S . -B cmake-build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build cmake-build

# запуск
open cmake-build/mediacatalog.app
# или с отдельным каталогом данных
./cmake-build/mediacatalog.app/Contents/MacOS/mediacatalog --data-dir /tmp/mc-data
```

Сборка пакета `.app` с библиотеками Qt внутри и DMG:

```bash
cmake --build cmake-build --target deploy   # macdeployqt + повторная подпись
cd cmake-build && cpack -G DragNDrop        # получится MediaCatalog-2.0.0-Darwin.dmg
```

Подпись по умолчанию — ad-hoc (достаточно для запуска на своей машине). Для
распространения укажите сертификат:

```bash
cmake -S . -B cmake-build -DMACOS_SIGN_IDENTITY="Developer ID Application: Имя (TEAMID)"
```

Если CMake не находит Qt, укажите префикс вручную:
`-DCMAKE_PREFIX_PATH=/opt/homebrew`.

## Windows

1. Установите [Qt 6 Online Installer](https://www.qt.io/download-qt-installer) и
   отметьте компоненты **MSVC 2019/2022 64-bit** с модулями Qt Quick, Qt
   Multimedia и Qt Svg. Также нужны **Visual Studio Build Tools** (компилятор и
   Windows SDK), **CMake** и **Ninja**.
2. Соберите из «x64 Native Tools Command Prompt for VS»:

```bat
cmake -S . -B cmake-build -G Ninja -DCMAKE_BUILD_TYPE=Release ^
      -DCMAKE_PREFIX_PATH=C:/Qt/6.8.0/msvc2022_64
cmake --build cmake-build
cmake-build\mediacatalog.exe
```

3. Пакет с библиотеками Qt и установщик NSIS:

```bat
cmake --build cmake-build --target deploy   :: windeployqt
cd cmake-build
cpack -G NSIS                               :: MediaCatalog-2.0.0-win64.exe
```

`ffmpeg.exe` и `ffprobe.exe` положите в `PATH` или рядом с
`mediacatalog.exe` — иначе превью и дата съёмки видео работать не будут
(об этом приложение предупредит при запуске).

## Linux

Зависимости (пример для Debian/Ubuntu):

```bash
sudo apt install build-essential cmake ninja-build ffmpeg \
     qt6-base-dev qt6-declarative-dev qt6-multimedia-dev qt6-svg-dev \
     libqt6sql6-sqlite libgl1-mesa-dev
```

Сборка и запуск:

```bash
cmake -S . -B cmake-build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build cmake-build
./cmake-build/mediacatalog
```

Пакеты:

```bash
cd cmake-build
cpack -G TGZ     # архив с бинарником
cpack -G DEB     # пакет Debian/Ubuntu
```

Linux-сборка использует системные библиотеки Qt, поэтому копировать их в пакет не
нужно. Для самодостаточного варианта можно собрать AppImage утилитой
`linuxdeploy` с плагином Qt.

## Тесты

```bash
cd cmake-build
ctest --output-on-failure
# или отдельно
./mc_core_tests
./mc_model_tests
```

`mc_core_tests` проверяет ядро (хэш имён превью, EXIF, база, сканирование,
фильтры, экспорт/импорт), `mc_model_tests` — путь от файлов на диске до данных,
которые получает QML.

## Полезные ключи запуска

| Ключ | Назначение |
|------|-----------|
| `--data-dir <путь>` | Каталог данных вместо системного |
| `--version` | Версия приложения |
| `--help` | Список ключей |

## Частые проблемы

| Симптом | Причина и решение |
|---------|-------------------|
| `Could not find a package configuration file provided by "Qt6"` | Укажите `-DCMAKE_PREFIX_PATH=<каталог установки Qt>` |
| Превью не генерируются | Не найден `ffmpeg`: установите его или задайте `MEDIACATALOG_FFMPEG` |
| Видео не воспроизводится | Не установлен модуль Qt Multimedia или его системный бэкенд |
| `ERROR: Cannot resolve rpath` при `macdeployqt` | Не установлен какой-то модуль Qt (например, `qtsvg`): установите и пересоберите |
