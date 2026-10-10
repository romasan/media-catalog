cmake_minimum_required(VERSION 3.21)

project(MediaCatalog
    VERSION 2.0.0
    DESCRIPTION "Каталогизация фото и видео — нативная версия"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Тип сборки" FORCE)
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Homebrew и похожие менеджеры пакетов ставят Qt по общему префиксу.
# Подсказываем CMake, где искать, чтобы сборка не требовала
# ручного -DCMAKE_PREFIX_PATH.
if(NOT DEFINED CMAKE_PREFIX_PATH)
    if(APPLE)
        foreach(prefix /opt/homebrew /usr/local)
            if(EXISTS "${prefix}/lib/cmake/Qt6/Qt6Config.cmake")
                list(APPEND CMAKE_PREFIX_PATH "${prefix}")
                break()
            endif()
        endforeach()
    elseif(WIN32)
        foreach(prefix "C:/Qt/6.8.0/msvc2022_64" "C:/Qt/6.7.0/msvc2019_64")
            if(EXISTS "${prefix}/lib/cmake/Qt6/Qt6Config.cmake")
                list(APPEND CMAKE_PREFIX_PATH "${prefix}")
                break()
            endif()
        endforeach()
    endif()
endif()

find_package(Qt6 6.5 REQUIRED COMPONENTS
    Core Gui Qml Quick QuickControls2 Svg Sql Multimedia Test
)

qt_standard_project_setup(REQUIRES 6.5)

# ==== Ядро: логика без зависимости от GUI ====
# Вынесено в отдельную библиотеку, чтобы её можно было покрыть
# консольными тестами и переиспользовать в будущих оболочках.
add_library(mc_core STATIC
    src/core/MediaTypes.h
    src/core/PathHash.h
    src/core/PathHash.cpp
    src/core/Paths.h
    src/core/Paths.cpp
    src/core/MetaTags.h
    src/core/MetaTags.cpp
    src/core/CatalogDatabase.h
    src/core/CatalogDatabase.cpp
    src/core/MediaFormats.h
    src/core/MediaFormats.cpp
    src/core/MediaScanner.h
    src/core/MediaScanner.cpp
    src/core/MetadataReader.h
    src/core/MetadataReader.cpp
    src/core/ThumbnailGenerator.h
    src/core/ThumbnailGenerator.cpp
    src/core/MediaFilter.h
    src/core/MediaFilter.cpp
    src/core/ExportImport.h
    src/core/ExportImport.cpp
    src/core/MediaLibrary.h
    src/core/MediaLibrary.cpp
)

target_include_directories(mc_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(mc_core PUBLIC Qt6::Core Qt6::Gui Qt6::Sql)

# ==== Приложение ====
qt_add_executable(mediacatalog
    src/main.cpp
    src/app/AppController.h
    src/app/AppController.cpp
    src/app/MediaListModel.h
    src/app/MediaListModel.cpp
    src/app/TagListModel.h
    src/app/TagListModel.cpp
)

target_link_libraries(mediacatalog PRIVATE
    mc_core
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Svg
    Qt6::Multimedia
)

# Theme — синглтон: свойство нужно выставить до qt_add_qml_module,
# иначе `pragma Singleton` не учитывается и Theme не виден из других файлов.
set_source_files_properties(qml/Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

qt_add_qml_module(mediacatalog
    URI MediaCatalog
    VERSION 1.0
    QML_FILES
        qml/Theme.qml
        qml/Main.qml
        qml/MediaGrid.qml
        qml/MediaCard.qml
        qml/FullscreenViewer.qml
        qml/FilterBar.qml
        qml/BatchTagBar.qml
        qml/BurgerMenu.qml
        qml/CatalogManagerPopup.qml
        qml/TagManagerPopup.qml
        qml/DateRangeLabel.qml
        qml/ToastContainer.qml
        qml/ThumbnailProgressBar.qml
        qml/DraggableResizable.qml
        qml/PhotoGlyph.qml
        qml/VideoGlyph.qml
        qml/CloseGlyph.qml
        qml/BurgerGlyph.qml
        qml/PlayGlyph.qml
)

set_target_properties(mediacatalog PROPERTIES
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_BUNDLE_NAME "Media Catalog"
    MACOSX_BUNDLE_GUI_IDENTIFIER "com.mediacatalog.app"
    MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
    WIN32_EXECUTABLE TRUE
)

# Значок приложения: на macOS — .icns, на Windows — .ico.
if(APPLE AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/build/icon.icns")
    set_source_files_properties("${CMAKE_CURRENT_SOURCE_DIR}/build/icon.icns"
        PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")
    target_sources(mediacatalog PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/build/icon.icns")
    set_target_properties(mediacatalog PROPERTIES MACOSX_BUNDLE_ICON_FILE "icon.icns")
elseif(WIN32 AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/build/icon.ico")
    set_source_files_properties("${CMAKE_CURRENT_SOURCE_DIR}/build/icon.ico"
        PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")
    target_sources(mediacatalog PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/build/icon.ico")
endif()

# ==== Консольные тесты (без GUI) ====
add_executable(mc_core_tests
    tests/core_tests.cpp
)
target_link_libraries(mc_core_tests PRIVATE mc_core Qt6::Test)

# Тесты слоя моделей: проверяют путь от файлов на диске до данных для QML.
add_executable(mc_model_tests
    tests/model_tests.cpp
    src/app/MediaListModel.cpp
    src/app/TagListModel.cpp
)
target_include_directories(mc_model_tests PRIVATE src)
target_link_libraries(mc_model_tests PRIVATE mc_core Qt6::Test)

enable_testing()
add_test(NAME core COMMAND mc_core_tests)
add_test(NAME models COMMAND mc_model_tests)

include(GNUInstallDirs)

# ==== Упаковка ====
# Приложение зависит от библиотек Qt, поэтому перед упаковкой их нужно
# положить рядом: на macOS это macdeployqt, на Windows — windeployqt.
# Инструменты поставляются вместе с Qt и лежат в bin каталога установки Qt.
#
# Развёртывание идёт в ОТДЕЛЬНЫЙ каталог (<build>/deploy). Это принципиально:
# macdeployqt и windeployqt правят пакет по месту, подменяя пути к библиотекам
# на внутренние. Если развернуть пакет прямо в дереве сборки, при запуске на
# машине с установленным Qt загрузятся две копии библиотек (из пакета и из
# системы), и приложение не стартует. Поэтому сборка для разработки остаётся
# нетронутой, а пакет лежит отдельно.

set(DEPLOY_DIR "${CMAKE_BINARY_DIR}/deploy")

# Поиск каталога с инструментами развёртывания.
get_target_property(_qt_qmake Qt6::qmake IMPORTED_LOCATION)
get_filename_component(_qt_bin_dir "${_qt_qmake}" DIRECTORY)

if(APPLE)
    find_program(MACDEPLOYQT_EXECUTABLE macdeployqt HINTS "${_qt_bin_dir}")

    # Подпись ad-hoc (без сертификата) достаточна для запуска на этой машине;
    # для распространения задайте
    # -DMACOS_SIGN_IDENTITY="Developer ID Application: ...".
    set(MACOS_SIGN_IDENTITY "-" CACHE STRING "Идентификатор подписи macOS (- = ad-hoc)")

    # -qmldir обязателен: без него macdeployqt копирует библиотеки Qt, но не
    # QML-модули (QtQuick.Controls и его стили), и приложение не находит их
    # на машине без установленного Qt.
    add_custom_target(deploy
        COMMAND "${CMAKE_COMMAND}" -E rm -rf "${DEPLOY_DIR}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${DEPLOY_DIR}"
        COMMAND "${CMAKE_COMMAND}" -E copy_directory
                "$<TARGET_BUNDLE_DIR:mediacatalog>" "${DEPLOY_DIR}/mediacatalog.app"
        COMMAND "${MACDEPLOYQT_EXECUTABLE}" "${DEPLOY_DIR}/mediacatalog.app"
                -always-overwrite
                -qmldir=${CMAKE_CURRENT_SOURCE_DIR}/qml
        COMMAND /usr/bin/codesign --force --deep --sign "${MACOS_SIGN_IDENTITY}"
                "${DEPLOY_DIR}/mediacatalog.app"
        COMMENT "Пакет собран в ${DEPLOY_DIR}/mediacatalog.app (сборка для разработки не изменена)"
        DEPENDS mediacatalog
        USES_TERMINAL
        VERBATIM
    )

    # Запуск уже развёрнутого пакета — так проверяется именно то, что получит
    # пользователь.
    add_custom_target(deploy-run
        COMMAND /usr/bin/open "${DEPLOY_DIR}/mediacatalog.app"
        DEPENDS deploy
        COMMENT "Запуск развёрнутого пакета"
        USES_TERMINAL
    )

    set(CPACK_GENERATOR "DragNDrop")
    set(CPACK_DMG_VOLUME_NAME "Media Catalog")
elseif(WIN32)
    find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS "${_qt_bin_dir}")

    add_custom_target(deploy
        COMMAND "${CMAKE_COMMAND}" -E rm -rf "${DEPLOY_DIR}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${DEPLOY_DIR}"
        COMMAND "${CMAKE_COMMAND}" -E copy "$<TARGET_FILE:mediacatalog>"
                "${DEPLOY_DIR}/mediacatalog.exe"
        COMMAND "${WINDEPLOYQT_EXECUTABLE}" --release
                "${DEPLOY_DIR}/mediacatalog.exe"
        COMMENT "Пакет собран в ${DEPLOY_DIR}"
        DEPENDS mediacatalog
        USES_TERMINAL
        VERBATIM
    )

    set(CPACK_GENERATOR "NSIS")
    set(CPACK_NSIS_DISPLAY_NAME "Media Catalog")
    set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
    set(CPACK_NSIS_MODIFY_PATH OFF)
else()
    # На Linux Qt-библиотеки обычно приходят из пакетов дистрибутива,
    # поэтому достаточно скопировать бинарник.
    add_custom_target(deploy
        COMMAND "${CMAKE_COMMAND}" -E rm -rf "${DEPLOY_DIR}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${DEPLOY_DIR}"
        COMMAND "${CMAKE_COMMAND}" -E copy "$<TARGET_FILE:mediacatalog>" "${DEPLOY_DIR}/"
        COMMENT "Пакет собран в ${DEPLOY_DIR} (Qt берётся из системы)"
        DEPENDS mediacatalog
        USES_TERMINAL
        VERBATIM
    )

    add_custom_target(deploy-run
        COMMAND "${DEPLOY_DIR}/mediacatalog"
        DEPENDS deploy
        USES_TERMINAL
    )

    set(CPACK_GENERATOR "TGZ;DEB")
    set(CPACK_DEBIAN_PACKAGE_DEPENDS "libqt6core6, libqt6gui6, libqt6qml6, libqt6quick6, libqt6quickcontrols2-6, libqt6sql6-sqlite, libqt6multimedia6")
endif()

# Упаковщик берёт файлы из каталога развёртывания, а не из дерева сборки.
if(APPLE)
    install(DIRECTORY "${DEPLOY_DIR}/mediacatalog.app"
            DESTINATION .
            USE_SOURCE_PERMISSIONS)
else()
    install(PROGRAMS "${DEPLOY_DIR}/mediacatalog${CMAKE_EXECUTABLE_SUFFIX}"
            DESTINATION "${CMAKE_INSTALL_BINDIR}")
endif()

set(CPACK_PACKAGE_NAME "MediaCatalog")
set(CPACK_PACKAGE_VENDOR "Media Catalog")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Каталогизация фото и видео")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "Media Catalog")

include(CPack)



