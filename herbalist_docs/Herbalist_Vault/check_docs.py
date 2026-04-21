#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
check_docs.py — проверка целостности документации Herbalist Vault
Запуск: python check_docs.py
Генерирует отчёт docs_check_report.md в той же папке.
"""

import os
import re
import sys
from pathlib import Path
from collections import defaultdict
from datetime import datetime

# Конфигурация
ROOT_DIR = Path(__file__).parent.absolute()
SKIP_DIRS = {'.obsidian', 'build', '05_Assets', 'snippets', 'plugins', 'Current', 'Future'}
SKIP_FILES = {'_template.md', '_Index.md', '.md'}  # .md — пустые файлы-заглушки (будут удалены)
ALLOWED_EMPTY = {'_template.md', '_Index.md'}  # эти файлы могут быть пустыми или почти пустыми

# Обязательные поля для разных типов файлов (ключ: часть пути, словарь: поле -> обязательное)
REQUIRED_FRONTMATTER = {
    '04_Compendium/Bestiary': {
        'name': 'название существа',
        'type': 'тип (например, дух, нечисть)',
        'biome': 'биом или any'
    },
    '04_Compendium/Ingredients': {
        'name': 'название ингредиента',
        'biome': 'биом происхождения',
        'properties': 'свойства (можно массив или строка)'
    },
    '04_Compendium/Biomes': {
        'name': 'название биома',
        'description': 'описание'
    },
    '01_Glossary': {
        'term': 'термин',
        'definition': 'определение'
    }
}

# Дополнительные проверки: для бестиария/ингредиентов биом должен существовать в Biomes/ или быть 'any'
BIOMES_DIR = ROOT_DIR / '04_Compendium' / 'Biomes'
KNOWN_BIOMES = {f.stem for f in BIOMES_DIR.glob('*.md') if f.stem != '_template'}

# Специальные папки в бестиарии, которые не соответствуют биомам (например, нечисть)
SPECIAL_BESTIARY_DIRS = {'nechist'}


def get_all_md_files(root):
    """Рекурсивно собирает все .md файлы, исключая SKIP_DIRS."""
    md_files = []
    for path in root.rglob('*.md'):
        # Пропуск папок
        if any(skip in path.parts for skip in SKIP_DIRS):
            continue
        md_files.append(path)
    return md_files


def is_empty_file(path):
    """Проверяет, пуст ли файл (только пробелы/переносы или пустой frontmatter)."""
    if path.name in ALLOWED_EMPTY:
        return False
    content = path.read_text(encoding='utf-8', errors='ignore').strip()
    if not content:
        return True
    # Файл, содержащий только --- (пустой frontmatter) тоже считаем пустым
    if content == '---':
        return True
    return False


def find_duplicates(files):
    """Возвращает словарь: имя_файла_без_расширения -> список путей с одинаковым именем в одной папке."""
    by_folder = defaultdict(list)
    for f in files:
        by_folder[f.parent].append(f)
    
    duplicates = {}
    for folder, flist in by_folder.items():
        names = defaultdict(list)
        for f in flist:
            stem = f.stem
            names[stem].append(f)
        for stem, paths in names.items():
            if len(paths) > 1:
                duplicates[stem] = paths
    return duplicates


def extract_wikilinks(content, current_file):
    """
    Извлекает из markdown все внутренние ссылки [[...]].
    Возвращает список кортежей (строка_ссылки, строка_номера).
    """
    # Регулярка для [[...]] — не жадная, поддерживает алиасы и якоря
    pattern = r'\[\[(.*?)\]\]'
    matches = []
    for line_num, line in enumerate(content.split('\n'), 1):
        for match in re.finditer(pattern, line):
            link = match.group(1)
            # Отбрасываем алиас и якорь
            if '|' in link:
                link = link.split('|')[0]
            if '#' in link:
                link = link.split('#')[0]
            matches.append((link.strip(), line_num))
    return matches


def resolve_wikilink(link, current_file):
    """
    Пытается найти целевой файл по вики-ссылке.
    Возвращает Path к файлу или None.
    """
    # Если ссылка начинается с / (абсолютная от корня)
    if link.startswith('/'):
        target = ROOT_DIR / link[1:]
    else:
        # Относительная от текущего файла
        target = (current_file.parent / link).with_suffix('.md')
        if not target.exists():
            # Попробуем без указания расширения (если ссылка без .md)
            if not link.endswith('.md'):
                target = (current_file.parent / (link + '.md'))
    
    # Проверяем существование
    if target.exists() and target.is_file():
        return target
    
    # Если не нашли, возможно ссылка ведёт на файл в другой части иерархии (глобальный поиск)
    # Это неоптимально, но для совместимости сделаем поиск по всем .md
    # Но для скорости лучше сначала проверить в подпапках, но пока упростим:
    for root, dirs, files in os.walk(ROOT_DIR):
        if any(skip in root for skip in SKIP_DIRS):
            continue
        for file in files:
            if file == f"{link}.md" or file == link:
                return Path(root) / file
    return None


def extract_frontmatter(content):
    """Возвращает словарь полей из YAML frontmatter (между --- ... ---)."""
    lines = content.split('\n')
    if not lines or lines[0].strip() != '---':
        return {}
    end_index = None
    for i in range(1, len(lines)):
        if lines[i].strip() == '---':
            end_index = i
            break
    if end_index is None:
        return {}
    frontmatter_lines = lines[1:end_index]
    # Простой парсинг "ключ: значение"
    data = {}
    for line in frontmatter_lines:
        if ':' in line:
            key, val = line.split(':', 1)
            key = key.strip()
            val = val.strip()
            # Убираем кавычки, если есть
            if val.startswith('"') and val.endswith('"'):
                val = val[1:-1]
            if val.startswith("'") and val.endswith("'"):
                val = val[1:-1]
            data[key] = val
    return data


def check_frontmatter(path, required_fields):
    """Проверяет наличие обязательных полей в frontmatter. Возвращает список ошибок."""
    content = path.read_text(encoding='utf-8', errors='ignore')
    fm = extract_frontmatter(content)
    errors = []
    for field, description in required_fields.items():
        if field not in fm:
            errors.append(f"Отсутствует поле '{field}' ({description})")
    return errors


def check_biome_consistency(path, file_type):
    """
    Для бестиария и ингредиентов проверяет, что указанный биом существует в папке Biomes
    или является специальным значением ('any', 'nechist' и т.п.)
    """
    content = path.read_text(encoding='utf-8', errors='ignore')
    fm = extract_frontmatter(content)
    if 'biome' not in fm:
        return []  # ошибка будет поймана обязательными полями
    
    biome_value = fm['biome']
    errors = []
    
    # Специальные допустимые значения
    special_ok = {'any', 'nechist', 'all', 'none'}
    if biome_value in special_ok:
        return []
    
    # Проверка на существование файла биома
    biome_file = BIOMES_DIR / f"{biome_value}.md"
    if not biome_file.exists():
        errors.append(f"Биом '{biome_value}' не найден среди {KNOWN_BIOMES}")
    return errors


def main():
    print(f"Проверка документации в {ROOT_DIR}\n")
    errors = []
    warnings = []
    
    # 1. Сбор всех .md файлов
    all_md = get_all_md_files(ROOT_DIR)
    print(f"Найдено .md файлов: {len(all_md)}")
    
    # 2. Проверка пустых файлов
    empty = [f for f in all_md if is_empty_file(f)]
    for f in empty:
        errors.append(f"[ПУСТОЙ ФАЙЛ] {f.relative_to(ROOT_DIR)}")
    
    # 3. Дубликаты в одной папке
    duplicates = find_duplicates(all_md)
    for name, paths in duplicates.items():
        rel_paths = [str(p.relative_to(ROOT_DIR)) for p in paths]
        errors.append(f"[ДУБЛИКАТ] Имя '{name}' в папке {paths[0].parent.relative_to(ROOT_DIR)}: {', '.join(rel_paths)}")
    
    # 4. Проверка вики-ссылок
    broken_links = []
    for md_file in all_md:
        content = md_file.read_text(encoding='utf-8', errors='ignore')
        links = extract_wikilinks(content, md_file)
        for link, line_num in links:
            # Пропускаем внешние ссылки
            if re.match(r'^(https?://|mailto:|ftp://)', link):
                continue
            target = resolve_wikilink(link, md_file)
            if target is None:
                broken_links.append((md_file, link, line_num))
    
    for md_file, link, line_num in broken_links:
        errors.append(f"[БИТАЯ ССЫЛКА] {md_file.relative_to(ROOT_DIR)} стр.{line_num}: [[{link}]]")
    
    # 5. Проверка frontmatter и биомов
    for rel_path_pattern, required in REQUIRED_FRONTMATTER.items():
        pattern_dir = ROOT_DIR / rel_path_pattern
        if not pattern_dir.exists():
            continue
        for md_file in pattern_dir.rglob('*.md'):
            if md_file.name in SKIP_FILES:
                continue
            # Определяем тип файла по пути
            if 'Bestiary' in md_file.parts:
                file_type = 'bestiary'
            elif 'Ingredients' in md_file.parts:
                file_type = 'ingredient'
            elif 'Biomes' in md_file.parts:
                file_type = 'biome'
            elif 'Glossary' in md_file.parts:
                file_type = 'glossary'
            else:
                continue
            
            # Проверка обязательных полей
            fm_errors = check_frontmatter(md_file, required)
            for err in fm_errors:
                errors.append(f"[FRONTMATTER] {md_file.relative_to(ROOT_DIR)}: {err}")
            
            # Проверка биома (только для bestiary и ingredient)
            if file_type in ('bestiary', 'ingredient'):
                # Для бестиария дополнительная проверка: если файл лежит в папке SPECIAL_BESTIARY_DIRS, то не проверяем биом строго
                parent_dir = md_file.parent.name
                if file_type == 'bestiary' and parent_dir in SPECIAL_BESTIARY_DIRS:
                    continue  # пропускаем, так как нечисть может быть везде
                biome_errors = check_biome_consistency(md_file, file_type)
                for err in biome_errors:
                    errors.append(f"[БИОМ] {md_file.relative_to(ROOT_DIR)}: {err}")
    
    # 6. Дополнительная проверка: все ли папки бестиария соответствуют биомам (кроме special)
    bestiary_root = ROOT_DIR / '04_Compendium' / 'Bestiary'
    if bestiary_root.exists():
        for subdir in bestiary_root.iterdir():
            if subdir.is_dir() and subdir.name not in SPECIAL_BESTIARY_DIRS:
                if subdir.name not in KNOWN_BIOMES:
                    warnings.append(f"[НЕИЗВЕСТНЫЙ БИОМ В БЕСТИАРИИ] Папка '{subdir.name}' не соответствует ни одному биому из {KNOWN_BIOMES}")
    
    # Вывод отчёта в консоль
    print("\n" + "="*80)
    if errors:
        print(f"НАЙДЕНО {len(errors)} ОШИБОК:")
        for err in errors:
            print(f"  {err}")
    else:
        print("ОШИБОК НЕ НАЙДЕНО")
    
    if warnings:
        print(f"\nПРЕДУПРЕЖДЕНИЙ: {len(warnings)}")
        for warn in warnings:
            print(f"  {warn}")
    else:
        print("\nПРЕДУПРЕЖДЕНИЙ НЕТ")
    
    # ---- Запись отчёта в MD файл ----
    report_path = ROOT_DIR / "docs_check_report.md"
    timestamp = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    
    with open(report_path, 'w', encoding='utf-8') as f:
        f.write(f"# Отчёт проверки документации\n\n")
        f.write(f"**Дата и время:** {timestamp}\n\n")
        f.write(f"**Корневая папка:** `{ROOT_DIR}`\n\n")
        f.write(f"**Проверено файлов `.md`:** {len(all_md)}\n\n")
        f.write(f"## Сводка\n\n")
        f.write(f"- **Ошибки:** {len(errors)}\n")
        f.write(f"- **Предупреждения:** {len(warnings)}\n\n")
        
        if errors:
            f.write(f"## Список ошибок\n\n")
            for err in errors:
                # Экранируем квадратные скобки для markdown (они могут быть в сообщениях)
                err_escaped = err.replace('[', '\\[').replace(']', '\\]')
                f.write(f"- {err_escaped}\n")
            f.write("\n")
        else:
            f.write(f"## Ошибки\n\n✅ Ошибок не найдено.\n\n")
        
        if warnings:
            f.write(f"## Предупреждения\n\n")
            for warn in warnings:
                warn_escaped = warn.replace('[', '\\[').replace(']', '\\]')
                f.write(f"- {warn_escaped}\n")
            f.write("\n")
        else:
            f.write(f"## Предупреждения\n\n✅ Предупреждений нет.\n\n")
        
        f.write("---\n")
        f.write("*Отчёт сгенерирован автоматически скриптом `check_docs.py`.*\n")
    
    print(f"\nОтчёт сохранён: {report_path}")
    
    # Возвращаем код выхода
    sys.exit(1 if errors else 0)


if __name__ == '__main__':
    main()