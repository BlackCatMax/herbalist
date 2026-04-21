#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
remove_compendium_footers.py — удаление навигационных футеров вида:
---
... ← ... [...](...) ...

во всех .md файлах внутри указанной подпапки (по умолчанию 04_Compendium).
"""

import os
import sys
import argparse
from pathlib import Path

SEPARATOR = "---"


def is_nav_line(line: str) -> bool:
    """Возвращает True, если строка похожа на навигационную ссылку."""
    # Содержит стрелку и квадратные скобки
    return "←" in line and "[" in line and "]" in line


def remove_nav_footer(content):
    """
    Удаляет блок футера из контента файла.
    Возвращает (новый_контент, был_ли_удалён).
    """
    lines = content.splitlines()
    if len(lines) < 2:
        return content, False

    # Найти последнее вхождение разделителя "---", за которым следует навигационная строка
    footer_start = None
    for i in range(len(lines) - 2, -1, -1):
        if lines[i] == SEPARATOR and i + 1 < len(lines) and is_nav_line(lines[i + 1]):
            footer_start = i
            break

    if footer_start is None:
        return content, False

    # Обрезаем до разделителя (не включая его)
    new_lines = lines[:footer_start]
    # Убираем пустые строки в конце
    while new_lines and new_lines[-1] == "":
        new_lines.pop()
    if new_lines:
        new_lines.append("")  # одна пустая строка в конце файла

    return "\n".join(new_lines), True


def process_file(filepath, dry_run=False):
    try:
        with open(filepath, 'r', encoding='utf-8') as f:
            content = f.read()
    except Exception as e:
        print(f"Ошибка чтения {filepath}: {e}", file=sys.stderr)
        return False

    new_content, changed = remove_nav_footer(content)
    if not changed:
        return False

    if dry_run:
        rel_path = os.path.relpath(filepath, start=Path.cwd())
        print(f"[DRY RUN] Будет изменён: {rel_path}")
        return True
    else:
        try:
            with open(filepath, 'w', encoding='utf-8') as f:
                f.write(new_content)
            rel_path = os.path.relpath(filepath, start=Path.cwd())
            print(f"Изменён: {rel_path}")
            return True
        except Exception as e:
            print(f"Ошибка записи {filepath}: {e}", file=sys.stderr)
            return False


def main():
    parser = argparse.ArgumentParser(
        description="Удаление навигационных футеров (--- + ссылка со ←) из .md файлов"
    )
    parser.add_argument("root_dir", help="Корневая папка (например, K:\\...\\Herbalist_Vault)")
    parser.add_argument(
        "--subdir",
        default="04_Compendium",
        help="Подпапка для обработки (по умолчанию 04_Compendium)"
    )
    parser.add_argument("--dry-run", action="store_true", help="Показать, что будет изменено, без записи")
    args = parser.parse_args()

    root = Path(args.root_dir) / args.subdir
    if not root.is_dir():
        print(f"Ошибка: {root} не существует или не является директорией", file=sys.stderr)
        sys.exit(1)

    md_files = list(root.rglob("*.md"))
    print(f"Найдено .md файлов в {root}: {len(md_files)}")
    changed_count = 0

    for md_file in md_files:
        if process_file(md_file, dry_run=args.dry_run):
            changed_count += 1

    if args.dry_run:
        print(f"\n[DRY RUN] Файлов, которые будут изменены: {changed_count}")
    else:
        print(f"\nИзменено файлов: {changed_count}")


if __name__ == "__main__":
    main()