# Копирует в собранный пакет сторонние библиотеки, нужные приложению.
#
# Зачем это отдельным шагом: windeployqt кладёт библиотеки Qt, плагины и
# QML-модули, но не копирует сторонние библиотеки, от которых зависит Qt
# (ICU, freetype, harfbuzz, OpenSSL, ffmpeg с кодеками), и не копирует runtime
# MinGW (libgcc, libstdc++, libwinpthread). Без них приложение из пакета
# не запускается.
#
# Список библиотек не хардкодится: скрипт читает таблицы импортов всех уже
# развёрнутых бинарников (exe, DLL плагинов и QML-модулей) и добирает
# недостающие библиотеки из каталога bin тулчейна, повторяя обход, пока
# появляются новые зависимости. Набор поэтому сам подстраивается под версию Qt.
#
# Параметры:
#   OBJDUMP      — путь к objdump (есть в MinGW; при MSVC шаг пропускается);
#   PACKAGE_DIR  — каталог развёрнутого пакета;
#   SEARCH_DIR   — каталог bin тулчейна, откуда берутся библиотеки.
#
# Запуск: cmake -DOBJDUMP=... -DPACKAGE_DIR=... -DSEARCH_DIR=... -P этот_файл

if(NOT EXISTS "${OBJDUMP}")
    message(STATUS "Сторонние библиотеки не копируются: не найден objdump (${OBJDUMP})")
    return()
endif()

file(GLOB_RECURSE _queue LIST_DIRECTORIES FALSE
     "${PACKAGE_DIR}/*.exe" "${PACKAGE_DIR}/*.dll")

set(_scanned "")
set(_handled "")
set(_copied "")

while(_queue)
    # Сканируем только то, что ещё не проходили.
    set(_batch "")
    foreach(_file IN LISTS _queue)
        if(NOT _file IN_LIST _scanned)
            list(APPEND _batch "${_file}")
        endif()
    endforeach()
    set(_queue "")
    if(NOT _batch)
        break()
    endif()
    list(APPEND _scanned ${_batch})

    # objdump умеет читать несколько файлов за раз; режем на пачки, потому что
    # командная строка Windows ограничена по длине.
    list(LENGTH _batch _count)
    set(_from 0)
    while(_from LESS _count)
        math(EXPR _to "${_from} + 39")
        if(_to GREATER_EQUAL _count)
            math(EXPR _to "${_count} - 1")
        endif()
        set(_chunk "")
        foreach(_index RANGE ${_from} ${_to})
            list(GET _batch ${_index} _item)
            list(APPEND _chunk "${_item}")
        endforeach()

        execute_process(COMMAND "${OBJDUMP}" -p ${_chunk}
                        OUTPUT_VARIABLE _dump
                        ERROR_QUIET)
        string(REGEX MATCHALL "DLL Name: [^\r\n]+" _imports "${_dump}")

        foreach(_import IN LISTS _imports)
            string(REPLACE "DLL Name:" "" _name "${_import}")
            string(STRIP "${_name}" _name)
            string(TOLOWER "${_name}" _lower)
            # api-ms-win-* — виртуальные наборы API Windows, файлов нет.
            if(_lower MATCHES "^api-ms-win" OR _lower IN_LIST _handled)
                continue()
            endif()
            list(APPEND _handled "${_lower}")

            if(EXISTS "${PACKAGE_DIR}/${_name}")
                continue()   # уже в пакете
            endif()
            if(EXISTS "${SEARCH_DIR}/${_name}")
                list(APPEND _copied "${_name}")
                list(APPEND _queue "${SEARCH_DIR}/${_name}")
            endif()
        endforeach()

        math(EXPR _from "${_to} + 1")
    endwhile()
endwhile()

if(_copied)
    list(REMOVE_DUPLICATES _copied)
    set(_sources "")
    foreach(_name IN LISTS _copied)
        list(APPEND _sources "${SEARCH_DIR}/${_name}")
    endforeach()
    execute_process(COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                            ${_sources} "${PACKAGE_DIR}"
                    RESULT_VARIABLE _copy_result)
    if(NOT _copy_result EQUAL 0)
        message(FATAL_ERROR "Не удалось скопировать сторонние библиотеки в ${PACKAGE_DIR}")
    endif()
    list(LENGTH _copied _copied_count)
    message(STATUS "Сторонних библиотек скопировано: ${_copied_count}")
else()
    message(STATUS "Сторонние библиотеки не требуются")
endif()
