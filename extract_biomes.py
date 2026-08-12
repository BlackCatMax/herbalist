"""
Генерация DT_BiomeDefaults.json из компендиума биомов.

Аналог extract_ingredients.py, но для 04_Compendium/Биомы. Смысл тот же:
единственный источник истины — написанный лор, а числа в DataTable выводятся
из него, а не подбираются руками. До этого скрипта DT_BiomeDefaults.json вёлся
отдельно и разошёлся с компендиумом примерно по 20 значениям.

Что берётся из фронтматтера компендиума:
    body/mind/spirit/nature, magnitude,
    distortion/stability/purity/potency/resonance/corruption,
    toxicity/fertility/moisture

Что НЕ описано во фронтматтере и потому сохраняется из существующего JSON:
    EntityActivityBase  — в компендиуме отсутствует
    DefaultWaterState   — в компендиуме есть только прозой (раздел «## Вода»),
                          причём неполно: без Magnitude/Stability/Potency/Resonance.
                          Разбирать прозу регуляркой ненадёжно, поэтому вода
                          переносится из старого JSON целиком, а расхождения по
                          тем полям, что в прозе всё же указаны, — докладываются.

Скрипт НИЧЕГО не решает за автора: если фронтматтер противоречит таблицам в теле
того же документа, он это печатает и оставляет как есть (берётся фронтматтер,
как машиночитаемый источник).

Запуск:  py extract_biomes.py [--write]
Без --write только показывает, что изменится.
"""
import io
import json
import re
import sys
from pathlib import Path

if hasattr(sys.stdout, "reconfigure"):
    sys.stdout.reconfigure(encoding="utf-8")  # иначе кириллица в отчёте ломается на cp1251-консоли

REPO = Path(__file__).resolve().parent
BIOMES_DIR = REPO / "herbalist_docs" / "Herbalist_Vault" / "04_Compendium" / "Биомы"
OUT_JSON = REPO / "herbalist_docs" / "CSV_tabs" / "DT_BiomeDefaults.json"

# Русское имя файла -> Name в DataTable (те же ключи, что в extract_ingredients.py)
BIOME_MAP = {
    "Болото": "Bog",
    "Лесостепь": "ForestSteppe",
    "Речная пойма": "Floodplain",
    "Смешанный лес": "MixedForest",
    "Широколиственный лес": "BroadleafForest",
    "Степь": "Steppe",
    "Тайга": "Taiga",
    "Тундра": "Tundra",
}

META_KEYS = [
    ("distortion", "Distortion"),
    ("stability", "Stability"),
    ("purity", "Purity"),
    ("potency", "Potency"),
    ("resonance", "Resonance"),
    ("corruption", "Corruption"),
]
ENV_KEYS = [("toxicity", "Toxicity"), ("fertility", "Fertility"), ("moisture", "Moisture")]
DIR_KEYS = [("body", "Body"), ("mind", "Mind"), ("spirit", "Spirit"), ("nature", "Nature")]


def parse_frontmatter(text):
    """YAML-фронтматтер разбираем вручную: нужны только скалярные числа и
    строки, а тянуть зависимость pyyaml ради этого не хочется (в отличие от
    extract_ingredients.py, которому нужны вложенные списки)."""
    m = re.match(r"^---\n(.*?)\n---\n", text, re.DOTALL)
    if not m:
        return {}
    out = {}
    for line in m.group(1).splitlines():
        kv = re.match(r"^([A-Za-z_][A-Za-z0-9_]*):\s*(.*)$", line)
        if kv:
            out[kv.group(1)] = kv.group(2).strip()
    return out


def num(front, key, default=None):
    raw = front.get(key)
    if raw is None:
        return default
    m = re.match(r"^-?[\d.]+", str(raw))
    return float(m.group(0)) if m else default


def body_table_values(text):
    """Числа из markdown-таблиц в теле документа — только чтобы сверить их с
    фронтматтером и доложить расхождения. Источником не являются."""
    found = {}
    for m in re.finditer(r"\[\[(\w+)\]\]\s*\|\s*([\d.]+)\s*\|", text):
        found.setdefault(m.group(1).lower(), []).append(float(m.group(2)))
    return found


def main():
    write = "--write" in sys.argv
    if not BIOMES_DIR.exists():
        print(f"Папка компендиума не найдена: {BIOMES_DIR}")
        return 1

    old_rows = {}
    if OUT_JSON.exists():
        with io.open(OUT_JSON, encoding="utf-8") as f:
            old_rows = {r["Name"]: r for r in json.load(f)}

    rows, notes, diffs = [], [], []

    for ru, en in sorted(BIOME_MAP.items(), key=lambda kv: kv[1]):
        path = BIOMES_DIR / f"{ru}.md"
        if not path.exists():
            notes.append(f"{en}: файл компендиума не найден ({path.name}) — строка взята из старого JSON")
            if en in old_rows:
                rows.append(old_rows[en])
            continue

        text = io.open(path, encoding="utf-8").read()
        front = parse_frontmatter(text)
        prev = old_rows.get(en, {})

        # сверка фронтматтера с таблицами в теле — противоречия не решаем, докладываем
        tables = body_table_values(text)
        for fm_key, dt_key in META_KEYS + DIR_KEYS + [("magnitude", "Magnitude")]:
            fm_val = num(front, fm_key)
            for tv in tables.get(fm_key, []):
                if fm_val is not None and abs(tv - fm_val) > 1e-6:
                    notes.append(
                        f"{ru}: '{fm_key}' = {fm_val} во фронтматтере, но {tv} в тексте документа "
                        f"— взят фронтматтер, решать автору"
                    )
                    break

        row = {
            "Name": en,
            "DisplayName": front.get("name", ru),
            "Direction": {dt: num(front, fm, 0.25) for fm, dt in DIR_KEYS},
            "Magnitude": num(front, "magnitude", 0.5),
            "Meta": {dt: num(front, fm, 0.0) for fm, dt in META_KEYS},
            "Environment": {dt: num(front, fm, 0.0) for fm, dt in ENV_KEYS},
            # не описаны во фронтматтере — переносим как есть
            "EntityActivityBase": prev.get("EntityActivityBase", 0.3),
            "DefaultWaterState": prev.get(
                "DefaultWaterState",
                {"Magnitude": 0.5,
                 "Direction": {dt: num(front, fm, 0.25) for fm, dt in DIR_KEYS},
                 "Meta": {dt: num(front, fm, 0.0) for fm, dt in META_KEYS}},
            ),
        }
        rows.append(row)

        for section, keys in (("Meta", META_KEYS), ("Environment", ENV_KEYS)):
            for _, dt in keys:
                new = row[section][dt]
                old = prev.get(section, {}).get(dt)
                if old is not None and abs(new - old) > 1e-6:
                    diffs.append(f"  {en:<18}{section}.{dt:<12}{old:>6.2f} -> {new:>6.2f}")
        for _, dt in DIR_KEYS:
            new, old = row["Direction"][dt], prev.get("Direction", {}).get(dt)
            if old is not None and abs(new - old) > 1e-6:
                diffs.append(f"  {en:<18}Direction.{dt:<7}{old:>6.2f} -> {new:>6.2f}")
        if prev.get("Magnitude") is not None and abs(row["Magnitude"] - prev["Magnitude"]) > 1e-6:
            diffs.append(f"  {en:<18}{'Magnitude':<17}{prev['Magnitude']:>6.2f} -> {row['Magnitude']:>6.2f}")

    print(f"Биомов разобрано: {len(rows)}")
    if diffs:
        print(f"\nРасхождения компендиум <-> текущий DataTable ({len(diffs)}):")
        print("\n".join(diffs))
    else:
        print("\nРасхождений нет — DataTable уже соответствует компендиуму.")
    if notes:
        print(f"\nТребует внимания автора ({len(notes)}):")
        for n in notes:
            print(f"  ! {n}")

    if write:
        with io.open(OUT_JSON, "w", encoding="utf-8") as f:
            json.dump(rows, f, ensure_ascii=False, indent=2)
            f.write("\n")
        print(f"\nЗаписано: {OUT_JSON}")
        print("ВНИМАНИЕ: игра читает Content/Data/DT_BiomeDefaults.uasset, а не этот JSON.")
        print("Чтобы правки дошли до игры, таблицу нужно переимпортировать в редакторе UE.")
    else:
        print("\n(предпросмотр; чтобы записать — запустить с --write)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
