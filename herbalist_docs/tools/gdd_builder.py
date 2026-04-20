# -*- coding: utf-8 -*-
"""
Herbalist GDD Builder
Сборка design-документа в единый файл
"""

import os

# --------------------------------------------------
# PATHS
# --------------------------------------------------

BASE_PATH = r"K:\herbalist\docs\src\design"
OUTPUT_PATH = r"K:\herbalist\docs\build"
OUTPUT_FILE = os.path.join(OUTPUT_PATH, "design_full.md")

# --------------------------------------------------
# FILE ORDER (ЖЁСТКИЙ ПОРЯДОК)
# --------------------------------------------------

FILES = [
    "00_core_lock.md",
    "01-introduction.md",
    "02-setting.md",
    "03-narrative.md",
    "04-game-loop.md",
    "05-systems.md",
    "06-progression.md",
    "07-ux.md",
    "08-tables.md",
    "09-exp_dynamic.md",
    "10-intent-evolution.md",
    "11-entity-dynamics.md",
    "12-biome-change.md",
    "13-world-pipeline.md",
]

# --------------------------------------------------
# SETTINGS
# --------------------------------------------------

ADD_HEADERS = True
SEPARATOR = "\n\n---\n\n"

# --------------------------------------------------
# CORE
# --------------------------------------------------

def read_file(path: str) -> str:
    if not os.path.exists(path):
        raise FileNotFoundError(f"❌ Файл не найден: {path}")

    with open(path, "r", encoding="utf-8") as f:
        return f.read().strip()


def build():
    print("🔧 Сборка GDD...\n")

    parts = []

    for filename in FILES:
        full_path = os.path.join(BASE_PATH, filename)

        print(f"[+] {filename}")

        content = read_file(full_path)

        if ADD_HEADERS:
            header = f"\n\n<!-- ===== {filename} ===== -->\n\n"
            parts.append(header)

        parts.append(content)

    final_text = SEPARATOR.join(parts)

    os.makedirs(OUTPUT_PATH, exist_ok=True)

    with open(OUTPUT_FILE, "w", encoding="utf-8") as f:
        f.write(final_text)

    print("\n✅ Готово:")
    print(OUTPUT_FILE)


# --------------------------------------------------
# ENTRY
# --------------------------------------------------

if __name__ == "__main__":
    build()