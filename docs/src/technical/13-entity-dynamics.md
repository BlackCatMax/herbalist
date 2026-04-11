# 13. Динамика сущностей

## 13.1 Структура

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

## 13.2 Присутствие

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

## 13.3 Отношение

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

## 13.4 Миграция

PRESENCE_THRESHOLD = 0.1
PRESENCE_DISAPPEAR = 0.05

def update_entities(biome, world, season, time, player, current_entities):
    
    `current_entities: dict {spec.id: Entity} из предыдущего состояния`
    
    result = []
    for spec in BIOME_TABLE.get(biome, []):
        p = presence(spec, world, season, time, player)
        
        if spec.id in current_entities:
            *Уже присутствует – исчезает только при p < PRESENCE_DISAPPEAR*
            if p >= PRESENCE_DISAPPEAR:
                e = current_entities[spec.id]
                e.intensity = p
                e.attitude = attitude(spec, world, player)
                result.append(e)
        else:
            *Не присутствует – появляется при p >= PRESENCE_THRESHOLD*
            if p >= PRESENCE_THRESHOLD:
                result.append(new_entity(spec, p, attitude(spec, world, player)))
    return result
	
Гистерезис предотвращает мерцание: сущность, однажды появившаяся, остаётся до тех пор, пока присутствие не упадёт ниже PRESENCE_DISAPPEAR (0.05), даже если оно опустится ниже порога появления (0.1).
	
## 13.5 Влияние на мир

K_ENTITY_WORLD_INFLUENCE = 0.1

def entity_to_world(world, entity):
    `Вычисляем влияние с учётом отношения`
    `Для attitude >= 0: influence = intensity * (1 + attitude) * 0.5`
    `Для attitude < 0: influence = -intensity * (1 - attitude) * 0.5`
    if entity.attitude >= 0:
        influence = entity.intensity * (1 + entity.attitude) * 0.5
    else:
        influence = -entity.intensity * (1 - entity.attitude) * 0.5
    
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

## 13.6 Константы

Константа	Значение
PRESENCE_THRESHOLD	0.1
PRESENCE_DISAPPEAR	0.05
K_ENTITY_WORLD_INFLUENCE	0.1
K_ATTITUDE_CORRUPTION_PENALTY	0.5