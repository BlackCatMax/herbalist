#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Сборка итоговых документов из Obsidian Vault Herbalist.

Объединяет:
- GDD (02_GDD) → build/gdd_full.md
- Глоссарий (01_Glossary) → build/glossary_full.md
- Ингредиенты (04_Compendium/Ingredients) → build/ingredients_compendium.md
- Бестиарий (04_Compendium/Bestiary) → build/bestiary_compendium.md
- Биомы (04_Compendium/Biomes) → build/biomes_compendium.md
"""

import os
from pathlib import Path
from typing import List, Set

# Корневая папка Vault (скрипт лежит в корне)
VAULT_ROOT = Path(__file__).parent.resolve()
BUILD_DIR = VAULT_ROOT / "build"

# Исключаемые файлы (шаблоны)
EXCLUDE_NAMES: Set[str] = {"_template.md", ".gitkeep"}


def ensure_build_dir() -> None:
    """Создать папку build, если её нет."""
    BUILD_DIR.mkdir(exist_ok=True)


def collect_files(directory: Path, recursive: bool = True, sort_key=None) -> List[Path]:
    """
    Собрать все .md файлы из директории, исключая указанные имена.
    Возвращает список, отсортированный по sort_key (по умолчанию по имени).
    """
    if not directory.exists():
        print(f"⚠️  Директория не найдена: {directory}")
        return []

    pattern = "**/*.md" if recursive else "*.md"
    files = [
        p for p in directory.glob(pattern)
        if p.is_file() and p.name not in EXCLUDE_NAMES
    ]
    if sort_key is None:
        files.sort(key=lambda p: p.name)
    else:
        files.sort(key=sort_key)
    return files


def merge_files(files: List[Path], output_file: Path, title: str = "") -> None:
    """
    Объединить содержимое файлов в один, добавив заголовок и разделители.
    """
    with open(output_file, "w", encoding="utf-8") as out:
        if title:
            out.write(f"# {title}\n\n")
        for i, fpath in enumerate(files):
            rel_path = fpath.relative_to(VAULT_ROOT)
            out.write(f"<!-- BEGIN {rel_path} -->\n\n")
            with open(fpath, "r", encoding="utf-8") as inf:
                out.write(inf.read().rstrip() + "\n")
            out.write(f"\n<!-- END {rel_path} -->\n")
            if i < len(files) - 1:
                out.write("\n\n---\n\n\n")
    print(f"✅ Создан: {output_file} ({len(files)} файлов)")


def build_gdd() -> None:
    """Собрать GDD из 02_GDD."""
    source = VAULT_ROOT / "02_GDD"
    output = BUILD_DIR / "gdd_full.md"
    files = collect_files(source, recursive=False)
    # Сортировка по номеру в начале имени файла
    files.sort(key=lambda p: p.name)
    merge_files(files, output, title="Herbalist Game Design Document")


def build_glossary() -> None:
    """Собрать глоссарий из 01_Glossary."""
    source = VAULT_ROOT / "01_Glossary"
    output = BUILD_DIR / "glossary_full.md"
    files = collect_files(source, recursive=False)
    # _Index.md поставить первым, остальные по алфавиту
    def glossary_key(p: Path) -> str:
        if p.name == "_Index.md":
            return "000_Index"
        return p.name
    files.sort(key=glossary_key)
    merge_files(files, output, title="Глоссарий Herbalist")


def build_compendium(subdir: str, output_name: str, title: str) -> None:
    """
    Собрать компендиум из вложенной структуры (по биомам).
    """
    source = VAULT_ROOT / "04_Compendium" / subdir
    output = BUILD_DIR / output_name
    files = collect_files(source, recursive=True)
    # Сортировка по пути: сначала папка, потом имя файла
    files.sort(key=lambda p: (p.parent.name, p.name))
    merge_files(files, output, title=title)


def main() -> None:
    print("🚀 Сборка документации Herbalist...\n")
    ensure_build_dir()

    build_gdd()
    build_glossary()
    build_compendium("Ingredients", "ingredients_compendium.md", "Компендиум ингредиентов")
    build_compendium("Bestiary", "bestiary_compendium.md", "Бестиарий")
    build_compendium("Biomes", "biomes_compendium.md", "Компендиум биомов")

    print("\n✨ Готово! Все файлы сохранены в папке build.")


if __name__ == "__main__":
    main()