#!/usr/bin/env python3
"""
HERBALIST DOCUMENTATION MEGA-SCRIPT
Объединяет все проверки и исправления документации.

Возможности:
- Проверка структуры и чистоты файлов
- Исправление YAML-frontmatter
- Удаление эмодзи
- Исправление опечаток
- Унификация терминологии
- Исправление битых ссылок
- Генерация полного отчёта
- Создание резервной копии

Использование:
    python mega_docs.py --all           # Полная обработка
    python mega_docs.py --check         # Только проверка
    python mega_docs.py --fix           # Только исправление
    python mega_docs.py --report        # Генерация отчёта
    python mega_docs.py --backup        # Создание бэкапа
"""

import os
import re
import shutil
import argparse
from pathlib import Path
from typing import Dict, List, Tuple, Optional, Set
from datetime import datetime
from dataclasses import dataclass, field
from enum import Enum

# ============================================================================
# КОНФИГУРАЦИЯ
# ============================================================================

ROOT_DIR = Path(".")

# Папки для обработки
INCLUDE_DIRS = ["00_Meta", "01_Glossary", "02_GDD", "03_Technical", "04_Compendium"]

# Исключаемые папки
EXCLUDE_DIRS = [".obsidian", ".backup_*", ".translit_backup", "build", ".git", "__pycache__"]

# Папки с изображениями
ASSETS_DIRS = ["05_Assets"]

# ============================================================================
# КАРТЫ УНИФИКАЦИИ
# ============================================================================

BIOME_NAME_MAP = {
    "Болото": "[[Болото]]", "болото": "[[Болото]]",
    "Тайга": "[[Тайга]]", "тайга": "[[Тайга]]",
    "Тундра": "[[Тундра]]", "тундра": "[[Тундра]]",
    "Степь": "[[Степь]]", "степь": "[[Степь]]",
    "Лесостепь": "[[Лесостепь]]", "лесостепь": "[[Лесостепь]]",
    "Смешанные леса": "[[Смешанный лес]]",
    "Смешанный лес": "[[Смешанный лес]]",
    "смешанный лес": "[[Смешанный лес]]",
    "Широколиственные леса": "[[Широколиственный лес]]",
    "Широколиственный лес": "[[Широколиственный лес]]",
    "широколиственный лес": "[[Широколиственный лес]]",
    "Речная пойма": "[[Речная пойма]]",
    "Пойма": "[[Речная пойма]]",
    "Пойма (речная)": "[[Речная пойма]]",
    "пойма": "[[Речная пойма]]",
    "Повсеместно": "[[Повсеместно]]",
    "повсеместно": "[[Повсеместно]]",
}

ELEMENT_NAME_MAP = {
    "Вода": "[[Вода]]", "вода": "[[Вода]]",
    "Огонь": "[[Огонь]]", "огонь": "[[Огонь]]",
    "Земля": "[[Земля]]", "земля": "[[Земля]]",
    "Воздух": "[[Воздух]]", "воздух": "[[Воздух]]",
}

TERM_UNIFICATION = {
    "Морок": "[[Morok]]", "Мороком": "[[Morok|Мороком]]",
    "Морока": "[[Morok|Морока]]", "морок": "[[Morok]]",
    "Заряна": "[[Zaryana]]", "Заряны": "[[Zaryana|Заряны]]",
    "Алатырь": "[[S0|Алатырь]]", "Алатыря": "[[S0|Алатыря]]",
    "эталонное состояние": "[[S0|эталонное состояние]]",
    "намерение": "[[Intent]]", "намерения": "[[Intent|намерения]]",
    "состояние биома": "[[BiomeState]]",
}

HEADER_FIXES = {
    "## 📍 Где встретить**": "## Где встретить",
    "## 🧪 Алхимическое значение**": "## Алхимическое значение",
}

EMOJI_PATTERN = re.compile(
    "["
    "\U0001F300-\U0001F5FF"
    "\U0001F600-\U0001F64F"
    "\U0001F680-\U0001F6FF"
    "\U0001F700-\U0001F77F"
    "\U0001F780-\U0001F7FF"
    "\U0001F800-\U0001F8FF"
    "\U0001F900-\U0001F9FF"
    "\U0001FA00-\U0001FA6F"
    "\U0001FA70-\U0001FAFF"
    "\U00002702-\U000027B0"
    "\U000024C2-\U0001F251"
    "]+", flags=re.UNICODE
)

# ============================================================================
# ТИПЫ ОШИБОК
# ============================================================================

class ErrorType(Enum):
    EMPTY_FILE = "🚫 ПУСТОЙ ФАЙЛ"
    BROKEN_LINK = "🔗 БИТАЯ ССЫЛКА"
    MISSING_IMAGE = "🖼️ НЕТ ИЗОБРАЖЕНИЯ"
    INVALID_YAML = "📋 НЕВАЛИДНЫЙ YAML"
    DUPLICATE_FRONTMATTER = "📄 ДУБЛИКАТ FRONTMATTER"
    EMOJI_IN_HEADER = "😊 ЭМОДЗИ В ЗАГОЛОВКЕ"
    TYPO_IN_HEADER = "✏️ ОПЕЧАТКА В ЗАГОЛОВКЕ"
    INCONSISTENT_BIOME = "🌍 НЕКОНСИСТЕНТНЫЙ БИОМ"
    INCONSISTENT_ELEMENT = "💧 НЕКОНСИСТЕНТНАЯ СТИХИЯ"
    FOLDER_LINK = "📁 ССЫЛКА НА ПАПКУ"


@dataclass
class Error:
    """Ошибка в документации."""
    type: ErrorType
    file: str
    line: Optional[int] = None
    detail: Optional[str] = None
    fixed: bool = False
    
    def __str__(self):
        loc = f"{self.file}" + (f":{self.line}" if self.line else "")
        detail = f" - {self.detail}" if self.detail else ""
        status = " ✅" if self.fixed else ""
        return f"[{self.type.value}] {loc}{detail}{status}"


@dataclass
class Report:
    """Отчёт о проверке."""
    timestamp: datetime = field(default_factory=datetime.now)
    files_checked: int = 0
    errors: List[Error] = field(default_factory=list)
    warnings: List[str] = field(default_factory=list)
    actions_performed: List[str] = field(default_factory=list)
    
    @property
    def error_count(self) -> int:
        return len([e for e in self.errors if not e.fixed])
    
    @property
    def fixed_count(self) -> int:
        return len([e for e in self.errors if e.fixed])
    
    @property
    def errors_by_type(self) -> Dict[ErrorType, int]:
        counts = {}
        for e in self.errors:
            counts[e.type] = counts.get(e.type, 0) + 1
        return counts


# ============================================================================
# ОСНОВНОЙ КЛАСС
# ============================================================================

class HerbalistDocChecker:
    """Главный класс для проверки и исправления документации."""
    
    def __init__(self, root_dir: Path = ROOT_DIR, fix: bool = False, backup: bool = False):
        self.root_dir = root_dir
        self.fix_mode = fix
        self.backup_mode = backup
        self.report = Report()
        self.all_files: List[Path] = []
        self.file_index: Dict[str, Path] = {}
        
    def run(self) -> Report:
        """Запускает полную проверку и исправление."""
        print("\n" + "█" * 70)
        print("█" + " " * 68 + "█")
        print("█" + "   HERBALIST DOCUMENTATION MEGA-CHECKER   ".center(68) + "█")
        print("█" + " " * 68 + "█")
        print("█" * 70)
        
        mode_str = "ИСПРАВЛЕНИЕ" if self.fix_mode else "ПРОВЕРКА"
        print(f"\n🔧 Режим: {mode_str}")
        
        if self.backup_mode:
            self._create_backup()
        
        self._scan_all_files()
        self._check_empty_files()
        self._check_yaml_frontmatter()
        self._check_headers()
        self._check_biome_consistency()
        self._check_element_consistency()
        self._check_broken_links()
        self._check_missing_images()
        self._check_folder_links()
        self._unify_terminology()
        
        self._generate_report_file()
        
        return self.report
    
    def _create_backup(self):
        """Создаёт резервную копию."""
        backup_dir = self.root_dir / f".backup_{datetime.now().strftime('%Y%m%d_%H%M%S')}"
        print(f"\n💾 Создание резервной копии: {backup_dir}")
        
        for dir_name in INCLUDE_DIRS + ASSETS_DIRS:
            src = self.root_dir / dir_name
            if src.exists():
                dst = backup_dir / dir_name
                shutil.copytree(src, dst)
        
        self.report.actions_performed.append(f"Создана резервная копия: {backup_dir}")
        print("   ✅ Готово")
    
    def _scan_all_files(self):
        """Сканирует все .md файлы."""
        print("\n📂 Сканирование файлов...")
        
        for dir_name in INCLUDE_DIRS:
            dir_path = self.root_dir / dir_name
            if not dir_path.exists():
                continue
            
            for filepath in dir_path.rglob("*.md"):
                # Пропускаем исключаемые папки
                if any(excl in filepath.parts for excl in EXCLUDE_DIRS if "*" not in excl):
                    continue
                
                self.all_files.append(filepath)
                rel_path = str(filepath.relative_to(self.root_dir)).replace("\\", "/")
                self.file_index[filepath.stem] = filepath
                self.file_index[rel_path] = filepath
        
        self.report.files_checked = len(self.all_files)
        print(f"   Найдено файлов: {len(self.all_files)}")
    
    def _check_empty_files(self):
        """Проверяет пустые файлы."""
        print("\n🚫 Проверка пустых файлов...")
        
        for filepath in self.all_files:
            try:
                with open(filepath, "r", encoding="utf-8") as f:
                    content = f.read()
                
                if len(content.strip()) == 0:
                    error = Error(
                        type=ErrorType.EMPTY_FILE,
                        file=str(filepath.relative_to(self.root_dir))
                    )
                    self.report.errors.append(error)
                    
                    if self.fix_mode:
                        filepath.unlink()
                        error.fixed = True
                        print(f"   ✅ Удалён: {filepath.name}")
                    else:
                        print(f"   ⚠️ Пустой файл: {filepath.name}")
            except Exception as e:
                self.report.warnings.append(f"Ошибка чтения {filepath}: {e}")
    
    def _check_yaml_frontmatter(self):
        """Проверяет и исправляет YAML-frontmatter."""
        print("\n📋 Проверка YAML-frontmatter...")
        
        for filepath in self.all_files:
            with open(filepath, "r", encoding="utf-8") as f:
                content = f.read()
            
            original = content
            
            # Проверка на дублирующиеся ---
            lines = content.split("\n")
            if len(lines) >= 3 and lines[0].strip() == "---" and lines[1].strip() == "---":
                error = Error(
                    type=ErrorType.DUPLICATE_FRONTMATTER,
                    file=str(filepath.relative_to(self.root_dir))
                )
                self.report.errors.append(error)
                
                if self.fix_mode:
                    lines = lines[2:]
                    content = "\n".join(lines)
                    error.fixed = True
            
            # Унификация YAML
            if self.fix_mode:
                content = self._fix_yaml_content(content, filepath)
            
            if content != original and self.fix_mode:
                with open(filepath, "w", encoding="utf-8") as f:
                    f.write(content)
                print(f"   ✅ Исправлен YAML: {filepath.name}")
    
    def _fix_yaml_content(self, content: str, filepath: Path) -> str:
        """Исправляет YAML в содержимом файла."""
        lines = content.split("\n")
        
        # Ищем границы frontmatter
        start = -1
        end = -1
        for i, line in enumerate(lines):
            if line.strip() == "---":
                if start == -1:
                    start = i
                elif end == -1:
                    end = i
                    break
        
        if start == -1 or end == -1:
            return content
        
        # Исправляем строки внутри frontmatter
        for i in range(start + 1, end):
            line = lines[i]
            
            # biome
            if line.startswith("biome:"):
                for old, new in BIOME_NAME_MAP.items():
                    if old in line:
                        lines[i] = line.replace(old, new)
                        break
            
            # element
            if line.startswith("element:"):
                for old, new in ELEMENT_NAME_MAP.items():
                    if old in line:
                        lines[i] = line.replace(old, new)
                        break
            
            # d_base / d_manifest - приводим к однострочному массиву
            if "d_base:" in line or "d_manifest:" in line:
                # Собираем многострочный массив
                if "[" not in line:
                    array_lines = [line]
                    j = i + 1
                    while j < end and ("-" in lines[j] or lines[j].strip().startswith(("-", "0", "1", "2", "3", "4", "5", "6", "7", "8", "9"))):
                        array_lines.append(lines[j])
                        j += 1
                    
                    # Парсим значения
                    values = []
                    for al in array_lines:
                        nums = re.findall(r"[\d.]+", al)
                        values.extend(nums)
                    
                    if values:
                        lines[i] = f"{line.split(':')[0]}: [{', '.join(values[:4])}]"
                        # Удаляем остальные строки
                        for k in range(i + 1, j):
                            lines[k] = ""
        
        return "\n".join(lines)
    
    def _check_headers(self):
        """Проверяет заголовки на опечатки и эмодзи."""
        print("\n✏️ Проверка заголовков...")
        
        for filepath in self.all_files:
            with open(filepath, "r", encoding="utf-8") as f:
                content = f.read()
            
            original = content
            lines = content.split("\n")
            
            for i, line in enumerate(lines):
                # Проверка эмодзи
                if EMOJI_PATTERN.search(line) and line.strip().startswith("#"):
                    error = Error(
                        type=ErrorType.EMOJI_IN_HEADER,
                        file=str(filepath.relative_to(self.root_dir)),
                        line=i + 1,
                        detail=line.strip()[:50]
                    )
                    self.report.errors.append(error)
                    
                    if self.fix_mode:
                        lines[i] = EMOJI_PATTERN.sub("", line)
                        error.fixed = True
                
                # Проверка опечаток
                for old, new in HEADER_FIXES.items():
                    if old in line:
                        error = Error(
                            type=ErrorType.TYPO_IN_HEADER,
                            file=str(filepath.relative_to(self.root_dir)),
                            line=i + 1,
                            detail=old
                        )
                        self.report.errors.append(error)
                        
                        if self.fix_mode:
                            lines[i] = line.replace(old, new)
                            error.fixed = True
            
            if self.fix_mode:
                content = "\n".join(lines)
                if content != original:
                    with open(filepath, "w", encoding="utf-8") as f:
                        f.write(content)
    
    def _check_biome_consistency(self):
        """Проверяет консистентность имён биомов."""
        print("\n🌍 Проверка имён биомов...")
        
        inconsistent = ["Смешанные леса", "Широколиственные леса", "Пойма", "Пойма (речная)"]
        
        for filepath in self.all_files:
            with open(filepath, "r", encoding="utf-8") as f:
                content = f.read()
            
            for old in inconsistent:
                if old in content and "[[BiomeState]]" not in content:
                    error = Error(
                        type=ErrorType.INCONSISTENT_BIOME,
                        file=str(filepath.relative_to(self.root_dir)),
                        detail=old
                    )
                    self.report.errors.append(error)
    
    def _check_element_consistency(self):
        """Проверяет консистентность стихий."""
        print("\n💧 Проверка стихий...")
        
        for filepath in self.all_files:
            with open(filepath, "r", encoding="utf-8") as f:
                content = f.read()
            
            # Ищем element: Вода (без скобок)
            if re.search(r'element:\s*(Вода|Огонь|Земля|Воздух)\s*$', content, re.MULTILINE):
                error = Error(
                    type=ErrorType.INCONSISTENT_ELEMENT,
                    file=str(filepath.relative_to(self.root_dir))
                )
                self.report.errors.append(error)
    
    def _check_broken_links(self):
        """Проверяет битые wikilinks."""
        print("\n🔗 Проверка wikilinks...")
        
        for filepath in self.all_files:
            with open(filepath, "r", encoding="utf-8") as f:
                content = f.read()
            
            links = self._extract_wikilinks(content)
            
            for link, line_num in links:
                if link.startswith("#") or link.startswith("http"):
                    continue
                
                if link.endswith((".png", ".jpg", ".jpeg", ".gif", ".webp")):
                    continue
                
                if link == "Новый термин":  # Плейсхолдер шаблона
                    continue
                
                target = self._find_file_by_wikilink(link, filepath)
                
                if target is None:
                    error = Error(
                        type=ErrorType.BROKEN_LINK,
                        file=str(filepath.relative_to(self.root_dir)),
                        line=line_num,
                        detail=link
                    )
                    self.report.errors.append(error)
    
    def _extract_wikilinks(self, content: str) -> List[Tuple[str, int]]:
        """Извлекает wikilinks с номерами строк."""
        links = []
        lines = content.split("\n")
        pattern = re.compile(r"\[\[([^\]]+)\]\]")
        
        for i, line in enumerate(lines, 1):
            for match in pattern.finditer(line):
                link = match.group(1).split("|")[0].split("#")[0].strip()
                links.append((link, i))
        
        return links
    
    def _find_file_by_wikilink(self, link: str, current_file: Path) -> Optional[Path]:
        """Находит файл по wikilink."""
        clean_link = link.split("|")[0].split("#")[0].strip()
        
        # Прямой путь
        search_path = self.root_dir / f"{clean_link}.md"
        if search_path.exists():
            return search_path
        
        # Относительно текущего файла
        rel_path = current_file.parent / f"{clean_link}.md"
        if rel_path.exists():
            return rel_path
        
        # Поиск по имени
        filename = f"{clean_link}.md"
        if filename in self.file_index:
            return self.file_index[filename]
        
        # Поиск по стему (для коротких ссылок)
        stem = clean_link.split("/")[-1]
        if stem in self.file_index:
            return self.file_index[stem]
        
        return None
    
    def _check_missing_images(self):
        """Проверяет отсутствующие изображения."""
        print("\n🖼️ Проверка изображений...")
        
        for filepath in self.all_files:
            with open(filepath, "r", encoding="utf-8") as f:
                content = f.read()
            
            # Ищем ссылки на изображения
            pattern = re.compile(r"!\[\[(.*?\.(?:png|jpg|jpeg|gif|webp))(?:\|.*?)?\]\]")
            
            for match in pattern.finditer(content):
                img_name = match.group(1)
                
                # Ищем изображение
                img_found = False
                for assets_dir in ASSETS_DIRS:
                    img_path = self.root_dir / assets_dir
                    if list(img_path.rglob(img_name)):
                        img_found = True
                        break
                
                if not img_found:
                    # Ищем по строке в content для определения номера строки
                    line_num = content[:match.start()].count("\n") + 1
                    error = Error(
                        type=ErrorType.MISSING_IMAGE,
                        file=str(filepath.relative_to(self.root_dir)),
                        line=line_num,
                        detail=img_name
                    )
                    self.report.errors.append(error)
                    
                    if self.fix_mode:
                        self.report.actions_performed.append(f"Пропущено изображение: {img_name}")
    
    def _check_folder_links(self):
        """Проверяет ссылки на папки."""
        print("\n📁 Проверка ссылок на папки...")
        
        folder_patterns = [
            r"\[\[04_Compendium/Биомы/",
            r"\[\[04_Compendium/Bestiary/",
            r"\[\[04_Compendium/Растительность/",
            r"\[\[03_Technical/Current/",
        ]
        
        for filepath in self.all_files:
            with open(filepath, "r", encoding="utf-8") as f:
                content = f.read()
            
            lines = content.split("\n")
            
            for i, line in enumerate(lines, 1):
                for pattern in folder_patterns:
                    if re.search(pattern, line):
                        error = Error(
                            type=ErrorType.FOLDER_LINK,
                            file=str(filepath.relative_to(self.root_dir)),
                            line=i,
                            detail=line.strip()[:50]
                        )
                        self.report.errors.append(error)
                        
                        if self.fix_mode:
                            self._fix_folder_link(filepath, line)
                            error.fixed = True
    
    def _fix_folder_link(self, filepath: Path, line: str):
        """Исправляет ссылку на папку."""
        replacements = {
            "[[04_Compendium/Биомы/": "[[04_Compendium/Биомы/Болото|Биомы]]",
            "[[04_Compendium/Bestiary/": "[[04_Compendium/Bestiary/Болото/Болотник|Бестиарий]]",
            "[[04_Compendium/Растительность/": "[[04_Compendium/Растительность/Болото/Багульник|Ингредиенты]]",
            "[[03_Technical/Current/": "[[03_Technical/Current/Core_Current|Техническая документация]]",
        }
        
        with open(filepath, "r", encoding="utf-8") as f:
            content = f.read()
        
        for old, new in replacements.items():
            content = content.replace(old, new)
        
        with open(filepath, "w", encoding="utf-8") as f:
            f.write(content)
    
    def _unify_terminology(self):
        """Унифицирует терминологию в тексте."""
        if not self.fix_mode:
            return
        
        print("\n📝 Унификация терминологии...")
        
        for filepath in self.all_files:
            with open(filepath, "r", encoding="utf-8") as f:
                content = f.read()
            
            original = content
            
            for old, new in TERM_UNIFICATION.items():
                # Не заменяем внутри wikilinks
                pattern = rf'(?<!\[\[){re.escape(old)}(?!\]\])'
                content = re.sub(pattern, new, content)
            
            if content != original:
                with open(filepath, "w", encoding="utf-8") as f:
                    f.write(content)
                print(f"   ✅ {filepath.name}")
    
    def _generate_report_file(self):
        """Генерирует файл отчёта."""
        report_path = self.root_dir / "docs_check_report.md"
        
        lines = [
            "# Отчёт проверки документации Herbalist",
            "",
            f"**Дата и время:** {self.report.timestamp.strftime('%Y-%m-%d %H:%M:%S')}",
            "",
            f"**Корневая папка:** `{self.root_dir.absolute()}`",
            "",
            f"**Режим:** {'ИСПРАВЛЕНИЕ' if self.fix_mode else 'ПРОВЕРКА'}",
            "",
            f"**Проверено файлов `.md`:** {self.report.files_checked}",
            "",
            "## 📊 Сводка",
            "",
            f"| Показатель | Значение |",
            f"|------------|----------|",
            f"| Всего ошибок | {len(self.report.errors)} |",
            f"| Исправлено | {self.report.fixed_count} |",
            f"| Осталось | {self.report.error_count} |",
            f"| Предупреждений | {len(self.report.warnings)} |",
            "",
        ]
        
        # Статистика по типам ошибок
        if self.report.errors_by_type:
            lines.extend([
                "### По типам ошибок",
                "",
                "| Тип | Количество |",
                "|-----|------------|",
            ])
            for error_type, count in self.report.errors_by_type.items():
                lines.append(f"| {error_type.value} | {count} |")
            lines.append("")
        
        # Список ошибок
        if self.report.errors:
            lines.extend([
                "## ❌ Список ошибок",
                "",
            ])
            for error in sorted(self.report.errors, key=lambda e: (e.type.value, e.file)):
                lines.append(f"- {error}")
            lines.append("")
        else:
            lines.extend([
                "## ✅ Ошибок не найдено",
                "",
            ])
        
        # Предупреждения
        if self.report.warnings:
            lines.extend([
                "## ⚠️ Предупреждения",
                "",
            ])
            for warning in self.report.warnings:
                lines.append(f"- {warning}")
            lines.append("")
        
        # Выполненные действия
        if self.report.actions_performed:
            lines.extend([
                "## 🔧 Выполненные действия",
                "",
            ])
            for action in self.report.actions_performed:
                lines.append(f"- {action}")
            lines.append("")
        
        lines.extend([
            "---",
            "*Отчёт сгенерирован автоматически mega-docs.py*",
        ])
        
        with open(report_path, "w", encoding="utf-8") as f:
            f.write("\n".join(lines))
        
        print(f"\n📄 Отчёт сохранён: {report_path}")


# ============================================================================
# ТОЧКА ВХОДА
# ============================================================================

def main():
    parser = argparse.ArgumentParser(
        description="Herbalist Documentation Mega-Checker",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Примеры:
  python mega_docs.py --all      # Полная проверка и исправление с бэкапом
  python mega_docs.py --check    # Только проверка (без изменений)
  python mega_docs.py --fix      # Только исправление (без бэкапа)
  python mega_docs.py --report   # Только генерация отчёта
        """
    )
    
    parser.add_argument("--all", action="store_true", help="Полная обработка (fix + backup + report)")
    parser.add_argument("--check", action="store_true", help="Только проверка")
    parser.add_argument("--fix", action="store_true", help="Только исправление")
    parser.add_argument("--backup", action="store_true", help="Создать бэкап перед исправлением")
    parser.add_argument("--report", action="store_true", help="Только генерация отчёта")
    
    args = parser.parse_args()
    
    # Определяем режим
    if args.all:
        fix_mode = True
        backup_mode = True
    elif args.check:
        fix_mode = False
        backup_mode = False
    elif args.fix:
        fix_mode = True
        backup_mode = args.backup
    elif args.report:
        # Только читаем существующий отчёт
        fix_mode = False
        backup_mode = False
    else:
        parser.print_help()
        return
    
    if not args.report:
        checker = HerbalistDocChecker(fix=fix_mode, backup=backup_mode)
        report = checker.run()
        
        print("\n" + "=" * 70)
        print("📊 ИТОГИ")
        print("=" * 70)
        print(f"   Проверено файлов: {report.files_checked}")
        print(f"   Найдено ошибок:  {len(report.errors)}")
        print(f"   Исправлено:       {report.fixed_count}")
        print(f"   Осталось:         {report.error_count}")
        print("=" * 70)
    else:
        # Просто показываем существующий отчёт
        report_path = ROOT_DIR / "docs_check_report.md"
        if report_path.exists():
            with open(report_path, "r", encoding="utf-8") as f:
                print(f.read())
        else:
            print("❌ Отчёт не найден. Запустите с --check или --all")


if __name__ == "__main__":
    main()