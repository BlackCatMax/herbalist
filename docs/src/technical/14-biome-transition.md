# 14. Смена биомов

## 14.1 Определение биома

```python
def get_biome(world):
    if world.toxicity >= 0.7:
        return "swamp"
    if world.fertility >= 0.7 and world.toxicity <= 0.3:
        return "forest"
    if world.disturbance >= 0.6:
        return "steppe"
    if world.fertility <= 0.3:
        return "wasteland"
    return "mixed"
14.2 Гистерезис
python
TRANSITIONS = {
    "mixed_to_swamp": {"toxicity": 0.7},
    "swamp_to_mixed": {"toxicity": 0.5},
    "mixed_to_forest": {"fertility": 0.7, "toxicity": 0.3},
    "forest_to_mixed": {"fertility": 0.5, "toxicity": 0.4},
    "mixed_to_steppe": {"disturbance": 0.6},
    "steppe_to_mixed": {"disturbance": 0.4},
    "mixed_to_wasteland": {"fertility": 0.3},
    "wasteland_to_mixed": {"fertility": 0.5},
}

def should_transition(current, world):
    key = f"{current}_to_{get_biome(world)}"
    if key not in TRANSITIONS:
        return False
    thresholds = TRANSITIONS[key]
    for param, threshold in thresholds.items():
        if getattr(world, param) < threshold:
            return False
    return True
14.3 Последствия
python
def apply_biome_change(biome, location):
    location.available_resources = BIOME_RESOURCES[biome]
    location.biome = biome
    # визуал и звук через события
14.4 Константы
Константа	Значение
SWAMP_TOXICITY_THRESHOLD	0.7
SWAMP_TOXICITY_RECOVER	0.5
FOREST_FERTILITY_THRESHOLD	0.7
FOREST_FERTILITY_RECOVER	0.5
STEPPE_DISTURBANCE_THRESHOLD	0.6
STEPPE_DISTURBANCE_RECOVER	0.4
WASTELAND_FERTILITY_THRESHOLD	0.3
WASTELAND_FERTILITY_RECOVER	0.5