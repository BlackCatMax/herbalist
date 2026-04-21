#!/usr/bin/env python3
"""
Унификация документации Herbalist:
- Приведение YAML-frontmatter к единому формату
- Унификация имён биомов
- Удаление эмодзи
- Исправление опечаток в заголовках
- Исправление навигационных ссылок
- Очистка дублирующихся разделителей
"""

import os
import re
from pathlib import Path
from typing import Dict, List, Tuple, Optional
import shutil

# ============================================================================
# КОНФИГУРАЦИЯ
# ============================================================================

ROOT_DIR = Path(".")  # Корень проекта

# Карта унификации имён биомов
BIOME_NAME_MAP = {
    # Варианты -> Единое имя (wikilink)
    "Болото": "[[Болото]]",
    "болото": "[[Болото]]",
    "Тайга": "[[Тайга]]",
    "тайга": "[[Тайга]]",
    "Тундра": "[[Тундра]]",
    "тундра": "[[Тундра]]",
    "Степь": "[[Степь]]",
    "степь": "[[Степь]]",
    "Лесостепь": "[[Лесостепь]]",
    "лесостепь": "[[Лесостепь]]",
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

# Карта унификации стихий
ELEMENT_NAME_MAP = {
    "Вода": "[[Вода]]",
    "вода": "[[Вода]]",
    "Огонь": "[[Огонь]]",
    "огонь": "[[Огонь]]",
    "Земля": "[[Земля]]",
    "земля": "[[Земля]]",
    "Воздух": "[[Воздух]]",
    "воздух": "[[Воздух]]",
}

# Замены для опечаток в заголовках
HEADER_FIXES = {
    "## 📍 Где встретить**": "## 📍 Где встретить",
    "## 🧪 Алхимическое значение**": "## 🧪 Алхимическое значение",
    "## 📍 Где встретить** ": "## 📍 Где встретить",
    "## 🧪 Алхимическое значение** ": "## 🧪 Алхимическое значение",
}

# Эмодзи для удаления (можно расширить)
EMOJI_PATTERN = re.compile(
    "["
    "\U0001F300-\U0001F5FF"  # Разные символы и пиктограммы
    "\U0001F600-\U0001F64F"  # Эмоции
    "\U0001F680-\U0001F6FF"  # Транспорт и карты
    "\U0001F700-\U0001F77F"  # Алхимические символы
    "\U0001F780-\U0001F7FF"  # Геометрические фигуры
    "\U0001F800-\U0001F8FF"  # Доп. стрелки
    "\U0001F900-\U0001F9FF"  # Доп. символы и пиктограммы
    "\U0001FA00-\U0001FA6F"  # Шахматы
    "\U0001FA70-\U0001FAFF"  # Расширенные символы
    "\U00002702-\U000027B0"  # Dingbats
    "\U000024C2-\U0001F251"  # Enclosed characters
    "]+",
    flags=re.UNICODE
)

# Файлы для специальной обработки навигационных ссылок
NAV_LINKS_TO_FIX = {
    "04_Compendium/Растительность/Тундра/Княженика.md": "Тундра",
    "04_Compendium/Растительность/Тундра/Водяника.md": "Тундра",
    "04_Compendium/Растительность/Тайга/Жимолость.md": "Тайга",
    "04_Compendium/Растительность/Тайга/Пихта.md": "Тайга",
    "04_Compendium/Растительность/Степь/Бессмертник.md": "Степь",
}


# ============================================================================
# ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ
# ============================================================================

def remove_emoji(text: str) -> str:
    """Удаляет все эмодзи из текста."""
    return EMOJI_PATTERN.sub("", text)


def fix_headers(text: str) -> str:
    """Исправляет опечатки в заголовках."""
    for old, new in HEADER_FIXES.items():
        text = text.replace(old, new)
    return text


def normalize_yaml_value(value: str, field_name: str) -> str:
    """Приводит значение YAML-поля к единому формату."""
    if field_name == "biome":
        return BIOME_NAME_MAP.get(value, value)
    elif field_name == "element":
        return ELEMENT_NAME_MAP.get(value, value)
    return value


def format_yaml_array(values: List) -> str:
    """Форматирует массив в однострочный YAML."""
    if not values:
        return "[]"
    # Если уже строка с квадратными скобками
    if isinstance(values, str) and values.strip().startswith("["):
        return values.strip()
    # Преобразуем в строку вида [0.1, 0.2, 0.3, 0.4]
    return "[" + ", ".join(str(v).strip() for v in values) + "]"


def parse_yaml_frontmatter(content: str) -> Tuple[Optional[str], str, str]:
    """
    Извлекает YAML-frontmatter из содержимого файла.
    Возвращает: (frontmatter, body, raw_frontmatter_lines)
    """
    lines = content.split("\n")
    
    # Ищем начало frontmatter
    start_idx = -1
    for i, line in enumerate(lines):
        if line.strip() == "---":
            start_idx = i
            break
    
    if start_idx == -1:
        return None, content, ""
    
    # Ищем конец frontmatter
    end_idx = -1
    for i in range(start_idx + 1, len(lines)):
        if lines[i].strip() == "---":
            end_idx = i
            break
    
    if end_idx == -1:
        return None, content, ""
    
    # Пропускаем пустые строки в начале frontmatter
    fm_start = start_idx + 1
    while fm_start < end_idx and lines[fm_start].strip() == "":
        fm_start += 1
    
    raw_fm = "\n".join(lines[start_idx:end_idx + 1])
    body = "\n".join(lines[end_idx + 1:])
    
    # Парсим YAML (простое построчное извлечение)
    fm_dict = {}
    fm_lines = lines[fm_start:end_idx]
    
    current_key = None
    current_list = []
    in_list = False
    
    for line in fm_lines:
        stripped = line.strip()
        if not stripped:
            continue
        
        if ":" in line and not line.lstrip().startswith("-"):
            # Ключ-значение
            if in_list and current_key:
                fm_dict[current_key] = current_list
                in_list = False
                current_list = []
            
            parts = line.split(":", 1)
            key = parts[0].strip()
            value = parts[1].strip() if len(parts) > 1 else ""
            
            if not value:  # Начало списка или вложенного объекта
                current_key = key
                in_list = True
            else:
                fm_dict[key] = value
        elif line.lstrip().startswith("-") and in_list:
            # Элемент списка
            item = line.lstrip()[1:].strip()
            if item:
                current_list.append(item)
    
    if in_list and current_key:
        fm_dict[current_list] = current_list
    
    return fm_dict, body, raw_fm


def rebuild_yaml_frontmatter(fm_dict: Dict) -> str:
    """Перестраивает YAML-frontmatter из словаря с унификацией."""
    lines = ["---"]
    
    # Поля в желаемом порядке
    field_order = [
        "id", "name", "biome", "type", "level", "behavior", "danger",
        "d_base", "d_manifest", "m_base", "morok_affinity",
        "potency", "purity", "stability", "resonance", "corruption", "distortion",
        "element", "tags", "image", "status", "aliases"
    ]
    
    # Сначала выводим поля в заданном порядке
    for key in field_order:
        if key in fm_dict:
            value = fm_dict[key]
            
            # Унификация значений
            if key == "biome":
                value = BIOME_NAME_MAP.get(value, value)
            elif key == "element":
                value = ELEMENT_NAME_MAP.get(value, value)
            elif key in ("d_base", "d_manifest"):
                # Преобразуем в однострочный массив
                if isinstance(value, list):
                    value = format_yaml_array(value)
                elif isinstance(value, str) and not value.startswith("["):
                    # Пытаемся распарсить строку как массив
                    try:
                        parts = [v.strip() for v in value.split(",")]
                        value = format_yaml_array(parts)
                    except:
                        pass
            elif key == "tags" and isinstance(value, list):
                value = "[" + ", ".join(value) + "]"
            elif key == "image" and value.startswith("![["):
                # Убираем ![[ ]] из поля image
                value = value.replace("![[", "").replace("]]", "")
            
            lines.append(f"{key}: {value}")
            del fm_dict[key]
    
    # Остальные поля
    for key, value in fm_dict.items():
        lines.append(f"{key}: {value}")
    
    lines.append("---")
    return "\n".join(lines)


def process_file(filepath: Path) -> bool:
    """Обрабатывает один файл: исправляет YAML, заголовки, эмодзи."""
    try:
        with open(filepath, "r", encoding="utf-8") as f:
            content = f.read()
        
        original_content = content
        
        # 1. Удаляем эмодзи
        content = remove_emoji(content)
        
        # 2. Исправляем опечатки в заголовках
        content = fix_headers(content)
        
        # 3. Обрабатываем YAML-frontmatter
        fm_dict, body, raw_fm = parse_yaml_frontmatter(content)
        
        if fm_dict is not None:
            new_fm = rebuild_yaml_frontmatter(fm_dict)
            content = new_fm + "\n" + body
        
        # 4. Специальная обработка для Осина.md
        if filepath.name == "Осина.md":
            lines = content.split("\n")
            # Удаляем дублирующиеся --- в начале
            cleaned_lines = []
            dash_count = 0
            for line in lines:
                if line.strip() == "---" and dash_count < 2:
                    dash_count += 1
                    cleaned_lines.append(line)
                elif dash_count >= 2 and line.strip() == "---":
                    continue  # Пропускаем лишние ---
                else:
                    cleaned_lines.append(line)
            content = "\n".join(cleaned_lines)
        
        # 5. Исправляем навигационные ссылки
        rel_path = str(filepath).replace("\\", "/")
        if rel_path in NAV_LINKS_TO_FIX:
            biome = NAV_LINKS_TO_FIX[rel_path]
            # Ищем и заменяем https://... ссылки
            pattern = r'_← \[К списку ингредиентов ' + biome + r'\]\(https://[^)]+\)_'
            replacement = f'← *[[{biome}|К списку ингредиентов {biome}]]*'
            content = re.sub(pattern, replacement, content)
        
        # Записываем только если были изменения
        if content != original_content:
            with open(filepath, "w", encoding="utf-8") as f:
                f.write(content)
            return True
        
        return False
        
    except Exception as e:
        print(f"Ошибка при обработке {filepath}: {e}")
        return False


def rename_file(old_path: Path, new_name: str) -> bool:
    """Переименовывает файл."""
    new_path = old_path.parent / new_name
    if old_path != new_path:
        old_path.rename(new_path)
        print(f"Переименован: {old_path.name} -> {new_name}")
        return True
    return False


# ============================================================================
# ОСНОВНЫЕ ФУНКЦИИ ОБРАБОТКИ
# ============================================================================

def process_all_md_files():
    """Обрабатывает все .md файлы в проекте."""
    print("\n" + "=" * 60)
    print("ОБРАБОТКА ВСЕХ .md ФАЙЛОВ")
    print("=" * 60)
    
    processed = 0
    changed = 0
    
    # Папки для обработки (исключаем .obsidian и build)
    include_dirs = ["00_Meta", "01_Glossary", "02_GDD", "03_Technical", "04_Compendium"]
    
    for include_dir in include_dirs:
        dir_path = ROOT_DIR / include_dir
        if not dir_path.exists():
            continue
        
        for filepath in dir_path.rglob("*.md"):
            processed += 1
            if process_file(filepath):
                changed += 1
                print(f"  Исправлен: {filepath}")
    
    print(f"\nОбработано файлов: {processed}")
    print(f"Изменено файлов: {changed}")


def fix_special_cases():
    """Обрабатывает особые случаи."""
    print("\n" + "=" * 60)
    print("ОБРАБОТКА ОСОБЫХ СЛУЧАЕВ")
    print("=" * 60)
    
    # 1. Переименование жар-птица.md
    old_path = ROOT_DIR / "04_Compendium/Bestiary/Смешанный лес/жар-птица.md"
    if old_path.exists():
        rename_file(old_path, "Жар-птица.md")
    
    # 2. Явная обработка Осина.md (дополнительная)
    osina_path = ROOT_DIR / "04_Compendium/Растительность/Смешанный лес/Осина.md"
    if osina_path.exists():
        with open(osina_path, "r", encoding="utf-8") as f:
            content = f.read()
        
        # Удаляем эмодзи
        content = remove_emoji(content)
        
        # Убираем лишние --- в начале
        lines = content.split("\n")
        if len(lines) >= 3 and lines[0].strip() == "---" and lines[1].strip() == "---" and lines[2].strip() == "---":
            lines = lines[2:]
            content = "\n".join(lines)
            
            with open(osina_path, "w", encoding="utf-8") as f:
                f.write(content)
            print(f"  Исправлен дублирующийся frontmatter: {osina_path}")


def create_backup():
    """Создаёт резервную копию перед изменениями."""
    backup_dir = ROOT_DIR / ".backup_before_unification"
    if backup_dir.exists():
        shutil.rmtree(backup_dir)
    
    print("\nСоздание резервной копии...")
    
    for include_dir in ["00_Meta", "01_Glossary", "02_GDD", "03_Technical", "04_Compendium"]:
        src = ROOT_DIR / include_dir
        if src.exists():
            dst = backup_dir / include_dir
            shutil.copytree(src, dst)
    
    print(f"Резервная копия создана в: {backup_dir}")


def print_summary():
    """Выводит итоговую статистику."""
    print("\n" + "=" * 60)
    print("ИТОГИ УНИФИКАЦИИ")
    print("=" * 60)
    print("""
Выполненные действия:
1. Удалены все эмодзи из заголовков и текста
2. Исправлены опечатки:
   - "## 📍 Где встретить**" -> "## Где встретить"
   - "## 🧪 Алхимическое значение**" -> "## Алхимическое значение"
3. Унифицированы YAML-frontmatter:
   - biome: приведены к формату [[Биом]]
   - element: приведены к формату [[Стихия]]
   - d_base / d_manifest: приведены к однострочным массивам
   - image: убраны ![[ ]]
4. Исправлен файл Осина.md (удалены дублирующиеся ---)
5. Исправлены навигационные ссылки (https:// -> wikilink)
6. Переименован файл жар-птица.md -> Жар-птица.md

Рекомендуется проверить:
- 02_GDD/00_Core_Lock.md — ссылки на Core_Current, Alchemy_Current
- Запустить check_docs.py для проверки битых ссылок
""")


# ============================================================================
# ТОЧКА ВХОДА
# ============================================================================

def main():
    """Главная функция."""
    import argparse
    
    parser = argparse.ArgumentParser(description="Унификация документации Herbalist")
    parser.add_argument("--backup", action="store_true", help="Создать резервную копию перед изменениями")
    parser.add_argument("--dry-run", action="store_true", help="Только проверка, без изменений")
    
    args = parser.parse_args()
    
    print("\n" + "█" * 60)
    print("█" + " " * 58 + "█")
    print("█" + "   УНИФИКАЦИЯ ДОКУМЕНТАЦИИ HERBALIST   ".center(58) + "█")
    print("█" + " " * 58 + "█")
    print("█" * 60)
    
    if args.dry_run:
        print("\n[РЕЖИМ ПРОВЕРКИ] Изменения НЕ будут сохранены")
        return
    
    if args.backup:
        create_backup()
    
    fix_special_cases()
    process_all_md_files()
    print_summary()


if __name__ == "__main__":
    main()