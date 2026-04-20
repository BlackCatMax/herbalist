# -*- coding: utf-8 -*-
"""
Herbalist — Technical Builder

Собирает полный technical_full.md из исходных .md файлов
в заданном порядке.

Source:
K:\herbalist\docs\src\technical\

Output:
K:\herbalist\docs\build\technical_full.md
"""

import os
from pathlib import Path

# ----------------------------------------------------------------------
# CONFIG
# ----------------------------------------------------------------------

SRC_DIR = Path(r"K:\herbalist\docs\src\technical")
OUT_FILE = Path(r"K:\herbalist\docs\build\technical_full.md")

# Строгий порядок сборки
FILES_ORDER = [
    "00_CORE.md",
    "01_CORE_structure.md",
    "02_CORE_pipeline.md",
    "03_fold.md",
    "04_delta.md",
    "05_context.md",
    "07_water.md",
    "08_axis_sign_modifiers.md",
    "09_interaction_rules.md",
    "10_morok.md",
    "11_delta_apply.md",
    "12_conflict_and_collapse.md",
    "13_zaryana.md",
    "14_final_clamp.md",
    "15_global_invariants.md",
    "16_biome_field_state.md",
    "17_state_gradients.md",
    "18_harvesting.md",
    "19_apply_alchemy_results.md",
    "20_diffusion.md",
    "21_shrines.md",
    "22_entities.md",
    "23_progression.md",
    "24_storage_inventiry_decay.md",
    "25_game_loop.md",
]

SEPARATOR = "\n\n---\n\n"


# ----------------------------------------------------------------------
# CORE LOGIC
# ----------------------------------------------------------------------

def read_file(path: Path) -> str:
    """Читает файл с базовой защитой."""
    if not path.exists():
        raise FileNotFoundError(f"[ERROR] File not found: {path}")
    
    with open(path, "r", encoding="utf-8") as f:
        return f.read().strip()


def build_document() -> str:
    """Собирает финальный документ."""
    parts = []

    print("=== TECHNICAL BUILD START ===")

    for filename in FILES_ORDER:
        file_path = SRC_DIR / filename

        print(f"[LOAD] {filename}")

        content = read_file(file_path)

        parts.append(content)

    print("=== ALL FILES LOADED ===")

    return SEPARATOR.join(parts)


def write_output(content: str):
    """Записывает результат."""
    OUT_FILE.parent.mkdir(parents=True, exist_ok=True)

    with open(OUT_FILE, "w", encoding="utf-8") as f:
        f.write(content)

    print(f"[OK] Written to: {OUT_FILE}")


# ----------------------------------------------------------------------
# ENTRY POINT
# ----------------------------------------------------------------------

def main():
    try:
        doc = build_document()
        write_output(doc)
        print("=== BUILD COMPLETE ===")
    except Exception as e:
        print(f"[FATAL ERROR] {e}")


if __name__ == "__main__":
    main()