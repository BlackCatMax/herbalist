"""
Начальные окна раскрытия цветов (OpenPhase) из карточек компендиума растений.

Этап 3б docs/research/DESIGN_Living_Vegetation_Research.md (§3.3, решение
пользователя 2026-09-16): окно раскрытия -- параметр инстанса материала вида
(MF_FlowerOpen), начальные значения -- из окна сбора HarvestTimeWindow, а
карточки, где текст говорит об ином окне или о сборе нераскрытым, -- в отчёт
для ручной разметки. Мешей и инстансов у видов пока нет (ResourceMesh пуст),
поэтому результат -- данные: когда инстансы появятся, их заполняют из json.

Цветущий вид: тип карточки -- трава или кустарник (деревья, хвойные, мхи,
лишайники, грибы, кора, папоротники, продукт гниения -- нет) и в тексте есть
цветение (цветёт, цветки, соцветия, лепестки).

OpenPhase -- веса окна [рассвет, день, закат, ночь] (порядок DayPhaseWeights):
окно сбора Dawn -> [1,0,0,0], Day -> [0,1,0,0], Dusk -> [0,0,1,0],
Night -> [0,0,0,1], без окна -> день.

Пометки Review -- {Flag, Text}: флаг и предложение карточки, которое его дало:
    closed_at_harvest -- собирают нераскрытым («пока не раскрылся», «в бутонах»):
                         окно сбора не равно окну раскрытия;
    open_close_text   -- текст сам описывает раскрытие или закрытие цветка;
    time_<фаза>       -- текст называет фазу суток, не совпадающую с окном сбора.
Всё это ищется только в предложениях о сборе или цветении. Поиск по словам:
пометка -- повод прочитать предложение, а не готовое расхождение. У видов с
неприметным цветком или сбором ягод и листьев (ковыль, крапива, брусника)
окно сбора к раскрытию цветка отношения не имеет -- их тоже размечать руками.

Запуск (из корня репозитория):  py tools/data_extraction/extract_flower_open_phase.py [путь_вывода.json]
Без пути пишет herbalist_docs/CSV_tabs/ingredient_flower_open_phase.json.
"""
import json
import re
import sys
from pathlib import Path

import yaml

if hasattr(sys.stdout, "reconfigure"):
    sys.stdout.reconfigure(encoding="utf-8")  # иначе кириллица в отчёте ломается на cp1251-консоли

ROOT = Path(__file__).resolve().parents[2]
CARDS = ROOT / "herbalist_docs" / "Herbalist_Vault" / "04_Compendium" / "Растительность"
WINDOWS = ROOT / "herbalist_docs" / "CSV_tabs" / "ingredient_harvest_windows.json"
DEFAULT_OUTPUT = ROOT / "herbalist_docs" / "CSV_tabs" / "ingredient_flower_open_phase.json"

PHASES = ["Dawn", "Day", "Dusk", "Night"]

FLOWERING_TYPE = re.compile(r"трав|кустарн", re.IGNORECASE)
NOT_FLOWERING_TYPE = re.compile(r"дерев|хвой|мох|лишайн|гриб|кора|папоротн|гниени", re.IGNORECASE)
# «цвет» (окраска) не считается: только цветок, цветы, цветёт, цветение.
FLOWERING_TEXT = re.compile(r"цвет(?:ок|ы|ов|ам|ами|ах|ут|ёт|ет|ёт)\b|цветк|цветени|соцвет|лепест|цветущ|отцвет|зацвет", re.IGNORECASE)

CLOSED_AT_HARVEST = re.compile(r"пока\s+(?:\w+\s+){0,2}не\s+раскры|до\s+(?:того,?\s+как\s+)?раскры|нераскры|в\s+бутон|(?:ещё|еще)\s+закрыт", re.IGNORECASE)
OPEN_CLOSE_TEXT = re.compile(r"раскрыва|раскроет|раскрыт|закрыва|закроет|закрыт|распуска|складыва|сложит", re.IGNORECASE)
TIME_WORDS = {
    "Dawn": re.compile(r"\bзар(?:я|е|ю|и)\b|зорьк|рассвет|утренн|\bутром\b|до\s+восход", re.IGNORECASE),
    "Day": re.compile(r"полд(?:ень|ня|не|нем)|полуден|\bднём\b|\bднем\b|в\s+жару", re.IGNORECASE),
    "Dusk": re.compile(r"закат|вечерн|\bвечером\b|сумерк", re.IGNORECASE),
    "Night": re.compile(r"\bноч(?:ь|ью|и|ной|ная|ное|ную)\b|полноч|купальск", re.IGNORECASE),
}


def read_card(path):
    text = path.read_text(encoding="utf-8")
    match = re.match(r"---\n(.*?)\n---\n(.*)", text, re.DOTALL)
    if not match:
        return None, text
    try:
        front = yaml.safe_load(match.group(1)) or {}
    except yaml.YAMLError:
        front = {}
    return front, match.group(2)


def open_phase_for(window):
    phase = window if window in PHASES else "Day"
    return [1.0 if name == phase else 0.0 for name in PHASES]


# Время суток ищется только в предложениях о сборе или цветении: иначе
# «ночью» из поверий и легенд давало бы ложные пометки.
RELEVANT_SENTENCE = re.compile(r"собира|сбор|срыва|сорва|срез|рвут|рвать|копа|раскры|закры|распуска|складыва", re.IGNORECASE)


def sentences_of(body):
    # Абзацы и строки заголовков -- отдельно, внутри абзаца -- по концу предложения.
    for block in re.split(r"\n\s*\n|\n(?=#)", body):
        for part in re.split(r"(?<=[.!?»])\s+", block):
            part = " ".join(part.split())
            if part:
                yield part


def review_notes(body, window):
    """Пометки и предложение, давшее каждую: разметчик видит, почему."""
    notes = []
    effective = window if window in PHASES else "Day"
    for sentence in sentences_of(body):
        if not (RELEVANT_SENTENCE.search(sentence) or FLOWERING_TEXT.search(sentence)):
            continue
        flags = []
        if CLOSED_AT_HARVEST.search(sentence):
            flags.append("closed_at_harvest")
        if OPEN_CLOSE_TEXT.search(sentence):
            flags.append("open_close_text")
        for phase, pattern in TIME_WORDS.items():
            if phase != effective and pattern.search(sentence):
                flags.append("time_" + phase.lower())
        for flag in flags:
            notes.append({"Flag": flag, "Text": sentence})
    return notes


def main():
    output = Path(sys.argv[1]) if len(sys.argv) > 1 else DEFAULT_OUTPUT
    windows = {row["Name"]: row.get("HarvestTimeWindow") for row in json.loads(WINDOWS.read_text(encoding="utf-8"))}

    rows = []
    skipped = 0
    for path in sorted(CARDS.rglob("*.md")):
        front, body = read_card(path)
        if not front or not front.get("id"):
            print(f"  ! карточка без фронтматтера или id пропущена: {path.relative_to(ROOT)}")
            continue
        card_type = str(front.get("type") or "")
        if not FLOWERING_TYPE.search(card_type) or NOT_FLOWERING_TYPE.search(card_type) or not FLOWERING_TEXT.search(body):
            skipped += 1
            continue
        window = windows.get(front["id"])
        rows.append({
            "Name": front["id"],
            "DisplayName": front.get("name", path.stem),
            "HarvestTimeWindow": window or "Any",
            "OpenPhase": open_phase_for(window),
            "Review": review_notes(body, window),
        })

    rows.sort(key=lambda row: row["Name"])
    output.write_text(json.dumps(rows, ensure_ascii=False, indent=4) + "\n", encoding="utf-8")

    needs_review = [row for row in rows if row["Review"]]
    print(f"Цветущих видов: {len(rows)} (не цветущих или без цветения в тексте: {skipped}); на разметку: {len(needs_review)}")
    for row in needs_review:
        flags = sorted({note["Flag"] for note in row["Review"]})
        print(f"  {row['Name']:8} {row['DisplayName']:30} окно {row['HarvestTimeWindow']:5} -> {', '.join(flags)}")
    print(f"Записано: {output.relative_to(ROOT) if output.is_relative_to(ROOT) else output}")


if __name__ == "__main__":
    main()
