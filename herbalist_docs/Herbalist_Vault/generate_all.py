#!/usr/bin/env python3
# -*- coding: utf-8 -*-

"""
Генератор карточек для Obsidian из ГДД Herbalist.
Использование:
    python generate_all.py [--all] [--ingredients] [--bestiary] [--biomes]
Без аргументов запускает интерактивное меню.
"""

import re
import sys
from pathlib import Path

# --------------------------- НАСТРОЙКИ ---------------------------
VAULT_ROOT = Path(__file__).parent.resolve()
GD_DOC_PATH = VAULT_ROOT / "02_GDD" / "10_Biomes_Reference.md"

# Папка для сгенерированных справочников
COMPENDIUM_DIR = VAULT_ROOT / "04_Compendium"

# Папка с изображениями
ASSETS_ROOT = VAULT_ROOT / "05_Assets"

# Относительный путь от карточки до ассетов
# Карточка: 04_Compendium/Ingredients/tundra/yagel.md
# Изображение: 05_Assets/ingredients/tundra/yagel.png
# Относительный путь: ../../../05_Assets/ingredients/tundra/yagel.png
ASSETS_BASE = {
    "ingredients": "../../../05_Assets/ingredients",
    "bestiary":    "../../../05_Assets/bestiary",
    "biomes":      "../../../05_Assets/biomes"
}

# --------------------------- УТИЛИТЫ ---------------------------
def slugify(text):
    trans = {
        'а': 'a', 'б': 'b', 'в': 'v', 'г': 'g', 'д': 'd', 'е': 'e', 'ё': 'e',
        'ж': 'zh', 'з': 'z', 'и': 'i', 'й': 'y', 'к': 'k', 'л': 'l', 'м': 'm',
        'н': 'n', 'о': 'o', 'п': 'p', 'р': 'r', 'с': 's', 'т': 't', 'у': 'u',
        'ф': 'f', 'х': 'h', 'ц': 'ts', 'ч': 'ch', 'ш': 'sh', 'щ': 'sch',
        'ъ': '', 'ы': 'y', 'ь': '', 'э': 'e', 'ю': 'yu', 'я': 'ya',
        ' ': '_', '-': '_', ',': '', '(': '', ')': '', ':': ''
    }
    text = text.lower()
    for ru, en in trans.items():
        text = text.replace(ru, en)
    text = re.sub(r'[^a-z0-9_]', '', text)
    return text.strip('_')

def find_image_ext(base_name, category, biome_slug):
    """Находит реальное расширение файла изображения в папке ассетов."""
    asset_dir = ASSETS_ROOT / category / biome_slug
    for ext in ['.png', '.jpg', '.jpeg', '.webp']:
        img_path = asset_dir / f"{base_name}{ext}"
        if img_path.exists():
            return ext
    return '.png'  # fallback

def read_gdd():
    with open(GD_DOC_PATH, 'r', encoding='utf-8') as f:
        return f.read()

# --------------------------- ПАРСЕРЫ ---------------------------
def parse_biome_tables(content):
    biomes = []
    lines = content.split('\n')
    i = 0
    current_biome = None
    while i < len(lines):
        line = lines[i]
        if line.startswith('## ') and re.match(r'## \d+\.', line):
            if current_biome:
                biomes.append(current_biome)
            name = re.sub(r'^## \d+\.\s+', '', line).strip()
            current_biome = {'name': name, 'params': {}, 'water': {}}
        if current_biome and line.startswith('| Параметр | Значение |'):
            i += 1
            i += 1
            while i < len(lines) and lines[i].startswith('|'):
                row = lines[i]
                cells = [c.strip() for c in row.split('|')[1:-1]]
                if len(cells) >= 2:
                    key = cells[0].replace('[[', '').replace(']]', '')
                    try:
                        val = float(cells[1])
                    except:
                        val = cells[1]
                    current_biome['params'][key] = val
                i += 1
        # Таблица воды
        if line.startswith('## 💧 9. Вода по биомам') or 'Вода по биомам' in line:
            i += 1
            while i < len(lines) and not lines[i].startswith('| Биом |'):
                i += 1
            i += 1  # пропускаем строку заголовка таблицы
            i += 1  # пропускаем строку разделителя (|---|)
            while i < len(lines) and lines[i].startswith('|'):
                row = lines[i]
                cells = [c.strip() for c in row.split('|')[1:-1]]
                if len(cells) >= 9:
                    biome_name = cells[0]
                    water_data = {
                        'type': cells[1],
                        'body': float(cells[2]),
                        'mind': float(cells[3]),
                        'spirit': float(cells[4]),
                        'nature': float(cells[5]),
                        'purity': float(cells[6]),
                        'corruption': float(cells[7]),
                        'distortion': float(cells[8])
                    }
                    for b in biomes:
                        if b['name'] == biome_name:
                            b['water'] = water_data
                            break
                i += 1
        i += 1
    if current_biome:
        biomes.append(current_biome)
    return biomes

def parse_ingredients(content):
    ingredients = []
    biome = None
    lines = content.split('\n')
    i = 0
    while i < len(lines):
        line = lines[i]
        biome_match = re.match(r'## (\d+\.\s+[^\n]+)', line)
        if biome_match:
            biome = re.sub(r'^\d+\.\s+', '', biome_match.group(1).strip())
        if line.startswith('| № | Ингредиент | Тип | Роль в биоме |'):
            i += 1
            i += 1
            rows = []
            while i < len(lines) and lines[i].startswith('|'):
                rows.append(lines[i])
                i += 1
            for row in rows:
                cells = [c.strip() for c in row.split('|')[1:-1]]
                if len(cells) >= 4:
                    num, name, itype, role = cells[:4]
                    ingredients.append({
                        'biome': biome,
                        'name': name,
                        'type': itype,
                        'role': role
                    })
            continue
        i += 1
    return ingredients

def parse_entities(content):
    entities = []
    biome = None
    lines = content.split('\n')
    i = 0
    while i < len(lines):
        line = lines[i]
        biome_match = re.match(r'## (\d+\.\s+[^\n]+)', line)
        if biome_match:
            biome = re.sub(r'^\d+\.\s+', '', biome_match.group(1).strip())
        if line.startswith('###') and 'Сущности' in line:
            i += 1
            while i < len(lines) and not lines[i].startswith('| Уровень'):
                i += 1
            if i >= len(lines): break
            i += 1
            while i < len(lines) and lines[i].startswith('|'):
                row = lines[i]
                cells = [c.strip() for c in row.split('|')[1:-1]]
                if len(cells) >= 2:
                    level = cells[0].replace('**', '')
                    names = cells[1]
                    for name in re.split(r',\s*', names):
                        name = name.strip()
                        if name:
                            entities.append({
                                'biome': biome,
                                'level': level,
                                'name': name
                            })
                i += 1
        i += 1
    return entities

# --------------------------- ГЕНЕРАТОРЫ ---------------------------
def generate_ingredient_card(ing, output_dir):
    name = ing['name']
    biome = ing['biome']
    itype = ing['type']
    role = ing['role']
    slug_name = slugify(name)
    slug_biome = slugify(biome)

    # Определяем расширение изображения
    ext = find_image_ext(slug_name, "ingredients", slug_biome)
    img_path = f"{ASSETS_BASE['ingredients']}/{slug_biome}/{slug_name}{ext}"

    # Параметры по умолчанию
    params = {
        'd_base': [0.5, 0.5, 0.5, 0.5],
        'm_base': 0.5,
        'potency': 0.5,
        'purity': 0.5,
        'stability': 0.5,
        'resonance': 0.5,
        'corruption': 0.3,
        'distortion': 0.3,
        'element': 'Земля'
    }
    CUSTOM = {
        "Ягель": {"d_base": [0.30,0.25,0.60,0.70], "m_base":0.45, "potency":0.50, "purity":0.65, "stability":0.55, "resonance":0.60, "corruption":0.20, "distortion":0.30, "element":"Земля"},
    }
    if name in CUSTOM:
        params.update(CUSTOM[name])

    d = params['d_base']
    yaml = f"""---
id: {slug_biome}_{slug_name}
name: {name}
biome: {biome}
type: {itype}
d_base: [{d[0]}, {d[1]}, {d[2]}, {d[3]}]
m_base: {params['m_base']}
potency: {params['potency']}
purity: {params['purity']}
stability: {params['stability']}
resonance: {params['resonance']}
corruption: {params['corruption']}
distortion: {params['distortion']}
element: {params['element']}
tags: [{slug_biome}, {slugify(itype)}]
image: "{name}{ext}"
---

# {name}

![[{img_path}]]

## Описание
{role}

## Свойства в алхимии
- **Оси:** Body {d[0]:.2f}, Mind {d[1]:.2f}, Spirit {d[2]:.2f}, Nature {d[3]:.2f}
- **Мета-параметры:** Potency {params['potency']:.2f}, Purity {params['purity']:.2f}, Stability {params['stability']:.2f}, Resonance {params['resonance']:.2f}, Corruption {params['corruption']:.2f}, Distortion {params['distortion']:.2f}
- **Стихия:** {params['element']}

## Где искать
{biome}, (уточнить конкретные места)

## Применение
(заполнить)

## Легенды и поверья
(заполнить)

---
← *[К списку ингредиентов {biome}](14_Ingredients_Compendium.md#{slug_biome})*
"""
    out_path = output_dir / slug_biome / f"{slug_name}.md"
    out_path.parent.mkdir(parents=True, exist_ok=True)
    with open(out_path, 'w', encoding='utf-8') as f:
        f.write(yaml)
    return out_path

def generate_entity_card(ent, output_dir):
    name = ent['name']
    biome = ent['biome']
    level = ent['level']
    slug_name = slugify(name)
    slug_biome = slugify(biome)
    ext = find_image_ext(slug_name, "bestiary", slug_biome)
    img_path = f"{ASSETS_BASE['bestiary']}/{slug_biome}/{slug_name}{ext}"

    d_manifest = [0.5, 0.5, 0.5, 0.5]
    morok = 0.3

    yaml = f"""---
id: {slug_biome}_{slugify(level)}_{slug_name}
name: {name}
biome: {biome}
level: {level}
type: (уточнить)
behavior: (уточнить)
danger: (уточнить)
d_manifest: {d_manifest}
morok_affinity: {morok}
image: "{name}{ext}"
tags: [{slug_biome}, {slugify(level)}]
---

# {name}

![[{img_path}]]

## 🪶 Описание
(заполнить)

## 📊 Параметры проявления
| Ось | Значение |
|-----|----------|
| Body | {d_manifest[0]:.2f} |
| Mind | {d_manifest[1]:.2f} |
| Spirit | {d_manifest[2]:.2f} |
| Nature | {d_manifest[3]:.2f} |

- **Сродство с [[Morok]]:** {morok:.2f}
- **Реакция на игрока:** (уточнить)

## ⚔️ Опасность
- **Уровень угрозы:** (уточнить)
- **Тип атак:** (уточнить)
- **Уязвимости:** (уточнить)

## 📍 Где встретить
{biome}, (уточнить конкретные места)

## 🧪 Алхимическое значение
(уточнить)

## 📜 Легенды и поверья
(заполнить)

---
← *[К бестиарию {biome}](15_Bestiary_Compendium.md#{slug_biome})*
"""
    out_path = output_dir / slug_biome / f"{slug_name}.md"
    out_path.parent.mkdir(parents=True, exist_ok=True)
    with open(out_path, 'w', encoding='utf-8') as f:
        f.write(yaml)
    return out_path

def generate_biome_card(biome_data, output_dir):
    name = biome_data['name']
    params = biome_data['params']
    water = biome_data['water']
    slug_name = slugify(name)
    ext = find_image_ext(slug_name, "biomes", "")  # биомы лежат прямо в 05_Assets/biomes/
    img_path = f"{ASSETS_BASE['biomes']}/{slug_name}{ext}"

    biome_type = "Лес" if "лес" in name.lower() else "Равнина" if "степь" in name.lower() or "тундра" in name.lower() else "Водно-болотный"

    yaml = f"""---
id: {slug_name}
name: {name}
type: {biome_type}
region: (уточнить)
climate: (уточнить)
s_real:
  body: {params.get('Body', 0.5)}
  mind: {params.get('Mind', 0.5)}
  spirit: {params.get('Spirit', 0.5)}
  nature: {params.get('Nature', 0.5)}
  magnitude: {params.get('Magnitude', 0.5)}
  potency: {params.get('Potency', 0.5)}
  purity: {params.get('Purity', 0.5)}
  stability: {params.get('Stability', 0.5)}
  resonance: {params.get('Resonance', 0.5)}
  corruption: {params.get('Corruption', 0.3)}
  distortion: {params.get('Distortion', 0.3)}
environment:
  toxicity: {params.get('Toxicity', 0.3)}
  fertility: {params.get('Fertility', 0.5)}
  moisture: {params.get('Moisture', 0.5)}
water:
  type: {water.get('type', '—')}
  body: {water.get('body', 0.5)}
  mind: {water.get('mind', 0.5)}
  spirit: {water.get('spirit', 0.5)}
  nature: {water.get('nature', 0.5)}
  purity: {water.get('purity', 0.5)}
  corruption: {water.get('corruption', 0.3)}
  distortion: {water.get('distortion', 0.3)}
morok_base: {params.get('Distortion', 0.3)}
tags: [биом, {slug_name}]
image: "{name}{ext}"
---

# {name}

![[{img_path}]]

## 🌍 Общее описание
(заполнить)

## 📊 Параметры состояния ([[S_real]])

### Оси
| Ось | Значение |
|-----|----------|
| [[Body]] | {params.get('Body', 0.5):.2f} |
| [[Mind]] | {params.get('Mind', 0.5):.2f} |
| [[Spirit]] | {params.get('Spirit', 0.5):.2f} |
| [[Nature]] | {params.get('Nature', 0.5):.2f} |

### Мета-параметры
| Параметр | Значение |
|----------|----------|
| [[Magnitude]] | {params.get('Magnitude', 0.5):.2f} |
| [[Potency]] | {params.get('Potency', 0.5):.2f} |
| [[Purity]] | {params.get('Purity', 0.5):.2f} |
| [[Stability]] | {params.get('Stability', 0.5):.2f} |
| [[Resonance]] | {params.get('Resonance', 0.5):.2f} |
| [[Corruption]] | {params.get('Corruption', 0.3):.2f} |
| [[Distortion]] | {params.get('Distortion', 0.3):.2f} |

### Параметры среды
| Параметр | Значение |
|----------|----------|
| [[Toxicity]] | {params.get('Toxicity', 0.3):.2f} |
| [[Fertility]] | {params.get('Fertility', 0.5):.2f} |
| [[Moisture]] | {params.get('Moisture', 0.5):.2f} |

## 💧 Вода
- **Тип:** {water.get('type', '—')}
- **Оси:** Body {water.get('body', 0.5):.2f}, Mind {water.get('mind', 0.5):.2f}, Spirit {water.get('spirit', 0.5):.2f}, Nature {water.get('nature', 0.5):.2f}
- **[[Purity]]:** {water.get('purity', 0.5):.2f}
- **[[Corruption]]:** {water.get('corruption', 0.3):.2f}
- **[[Distortion]]:** {water.get('distortion', 0.3):.2f}

## 🧌 Обитатели
(заполнить вручную)

## ⚠️ Особенности игрового процесса
(заполнить)

## 📜 Легенды и фольклор
(заполнить)

---
← *[К списку биомов](16_Biomes_Compendium.md)*
"""
    out_path = output_dir / f"{slug_name}.md"
    out_path.parent.mkdir(parents=True, exist_ok=True)
    with open(out_path, 'w', encoding='utf-8') as f:
        f.write(yaml)
    return out_path

# --------------------------- ОСНОВНАЯ ЛОГИКА ---------------------------
def run_generation(what):
    content = read_gdd()
    if 'ingredients' in what:
        print("🍄 Генерация карточек ингредиентов...")
        out_dir = COMPENDIUM_DIR / "Ingredients"
        ingredients = parse_ingredients(content)
        for ing in ingredients:
            p = generate_ingredient_card(ing, out_dir)
            print(f"  ✅ {p}")
        print(f"Сгенерировано {len(ingredients)} ингредиентов.\n")
    if 'bestiary' in what:
        print("👹 Генерация карточек существ...")
        out_dir = COMPENDIUM_DIR / "Bestiary"
        entities = parse_entities(content)
        for ent in entities:
            p = generate_entity_card(ent, out_dir)
            print(f"  ✅ {p}")
        print(f"Сгенерировано {len(entities)} существ.\n")
    if 'biomes' in what:
        print("🌍 Генерация карточек биомов...")
        out_dir = COMPENDIUM_DIR / "Biomes"
        biomes = parse_biome_tables(content)
        for b in biomes:
            p = generate_biome_card(b, out_dir)
            print(f"  ✅ {p}")
        print(f"Сгенерировано {len(biomes)} биомов.\n")

def interactive_menu():
    print("Что вы хотите сгенерировать?")
    print("1. Ингредиенты")
    print("2. Бестиарий (существа)")
    print("3. Биомы")
    print("4. Всё сразу")
    choice = input("Введите номер (1-4): ").strip()
    mapping = {'1': ['ingredients'], '2': ['bestiary'], '3': ['biomes'], '4': ['ingredients','bestiary','biomes']}
    if choice in mapping:
        run_generation(mapping[choice])
    else:
        print("Неверный выбор.")

if __name__ == "__main__":
    if len(sys.argv) > 1:
        what = []
        if '--all' in sys.argv:
            what = ['ingredients', 'bestiary', 'biomes']
        else:
            if '--ingredients' in sys.argv: what.append('ingredients')
            if '--bestiary' in sys.argv: what.append('bestiary')
            if '--biomes' in sys.argv: what.append('biomes')
        if not what:
            print("Укажите хотя бы один флаг: --ingredients, --bestiary, --biomes, --all")
            sys.exit(1)
        run_generation(what)
    else:
        interactive_menu()