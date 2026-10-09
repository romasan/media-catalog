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

# Homebrew ставит Qt по общему префиксу; подсказываем CMake, где искать,
# чтобы сборка работала без ручного -DCMAKE_PREFIX_PATH.
if(APPLE AND NOT DEFINED CMAKE_PREFIX_PATH)
    foreach(prefix /opt/homebrew /usr/local)
        if(EXISTS "${prefix}/lib/cmake/Qt6/Qt6Config.cmake")
            list(APPEND CMAKE_PREFIX_PATH "${prefix}")
            break()
        endif()
    endforeach()
endif()

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Sql Test)

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

if(APPLE)
    # Сканирование ФС и thumbnail-кэш работают с широкими путями.
    target_compile_definitions(mc_core PRIVATE _DARWIN_C_SOURCE)
endif()

# ==== Консольные тесты ядра (без GUI) ====
add_executable(mc_core_tests
    tests/core_tests.cpp
)
target_link_libraries(mc_core_tests PRIVATE mc_core Qt6::Test)

enable_testing()
add_test(NAME core COMMAND mc_core_tests)

# ==== Установка ====
include(GNUInstallDirs)
if(TARGET mediacatalog)
    install(TARGETS mediacatalog
        BUNDLE  DESTINATION .
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
    )
endif()


add_executable(dbg2 /tmp/dbg2.cpp)
target_link_libraries(dbg2 PRIVATE mc_core)

