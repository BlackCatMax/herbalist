import re
import yaml
import json
from pathlib import Path

BIOME_MAP = {
    "Болото": "Bog",
    "Лесостепь": "ForestSteppe",
    "Речная пойма": "Floodplain",
    "Смешанный лес": "MixedForest",
    "Широколиственный лес": "BroadleafForest",
    "Степь": "Steppe",
    "Тайга": "Taiga",
    "Тундра": "Tundra"
}

def parse_frontmatter(content):
    match = re.match(r'^---\n(.*?)\n---\n', content, re.DOTALL)
    if not match:
        return {}
    try:
        return yaml.safe_load(match.group(1))
    except:
        return {}

def parse_d_base(d_base_str):
    if isinstance(d_base_str, list):
        return d_base_str
    if isinstance(d_base_str, str):
        cleaned = re.sub(r'[\[\]\s]', '', d_base_str)
        parts = cleaned.split(',')
        if len(parts) == 4:
            return [float(p) for p in parts]
    return [0.25, 0.25, 0.25, 0.25]

def map_biome(rus_biome):
    return BIOME_MAP.get(rus_biome, rus_biome)

def normalize_biomes(biomes_raw):
    if isinstance(biomes_raw, list):
        result = []
        for item in biomes_raw:
            if isinstance(item, list):
                result.extend(normalize_biomes(item))
            else:
                result.append(map_biome(item))
        return result
    elif isinstance(biomes_raw, str):
        return [map_biome(biomes_raw)]
    else:
        return []

def element_to_string(element_raw):
    if element_raw is None:
        return ""
    if isinstance(element_raw, str):
        return element_raw
    if isinstance(element_raw, list):
        items = []
        for item in element_raw:
            sub = element_to_string(item)
            if sub:
                items.append(sub)
        return ", ".join(items)
    return str(element_raw)

def main():
    # Было захардкожено на K:/herbalist — путь с прежней буквы диска, script
    # молча не находил папку при запуске из репозитория на G:. Найдено при
    # аудите 2026-08-24 (extract_biomes.py уже брал REPO относительно себя).
    repo = Path(__file__).resolve().parent
    ingredients_dir = repo / "herbalist_docs" / "Herbalist_Vault" / "04_Compendium" / "Растительность"
    if not ingredients_dir.exists():
        print(f"Папка не найдена: {ingredients_dir}")
        return

    output_json = repo / "herbalist_docs" / "CSV_tabs" / "ingredients.json"
    output_json.parent.mkdir(parents=True, exist_ok=True)

    all_md = list(ingredients_dir.rglob("*.md"))
    rows = []

    for filepath in all_md:
        with open(filepath, 'r', encoding='utf-8') as f:
            content = f.read()
        front = parse_frontmatter(content)
        if not front or 'id' not in front:
            continue

        ingredient_id = front.get('id')
        name = front.get('name')
        body_text = re.split(r'\n---\n', content)[-1]
        description = body_text[:255].replace('\n', ' ').strip()

        ingr_type = front.get('type', '').lower()
        if 'гриб' in ingr_type:
            class_name = 'Fungus'
        elif 'минерал' in ingr_type:
            class_name = 'Mineral'
        elif 'катализатор' in ingr_type:
            class_name = 'Catalyst'
        elif 'эссенция' in ingr_type:
            class_name = 'Essence'
        elif 'вода' in ingr_type:
            class_name = 'Water'
        else:
            class_name = 'Plant'
        b_is_water = (class_name == 'Water')

        biomes_raw = front.get('biome', [])
        allowed_biomes = normalize_biomes(biomes_raw)

        element = element_to_string(front.get('element', ''))

        tags_raw = front.get('tags', [])
        if isinstance(tags_raw, list):
            tags = tags_raw
        else:
            tags = [tags_raw] if tags_raw else []

        d_base = parse_d_base(front.get('d_base', [0.25,0.25,0.25,0.25]))
        m_base = front.get('m_base', 0.5)
        potency = front.get('potency', 0.5)
        purity = front.get('purity', 0.5)
        stability = front.get('stability', 0.5)
        resonance = front.get('resonance', 0.5)
        corruption = front.get('corruption', 0.2)
        distortion = front.get('distortion', 0.3)
        # Отсутствовали в схеме до аудита 2026-08-24: без ключа в frontmatter
        # дефолт совпадает с FIngredientTableRow (Resilience=0.0, DecayRate=1.0),
        # так что старые карточки без этих полей не меняют поведения.
        resilience = front.get('resilience', 0.0)
        decay_rate = front.get('decay_rate', 1.0)

        row = {
            "Name": ingredient_id,
            "DisplayName": f'NSLOCTEXT("DT_IngredientClass", "{ingredient_id}_DisplayName", "{name}")',
            "Description": f'NSLOCTEXT("DT_IngredientClass", "{ingredient_id}_Description", "{description}")',
            "Icon": "",
            "ResourceMesh": "",
            "BaseState": {
                "Magnitude": float(m_base),
                "Direction": {
                    "Body": float(d_base[0]),
                    "Mind": float(d_base[1]),
                    "Spirit": float(d_base[2]),
                    "Nature": float(d_base[3])
                },
                "Meta": {
                    "Distortion": float(distortion),
                    "Stability": float(stability),
                    "Purity": float(purity),
                    "Potency": float(potency),
                    "Resonance": float(resonance),
                    "Corruption": float(corruption)
                }
            },
            "Class": class_name,
            "bIsWater": b_is_water,
            "AllowedBiomes": allowed_biomes,
            "RarityWeight": 1,
            "DecayRate": float(decay_rate),
            "Resilience": float(resilience),
            "Element": element,
            "Tags": tags
        }
        rows.append(row)

    if rows:
        with open(output_json, 'w', encoding='utf-8') as f:
            json.dump(rows, f, indent=4, ensure_ascii=False)
        print(f"Сохранено {len(rows)} ингредиентов в {output_json}")
        print("\nИмпорт в Unreal Engine:\n"
              "1. Откройте Content Browser\n"
              "2. Нажмите правой кнопкой мыши на папке /Game/Herbalist/Data/\n"
              "3. Выберите Import -> ingredients.json\n"
              "4. В диалоге укажите Row Structure = IngredientTableRow\n"
              "5. Нажмите Import")
    else:
        print("Ни одного ингредиента не найдено.")

if __name__ == "__main__":
    main()