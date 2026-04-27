import re
import yaml
import json
from pathlib import Path

ID_TO_BIOME_EN = {
    "boloto": "Bog",
    "lesostep": "ForestSteppe",
    "poyma_rechnaya": "Floodplain",
    "smeshannye_lesa": "MixedForest",
    "step": "Steppe",
    "shirokolistvennye_lesa": "BroadleafForest",
    "tayga": "Taiga",
    "tundra": "Tundra"
}

def parse_frontmatter(content):
    """Ищет YAML между ---, не обязательно в начале."""
    match = re.search(r'---\n(.*?)\n---', content, re.DOTALL)
    if not match:
        return {}
    try:
        return yaml.safe_load(match.group(1))
    except:
        return {}

def extract_water_section_params(content):
    """Извлекает параметры воды из раздела ##  Вода."""
    water_section = re.search(r'##  Вода\n\n(.*?)(?=\n##|\Z)', content, re.DOTALL)
    if not water_section:
        return None
    text = water_section.group(1)
    params = {}
    for key, pattern in [('purity', r'\[\[Purity\]\]:\s*([\d\.]+)'),
                         ('corruption', r'\[\[Corruption\]\]:\s*([\d\.]+)'),
                         ('distortion', r'\[\[Distortion\]\]:\s*([\d\.]+)')]:
        m = re.search(pattern, text)
        if m:
            params[key] = float(m.group(1))
    return params

def main():
    vault_dir = Path("K:/herbalist/herbalist_docs/Herbalist_Vault")
    biome_file = vault_dir / "build" / "biomes_compendium.md"
    if not biome_file.exists():
        biome_file = vault_dir / "biomes_compendium.md"
    if not biome_file.exists():
        print(f"Файл не найден: {biome_file}")
        return

    output_dir = Path("K:/herbalist/herbalist_docs/CSV_tabs")
    output_dir.mkdir(parents=True, exist_ok=True)
    output_json = output_dir / "water_types.json"

    with open(biome_file, 'r', encoding='utf-8') as f:
        full_text = f.read()

    # Разделяем на секции по BEGIN ... md -->
    sections = re.split(r'<!-- BEGIN.*?\.md -->', full_text)[1:]
    rows = []

    for section in sections:
        end_match = re.search(r'<!-- END.*?\.md -->', section)
        if end_match:
            section = section[:end_match.start()]

        front = parse_frontmatter(section)
        if not front or 'id' not in front:
            continue

        biome_id = front['id']
        if biome_id not in ID_TO_BIOME_EN:
            print(f"Неизвестный id биома: {biome_id}, пропускаем")
            continue

        biome_en = ID_TO_BIOME_EN[biome_id]
        water_type_id = f"{biome_en}Water"
        biome_name_ru = front.get('name', biome_id)

        base_potency = float(front.get('potency', 0.5))
        base_stability = float(front.get('stability', 0.5))

        # Параметры из секции воды (более точные для воды)
        water_params = extract_water_section_params(section)
        if water_params:
            base_purity = water_params.get('purity', float(front.get('purity', 0.5)))
            base_distortion = water_params.get('distortion', float(front.get('distortion', 0.3)))
            base_corruption = water_params.get('corruption', float(front.get('corruption', 0.2)))
        else:
            base_purity = float(front.get('purity', 0.5))
            base_distortion = float(front.get('distortion', 0.3))
            base_corruption = float(front.get('corruption', 0.2))

        rows.append({
            "Name": water_type_id,
            "WaterTypeID": water_type_id,
            "DisplayName": f"{biome_name_ru} вода",
            "AllowedBiomes": [biome_en],
            "BasePurity": base_purity,
            "BaseDistortion": base_distortion,
            "BaseStability": base_stability,
            "BasePotency": base_potency,
            "BaseCorruption": base_corruption,
            "SpecialEffect": "None",
            "Rarity": 1.0
        })

    if rows:
        with open(output_json, 'w', encoding='utf-8') as f:
            json.dump(rows, f, indent=4, ensure_ascii=False)
        print(f"Сохранено {len(rows)} типов воды в {output_json}")
        print("\nИмпорт в Unreal Engine:")
        print("1. Откройте Content Browser")
        print("2. Правый клик на папке /Game/Herbalist/Data/")
        print("3. Import → water_types.json")
        print("4. Row Structure = WaterTypeRow")
        print("5. Убедитесь, что столбец Name соответствует RowName")
    else:
        print("Не удалось извлечь ни одного типа воды.")

if __name__ == "__main__":
    main()