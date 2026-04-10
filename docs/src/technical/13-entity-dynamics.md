# 13. Динамика сущностей

## 13.1 Структура

```python
@dataclass
class EntitySpec:
    id: str
    archetype: str  # HOST, RESTLESS, PHENOMENON, ANCESTOR
    base_tags: list
    base_presence: dict
    base_attitude: float
    optimal_toxicity: float
    optimal_fertility: float
    optimal_moisture: float
    toxicity_sensitivity: float
    fertility_sensitivity: float
    disturbance_sensitivity: float
    niche_sensitivity: float
    respect_sensitivity: float
13.2 Присутствие
python
PRESENCE_THRESHOLD = 0.1
PRESENCE_DISAPPEAR = 0.05

def presence(spec, world, season, time, player):
    bio = spec.base_presence.get(world.biome, 0.0)
    eco = (
        (1 - world.toxicity) ** spec.toxicity_sensitivity *
        world.fertility ** spec.fertility_sensitivity *
        (1 - world.disturbance) ** spec.disturbance_sensitivity *
        (1 - abs(world.moisture - spec.optimal_moisture)) ** 2
    )
    temporal = temporal_multiplier(spec, season, time)
    behavior = 1 + player.respect_score * spec.respect_sensitivity if spec.archetype in ["HOST", "ANCESTOR"] else 1.0
    return max(0.0, min(1.0, bio * eco * temporal * behavior))
13.3 Отношение
python
K_ATTITUDE_CORRUPTION_PENALTY = 0.5

def attitude(spec, world, player):
    base = spec.base_attitude
    niche = (
        1 - abs(world.toxicity - spec.optimal_toxicity) -
        0.5 * abs(world.fertility - spec.optimal_fertility) -
        0.5 * abs(world.moisture - spec.optimal_moisture)
    )
    niche = max(-1.0, min(1.0, niche)) * spec.niche_sensitivity
    respect = player.respect_score * spec.respect_sensitivity if spec.archetype in ["HOST", "ANCESTOR"] else 0.0
    corruption = -player.corruption_memory * K_ATTITUDE_CORRUPTION_PENALTY
    return max(-1.0, min(1.0, base + niche + respect + corruption))
13.4 Миграция
python
def update_entities(biome, world, season, time, player, current):
    result = []
    for spec in BIOME_TABLE.get(biome, []):
        p = presence(spec, world, season, time, player)
        if p >= PRESENCE_THRESHOLD:
            if spec.id in current:
                e = current[spec.id]
                e.intensity = p
                e.attitude = attitude(spec, world, player)
                result.append(e)
            else:
                result.append(new_entity(spec, p, attitude(spec, world, player)))
    return result
13.5 Влияние на мир
python
K_ENTITY_WORLD_INFLUENCE = 0.1

def entity_to_world(world, entity):
    influence = entity.intensity * (1 + entity.attitude) * 0.5
    for tag in entity.spec.base_tags:
        if tag in WORLD_EFFECT_MAP:
            d = WORLD_EFFECT_MAP[tag]
            world.fertility += d.fertility * influence * K_ENTITY_WORLD_INFLUENCE
            world.moisture += d.moisture * influence * K_ENTITY_WORLD_INFLUENCE
            world.competition += d.competition * influence * K_ENTITY_WORLD_INFLUENCE
            world.disturbance += d.disturbance * influence * K_ENTITY_WORLD_INFLUENCE
            world.toxicity += d.toxicity * influence * K_ENTITY_WORLD_INFLUENCE
    for p in ["fertility", "moisture", "competition", "disturbance", "toxicity"]:
        setattr(world, p, max(0.0, min(1.0, getattr(world, p))))
13.6 Константы
Константа	Значение
PRESENCE_THRESHOLD	0.1
PRESENCE_DISAPPEAR	0.05
K_ENTITY_WORLD_INFLUENCE	0.1
K_ATTITUDE_CORRUPTION_PENALTY	0.5