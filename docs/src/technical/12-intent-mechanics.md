# 12. Intent механики

## 12.1 Структура intent

```python
from enum import Enum

class Intent(Enum):
    HEALING = "healing"
    CLEANSING = "cleansing"
    GROWTH = "growth"
    PROTECTION = "protection"
    HARM = "harm"
    RITUAL = "ritual"
    SACRIFICE = "sacrifice"
12.2 Влияние intent на Zaryana
python
INTENT_FACTORS = {
    Intent.HEALING: 1.2,
    Intent.CLEANSING: 1.3,
    Intent.GROWTH: 1.1,
    Intent.PROTECTION: 1.15,
    Intent.HARM: 0.8,
    Intent.RITUAL: 1.0,
    Intent.SACRIFICE: 0.6,
}

def apply_intent_to_zaryana(distortion, corruption, intent):
    Z_base = (1 - distortion) * (1 - 0.5 * abs(corruption - 0.5))
    factor = INTENT_FACTORS.get(intent, 1.0)
    Z_final = max(0.0, min(1.0, Z_base * factor))
    return Z_final
12.3 Влияние intent на эволюцию мира
python
INTENT_WORLD_MODIFIERS = {
    Intent.HEALING: {"toxicity": -0.005, "fertility": 0.005},
    Intent.CLEANSING: {"toxicity": -0.01, "disturbance": -0.005},
    Intent.GROWTH: {"fertility": 0.01, "competition": 0.005},
    Intent.PROTECTION: {"disturbance": -0.01},
    Intent.HARM: {"toxicity": 0.01, "disturbance": 0.01},
    Intent.RITUAL: {"spirit_pressure": 0.02},
    Intent.SACRIFICE: {"all": -0.02},
}

def apply_intent_to_world(world_state, result, intent):
    world_state.toxicity += 0.01 * (result.corruption - 0.5)
    world_state.fertility += 0.01 * (result.resonance - 0.5)
    world_state.disturbance += 0.005 * (result.distortion - 0.5)
    world_state.competition += 0.005 * (0.5 - result.purity)
    
    if intent in INTENT_WORLD_MODIFIERS:
        mods = INTENT_WORLD_MODIFIERS[intent]
        if "all" in mods:
            world_state.toxicity += mods["all"]
            world_state.fertility += mods["all"]
            world_state.disturbance += mods["all"]
            world_state.competition += mods["all"]
            world_state.spirit_pressure += mods["all"]
        else:
            for param, delta in mods.items():
                current = getattr(world_state, param)
                setattr(world_state, param, current + delta)
    
    world_state.toxicity = max(0.0, min(1.0, world_state.toxicity))
    world_state.fertility = max(0.0, min(1.0, world_state.fertility))
    world_state.disturbance = max(0.0, min(1.0, world_state.disturbance))
    world_state.competition = max(0.0, min(1.0, world_state.competition))
    world_state.spirit_pressure = max(0.0, min(1.0, world_state.spirit_pressure))
12.4 Конфигурационные константы
Константа	Значение
K_ZARYANA_CORR	0.5
INTENT_HEALING_FACTOR	1.2
INTENT_CLEANSING_FACTOR	1.3
INTENT_GROWTH_FACTOR	1.1
INTENT_PROTECTION_FACTOR	1.15
INTENT_HARM_FACTOR	0.8
INTENT_RITUAL_FACTOR	1.0
INTENT_SACRIFICE_FACTOR	0.6