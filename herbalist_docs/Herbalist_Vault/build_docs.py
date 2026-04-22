#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Сборка итоговых документов из Obsidian Vault Herbalist.

Объединяет:
- GDD (02_GDD) → build/gdd_full.md
- Глоссарий (01_Glossary) → build/glossary_full.md
- Ингредиенты (04_Compendium/Растительность) → build/ingredients_compendium.md
- Бестиарий (04_Compendium/Бестиарий) → build/bestiary_compendium.md
- Биомы (04_Compendium/Биомы) → build/biomes_compendium.md
"""

import os
from pathlib import Path
from typing import List, Set, Optional

# Корневая папка Vault (скрипт лежит в корне)
VAULT_ROOT = Path(__file__).parent.resolve()
BUILD_DIR = VAULT_ROOT / "build"

# Исключаемые файлы (шаблоны, пустые файлы)
EXCLUDE_NAMES: Set[str] = {"_template.md", "_template (1).md", ".gitkeep"}

# Исключаемые папки
EXCLUDE_DIRS: Set[str] = {".translit_backup", ".backup_gdd", ".obsidian", "build"}


def ensure_build_dir() -> None:
    """Создать папку build, если её нет."""
    BUILD_DIR.mkdir(exist_ok=True)


def collect_files(
    directory: Path, 
    recursive: bool = True, 
    exclude_dirs: Optional[Set[str]] = None
) -> List[Path]:
    """
    Собрать все .md файлы из директории, исключая указанные имена файлов и папок.
    """
    if not directory.exists():
        print(f"⚠️  Директория не найдена: {directory}")
        return []

    files = []
    exclude_dirs = exclude_dirs or set()
    
    if recursive:
        for root, dirs, filenames in os.walk(directory):
            # Исключаем папки
            dirs[:] = [d for d in dirs if d not in exclude_dirs and not d.startswith(".")]
            
            root_path = Path(root)
            for fname in filenames:
                if fname.endswith(".md") and fname not in EXCLUDE_NAMES:
                    files.append(root_path / fname)
    else:
        for fpath in directory.glob("*.md"):
            if fpath.is_file() and fpath.name not in EXCLUDE_NAMES:
                files.append(fpath)
    
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
            
            try:
                with open(fpath, "r", encoding="utf-8") as inf:
                    content = inf.read().rstrip()
                    out.write(content + "\n")
            except Exception as e:
                out.write(f"<!-- ОШИБКА ЧТЕНИЯ: {e} -->\n")
                print(f"  ⚠️  Ошибка чтения: {fpath}")
            
            out.write(f"\n<!-- END {rel_path} -->")
            
            if i < len(files) - 1:
                out.write("\n\n---\n\n\n")
    
    print(f"✅ Создан: {output_file} ({len(files)} файлов)")


def build_gdd() -> None:
    """Собрать GDD из 02_GDD."""
    source = VAULT_ROOT / "02_GDD"
    output = BUILD_DIR / "gdd_full.md"
    
    files = collect_files(source, recursive=False)
    # Исключаем _Index.md из сборки (это служебный файл)
    files = [f for f in files if f.name != "_Index.md"]
    # Сортировка по номеру в начале имени файла
    files.sort(key=lambda p: p.name)
    
    merge_files(files, output, title="Herbalist Game Design Document")


def build_glossary() -> None:
    """Собрать глоссарий из 01_Glossary."""
    source = VAULT_ROOT / "01_Glossary"
    output = BUILD_DIR / "glossary_full.md"
    
    files = collect_files(source, recursive=False)
    
    # _Index.md поставить первым, остальные по алфавиту
    def glossary_key(p: Path) -> tuple:
        if p.name == "_Index.md":
            return (0, "")
        return (1, p.name)
    
    files.sort(key=glossary_key)
    merge_files(files, output, title="Глоссарий Herbalist")


def build_ingredients() -> None:
    """Собрать компендиум ингредиентов."""
    source = VAULT_ROOT / "04_Compendium" / "Растительность"
    output = BUILD_DIR / "ingredients_compendium.md"
    
    files = collect_files(source, recursive=True)
    
    # Сортировка по биому (имя папки), затем по имени файла
    files.sort(key=lambda p: (p.parent.name, p.name))
    
    merge_files(files, output, title="Компендиум ингредиентов")


def build_bestiary() -> None:
    """Собрать бестиарий."""
    source = VAULT_ROOT / "04_Compendium" / "Бестиарий"
    output = BUILD_DIR / "bestiary_compendium.md"
    
    files = collect_files(source, recursive=True)
    
    # Сортировка по биому, затем по имени файла
    files.sort(key=lambda p: (p.parent.name, p.name))
    
    merge_files(files, output, title="Бестиарий")


def build_biomes() -> None:
    """Собрать компендиум биомов."""
    source = VAULT_ROOT / "04_Compendium" / "Биомы"
    output = BUILD_DIR / "biomes_compendium.md"
    
    files = collect_files(source, recursive=False)
    
    # Сортировка по имени файла
    files.sort(key=lambda p: p.name)
    
    merge_files(files, output, title="Компендиум биомов")


def main() -> None:
    print("🚀 Сборка документации Herbalist...\n")
    ensure_build_dir()

    build_gdd()
    build_glossary()
    build_ingredients()
    build_bestiary()
    build_biomes()

    print("\n✨ Готово! Все файлы сохранены в папку build.")


if __name__ == "__main__":
    main()