#!/usr/bin/env python3
import re
from pathlib import Path
from datetime import datetime
import argparse

ROOT = Path(__file__).parent.parent
SRC_DIR = ROOT / "src"
BUILD_DIR = ROOT / "build"

TECHNICAL_ORDER = [
    "technical/01-core.md",
    "technical/02-formal-model.md",
    "technical/03-process-system.md",
    "technical/04-input-layer.md",
    "technical/05-context-layer.md",
    "technical/06-resources.md",
    "technical/07-output-layer.md",
    "technical/08-meta-systems.md",
    "technical/09-integration.md",
    "technical/10-configuration.md",
    "technical/11-memory-fragments.md",
    "technical/12-intent-mechanics.md",
    "technical/13-entity-dynamics.md",
    "technical/14-biome-transition.md",
    "technical/15-world-pipeline.md",
]

DESIGN_ORDER = [
    "design/01-introduction.md",
    "design/02-setting.md",
    "design/03-narrative.md",
    "design/04-game-loop.md",
    "design/05-systems.md",
    "design/06-progression.md",
    "design/07-ux.md",
    "design/08-tables.md",
    "design/09-memory-fragments.md",
    "design/10-intent-evolution.md",
    "design/11-entity-dynamics.md",
    "design/12-biome-change.md",
    "design/13-world-pipeline.md",
]


def get_version():
    try:
        import subprocess
        result = subprocess.run(
            ["git", "describe", "--tags", "--abbrev=0"],
            capture_output=True,
            text=True,
            cwd=ROOT
        )
        if result.returncode == 0:
            return result.stdout.strip()
    except Exception:
        pass

    return datetime.now().strftime("%Y.%m.%d")


def build_document(order, output_name, version):
    output_lines = [
        f"# {output_name.replace('_', ' ').title()}",
        "",
        f"> Version: {version}",
        f"> Build date: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}",
        "",
        "---",
        ""
    ]

    for rel_path in order:
        src_path = SRC_DIR / rel_path

        if not src_path.exists():
            print(f"  ⚠ Missing: {src_path}")
            continue

        content = src_path.read_text(encoding="utf-8")

        output_lines.append(f"\n<!-- {rel_path} -->\n")
        output_lines.append(content)

    BUILD_DIR.mkdir(exist_ok=True)

    output_path = BUILD_DIR / f"{output_name}.md"
    output_path.write_text("\n".join(output_lines), encoding="utf-8")

    return output_path


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--clean", action="store_true")
    args = parser.parse_args()

    print("\nBuilding documentation...\n")

    if args.clean and BUILD_DIR.exists():
        import shutil
        shutil.rmtree(BUILD_DIR)
        print("  Cleaned build/")

    version = get_version()
    print(f"  Version: {version}\n")

    # Build technical
    tech_path = build_document(TECHNICAL_ORDER, "technical", version)
    print(f"  OK {tech_path}")

    # Build design
    design_path = build_document(DESIGN_ORDER, "design", version)
    print(f"  OK {design_path}")

    # Explicitly skip glossary
    print("  Skipped glossary (manual source of truth)")

    print("\nDone!")


if __name__ == "__main__":
    main()