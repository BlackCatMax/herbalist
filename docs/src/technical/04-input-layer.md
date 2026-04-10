# 4. ВХОДНОЙ СЛОЙ

## 4.1 Источники (Sources)

Sources — любые объекты, передающие параметры в систему.

**Типы источников:**
- ингредиенты (Resources)
- окружающие сущности (Entities)
- состояние игрока (Memory, Pressure)
- контекстные элементы (передаются отдельно)

**Характеристики:**
- каждый источник независим
- не содержит логики обработки
- передаёт только данные

**Entities как источники:**
- передают свои BehaviorTags в Input Assembly
- участвуют в формировании начальных параметров
- не содержат axes и meta
- их влияние ограничено коэффициентом `K_ENTITY_INFLUENCE_MAX`

**Примечание:** Вода не является источником в этом контексте. Вода обрабатывается отдельно на этапе Water Application (раздел 5.8).

Источник не определяет результат напрямую.

## 4.2 Система сущностей (Entities System)

### Определение

**Entities** — активные агенты мира (духи, существа, проявления природы), населяющие игровое пространство.

Entities:
- являются носителями BehaviorTags
- влияют на процесс сбора ресурсов
- изменяют состояние мира
- не участвуют напрямую в алхимии
- не содержат axes или meta
- не являются Resource

**Инварианты:**
- Entity не содержит Vector
- Entity не проходит через Process System
- Entity не сохраняется в Result Vector
- Entity не генерирует distortion


### Структура Entity

Entity = {
    # Идентификация
    id: str,
    name: str,                      # локализованное имя (интерпретация)
    
    # Базовые параметры
    tags: List[BehaviorTag],        # поведенческие теги
    archetype: Archetype,           # HOST, RESTLESS, PHENOMENON, ANCESTOR
    intensity: float,               # ∈ [0, 1] — сила присутствия
    
    # Пространственная привязка
    biome_affinity: float,          # ∈ [0, 1] — привязанность к биому
    location_bound: bool,           # привязана к конкретному месту?
    
    # Временная привязка
    seasonal_profile: SeasonalProfile,
    daily_profile: DailyProfile,
    
    # Поведенческая
    base_attitude: float,           # ∈ [-1, 1] — базовая враждебность
    attitude: float,                # ∈ [-1, 1] — текущее отношение к игроку
    respect_sensitivity: float,     # ∈ [0, 1] — чувствительность к уважению
    
    # Защита/умиротворение
    pacification_items: List[ItemType],
    pacification_rituals: List[RitualType],
    
    # Жизненный цикл
    growth: float,                  # ∈ [0, 1]
    decay: float                    # ∈ [0, 1]
}

### Архетипы сущностей (Entity Archetypes)

Все сущности классифицируются по четырём архетипам, определяющим их поведение.

| Архетип | Обозначение | Сущности | Ключевая механика |
|:---|:---|:---|:---|
| **Хозяин территории** | `HOST` | Леший, Водяной, Домовой, Полевик, Банник | Отношение зависит от уважения игрока к территории |
| **Заложный покойник** | `RESTLESS` | Русалка, Упырь, Навья, Кикимора (домашняя) | Всегда враждебны, требуют защиты или упокоения |
| **Природное явление** | `PHENOMENON` | Полудница, Берегиня | Активны только в определённое время |
| **Хранитель-предок** | `ANCESTOR` | Чур, Род, Щур | Связаны с родом, а не с местом |

**Формула присутствия (универсальная):**

def calculate_presence(entity, world_state, season, time_of_day, player):
    # 1. Биомный фактор
    biome_factor = base_presence[entity.type][biome]
    
    # 2. Фактор состояния мира
    toxicity_factor = (1 - world_state.toxicity) ** TOXICITY_SENSITIVITY[entity.archetype]
    fertility_factor = world_state.fertility ** FERTILITY_SENSITIVITY[entity.archetype]
    disturbance_factor = (1 - world_state.disturbance) ** DISTURBANCE_SENSITIVITY
    
    # 3. Временной фактор
    temporal_factor = get_temporal_multiplier(entity, season, time_of_day)
    
    # 4. Поведенческий фактор (только для HOST и ANCESTOR)
    if entity.archetype in [HOST, ANCESTOR]:
        behavioral_factor = 1 + player.respect_score * entity.respect_sensitivity
    else:
        behavioral_factor = 1.0
    
    # 5. Фактор защиты (снижает влияние)
    protection_factor = 1 - get_protection_power(player, entity.pacification_items)
    
    # Итоговая сила присутствия
    presence = (biome_factor * toxicity_factor * fertility_factor 
                * temporal_factor * behavioral_factor * protection_factor)
    
    return clamp(presence, 0, 1)
	
**Чувствительность к токсичности по архетипам:**

| Архетип | TOXICITY_SENSITIVITY | Интерпретация |
|:---|:---:|:---|
| HOST (светлый) | 1.2 | Сильно страдают от загрязнения |
| HOST (нейтральный) | 1.0 | Средняя чувствительность |
| HOST (тёмный) | 0.4 | Почти не чувствительны |
| RESTLESS | 0.3 | Не чувствительны (появляются при загрязнении) |
| PHENOMENON | 1.0 | Зависят от времени, не от загрязнения |
| ANCESTOR | 0.5 | Слабая чувствительность |

def update_entity_state(entity, world_state):
    # Рост зависит от плодородия
    entity.growth = clamp(entity.growth + world_state.fertility * K_GROWTH_BASE, 0, 1)
    
    # Увядание зависит от токсичности
    entity.decay = clamp(entity.decay + world_state.toxicity * K_DECAY_BASE, 0, 1)
    
    # Интенсивность определяется балансом роста и увядания
    entity.intensity = entity.growth * (1 - entity.decay)
    
    # Производные теги
    if entity.growth > K_GROWTH_THRESHOLD:
        entity.tags.add("mature")
    elif entity.growth < K_GROWTH_WEAK:
        entity.tags.add("young")
    
    if entity.decay > K_DECAY_THRESHOLD:
        entity.tags.add("rotten")


### Жизненный цикл и состояние

**Определение:**
Жизненный цикл сущности определяется состоянием мира, а не временем.

**Обновление (при изменении World State):**

**Инвариант:** Жизненный цикл не требует тиковой симуляции. Обновление происходит только при изменении World State (после сбора или алхимии).


### Распределение сущностей по биомам

Присутствие сущности в локации определяется как функция World State, без случайности.

**Формула получения сущностей в локации:**

def get_entities_in_location(location, world_state, season, time_of_day, player):
    entities = []
    
    for entity_type in location.biome.possible_entities:
        presence = calculate_presence(entity_type, world_state, season, time_of_day, player)
        
        if presence > PRESENCE_THRESHOLD:  # 0.1
            entity = create_entity(entity_type)
            entity.intensity = presence
            entity.attitude = calculate_attitude(entity, world_state, player)
            entities.append(entity)
    
    return entities
	
**Базовое распределение по биомам (веса присутствия):**

| Сущность | Архетип | Лес | Болото | Степь | Тайга | Горы | Дом |
|:---|:---|:---:|:---:|:---:|:---:|:---:|:---:|
| Леший | HOST (светлый) | 0.9 | 0.0 | 0.0 | 0.7 | 0.1 | 0.0 |
| Водяной | HOST (тёмный) | 0.1 | 0.9 | 0.0 | 0.2 | 0.0 | 0.0 |
| Кикимора болотная | RESTLESS | 0.1 | 0.8 | 0.0 | 0.1 | 0.0 | 0.0 |
| Кикимора домашняя | RESTLESS | 0.0 | 0.0 | 0.0 | 0.0 | 0.0 | 0.6 |
| Русалка | RESTLESS | 0.1 | 0.5 | 0.0 | 0.1 | 0.0 | 0.0 |
| Упырь | RESTLESS | 0.2 | 0.3 | 0.2 | 0.2 | 0.1 | 0.1 |
| Полудница | PHENOMENON | 0.1 | 0.0 | 0.7 | 0.0 | 0.0 | 0.0 |
| Полевик | HOST (нейтральный) | 0.1 | 0.0 | 0.8 | 0.0 | 0.0 | 0.0 |
| Домовой | HOST (светлый) | 0.0 | 0.0 | 0.0 | 0.0 | 0.0 | 1.0 |
| Банник | HOST (нейтральный) | 0.0 | 0.0 | 0.0 | 0.0 | 0.0 | 0.5 |
| Навья | RESTLESS | 0.2 | 0.3 | 0.2 | 0.2 | 0.1 | 0.1 |
| Берегиня | PHENOMENON | 0.3 | 0.5 | 0.0 | 0.2 | 0.0 | 0.0 |

**Примечание:** Значения в таблице — базовые веса. Итоговое присутствие рассчитывается по формуле из раздела 4.2.3.

def apply_entity_influence(resource, entities_in_location):
    for entity in entities_in_location:
        # Вес влияния ограничен
        influence = entity.intensity * K_ENTITY_INFLUENCE
        influence = clamp(influence, 0, K_ENTITY_INFLUENCE_MAX)
        
        for tag in entity.tags:
            resource.add_tag(tag, weight=influence)
			
**Порядок применения в Harvesting (раздел 6.1):**
1. Копирование тегов от собираемой сущности
2. Добавление тегов контекста (время, сезон, погода)
3. **Добавление тегов от окружающих сущностей**
4. Расчёт коэффициентов интенсивности и качества

**Конфигурация:**
| Константа | Значение | Описание |
|-----------|----------|----------|
| `K_ENTITY_INFLUENCE` | 0.5 | Базовый множитель влияния |
| `K_ENTITY_INFLUENCE_MAX` | 0.3 | Максимальное влияние одной сущности |


### Влияние сущности на мир (Entity → World)

**Определение:**
Сущности изменяют состояние мира **в момент сбора ресурса**, а не непрерывно.

**Формализация (применяется в момент сбора, после Harvest → World):**

### Влияние сущности на сбор (Entity → Harvest)

**Определение:**
Сущности, присутствующие в локации сбора, добавляют свои теги к собираемому ресурсу.

**Формализация (применяется в момент сбора):**

def apply_entity_world_influence(world_state, entities_in_location):
    for entity in entities_in_location:
        for tag in entity.tags:
            if tag in WORLD_EFFECT_MAP:
                Δ = WORLD_EFFECT_MAP[tag]
                world_state.fertility += Δ.fertility * entity.intensity * K_ENTITY_WORLD_INFLUENCE
                world_state.moisture += Δ.moisture * entity.intensity * K_ENTITY_WORLD_INFLUENCE
                world_state.competition += Δ.competition * entity.intensity * K_ENTITY_WORLD_INFLUENCE
                world_state.disturbance += Δ.disturbance * entity.intensity * K_ENTITY_WORLD_INFLUENCE
                world_state.toxicity += Δ.toxicity * entity.intensity * K_ENTITY_WORLD_INFLUENCE
    
    # Нормализация
    for param in world_state:
        world_state[param] = clamp(world_state[param], 0, 1)


**Конфигурация:**
| Константа | Значение | Описание |
|-----------|----------|----------|
| `K_ENTITY_WORLD_INFLUENCE` | 0.1 | Множитель влияния сущности на мир |


### Обратная связь от сбора к миру (Harvest → World)

**Определение:**
Процесс сбора ресурса изменяет состояние мира.

**Формализация (применяется в момент сбора):**

def apply_harvest_world_influence(world_state, harvested_resource):
    for tag in harvested_resource.tags:
        if tag in WORLD_EFFECT_MAP:
            Δ = WORLD_EFFECT_MAP[tag]
            world_state.fertility += Δ.fertility * harvested_resource.intensity * K_HARVEST_IMPACT
            world_state.moisture += Δ.moisture * harvested_resource.intensity * K_HARVEST_IMPACT
            world_state.competition += Δ.competition * harvested_resource.intensity * K_HARVEST_IMPACT
            world_state.disturbance += Δ.disturbance * harvested_resource.intensity * K_HARVEST_IMPACT
            world_state.toxicity += Δ.toxicity * harvested_resource.intensity * K_HARVEST_IMPACT
    
    # Нормализация
    for param in world_state:
        world_state[param] = clamp(world_state[param], 0, 1)


**Конфигурация:**
| Константа | Значение | Описание |
|-----------|----------|----------|
| `K_HARVEST_IMPACT` | 0.1 | Множитель влияния сбора на мир |


### Отображение мира на сущности (World → Entity)

**Определение:**
Состояние мира определяет свойства существующих сущностей.

**Формализация (применяется после изменения World State):**

def update_entities_from_world(entities_in_location, world_state):
    for entity in entities_in_location:
        # Обновление жизненного цикла
        entity.growth = clamp(entity.growth + world_state.fertility * K_GROWTH_BASE, 0, 1)
        entity.decay = clamp(entity.decay + world_state.toxicity * K_DECAY_BASE, 0, 1)
        
        # Обновление интенсивности
        entity.intensity = entity.growth * (1 - entity.decay)
        
        # Обновление производных тегов
        if entity.growth > K_GROWTH_THRESHOLD:
            entity.tags.add("mature")
        elif entity.growth < K_GROWTH_WEAK:
            entity.tags.add("young")
        
        if entity.decay > K_DECAY_THRESHOLD:
            entity.tags.add("rotten")

### Система отношения (Attitude System)

**Определение:**
Отношение сущности к игроку зависит от действий игрока, состояния мира и памяти коррупции.

**Формула:**

def calculate_attitude(entity, world_state, player):
    # Базовая установка от архетипа
    base = entity.base_attitude
    
    # Модификатор от уважения (для HOST)
    if entity.archetype == HOST:
        respect_mod = player.respect_score * entity.respect_sensitivity
    else:
        respect_mod = 0
    
    # Модификатор от памяти коррупции
    corruption_mod = -player.corruption_memory * K_ATTITUDE_CORRUPTION_PENALTY
    
    # Модификатор от состояния мира
    if entity.archetype == HOST:
        if world_state.toxicity > 0.5:
            world_mod = -0.3
        elif world_state.fertility > 0.7:
            world_mod = +0.2
        else:
            world_mod = 0
    else:
        world_mod = 0
    
    # Итоговое отношение
    attitude = base + respect_mod + corruption_mod + world_mod
    attitude = clamp(attitude, -1, 1)
    
    return attitude


**Поведенческие режимы по архетипам:**

| Архетип | Attitude < -0.5 | -0.5 ≤ Attitude ≤ 0.5 | Attitude > 0.5 |
|:---|:---|:---|:---|
| HOST | Разгневанный (вредит) | Нейтральный (игнорирует) | Благосклонный (помогает) |
| RESTLESS | Агрессивный (атакует) | Враждебный (пугает) | — (не бывает) |
| PHENOMENON | Карающий | Предупреждающий | — (не бывает) |
| ANCESTOR | Гневный | Равнодушный | Защищающий |

**Конфигурация:**
| Константа | Значение | Описание |
|-----------|----------|----------|
| `K_ATTITUDE_CORRUPTION_PENALTY` | 0.5 | Штраф к отношению от памяти коррупции |


### Временная динамика (Time Dynamics)

**Определение:**
Активность некоторых сущностей зависит от времени суток и сезона.

**Сезонные профили:**

| Сущность | Зима | Весна | Лето | Осень | Особые окна |
|:---|:---:|:---:|:---:|:---:|:---|
| Леший | 0.0 (спит) | 0.7 | 1.0 | 1.0 | Бешенство 17 окт (×1.5) |
| Водяной | 0.0 (спит) | 1.5 (гневный) | 1.0 | 0.5 | — |
| Русалка | 0.2 | 0.5 | 1.5 | 0.3 | Русальная неделя (×2) |
| Навья | 0.1 | 0.5 | 1.2 | 0.5 | Русальная неделя (×2) |
| Полудница | 0.0 | 0.0 | 1.0 (полдень) | 0.0 | Только полдень |
| Полевик | 0.3 | 0.7 | 1.0 | 0.7 | — |
| Домовой | 1.0 | 1.0 | 1.0 | 1.0 | — |
| Упырь | 0.8 | 1.0 | 1.0 | 0.8 | Ночь (×1.5) |

**Суточные профили:**

| Сущность | Ночь (0-0.25) | Утро (0.25-0.4) | День (0.4-0.6) | Вечер (0.6-0.75) | Полдень (~0.5) |
|:---|:---:|:---:|:---:|:---:|:---:|
| Полудница | 0.0 | 0.0 | 1.0 | 0.0 | 2.0 |
| Упырь | 1.5 | 0.5 | 0.0 | 0.5 | 0.0 |
| Русалка | 0.8 | 1.0 | 0.5 | 1.2 | 0.5 |
| Леший | 0.7 | 1.0 | 1.0 | 1.0 | 1.0 |

**Формула временного множителя:**

def get_temporal_multiplier(entity, season, time_of_day):
    # Сезонный множитель
    seasonal = entity.seasonal_profile[season]
    
    # Суточный множитель (интерполяция между ключевыми точками)
    daily = get_daily_multiplier(entity, time_of_day)
    
    # Особые окна
    special = 1.0
    for window in entity.special_windows:
        if is_active(window, season, time_of_day):
            special *= window.multiplier
    
    return seasonal * daily * special


### Полный цикл Entities System

┌─────────────────────────────────────────────────────────────────────────────────────────────┐
│                                                                                             │
▼                                                                                             │
┌───────────────────────────────┐                                                             │
│        WorldState             │                                                             │
│  (toxicity, fertility, etc.)  │                                                             │
└───────────────┬───────────────┘                                                             │
                │                                                                             │
                │ (при загрузке / после изменения WorldState)                                 │
                ▼                                                                             │
        ┌───────────────┐                                                                     │
        │  get_entities │  ← полный пересчёт присутствия                                      │
        │   (4.2.5)     │                                                                     │
        └───────┬───────┘                                                                     │
                │                                                                             │
                ▼                                                                             │
        ┌───────────────┐                                                                     │
        │   Entities    │                                                                     │
        └───────┬───────┘                                                                     │
                │                                                                             │
                │                         ┌─────────────────────────────────────────┐         │
                │                         │                                         │         │
                ▼                         ▼                                         │         │
        ┌───────────────┐         ┌───────────────┐                                 │         │
        │Entity→Harvest │         │Entity→World   │                                 │         │
        │  (4.2.6)      │         │  (4.2.7)      │                                 │         │
        └───────┬───────┘         └───────┬───────┘                                 │         │
                │                         │                                         │         │
                ▼                         │                                         │         │
        ┌───────────────┐                 │                                         │         │
        │  Harvesting   │                 │                                         │         │
        │    (6.1)      │                 │                                         │         │
        └───────┬───────┘                 │                                         │         │
                │                         │                                         │         │
                ▼                         │                                         │         │
        ┌───────────────┐                 │                                         │         │
        │   Resource    │                 │                                         │         │
        │ (pre-process) │                 │                                         │         │
        └───────┬───────┘                 │                                         │         │
                │                         │                                         │         │
                ▼                         │                                         │         │
        ┌───────────────┐                 │                                         │         │
        │   Alchemy     │                 │                                         │         │
        │ (Process Sys) │                 │                                         │         │
        └───────┬───────┘                 │                                         │         │
                │                         │                                         │         │
                ▼                         │                                         │         │
        ┌───────────────┐                 │                                         │         │
        │Harvest→World  │─────────────────┘                                         │         │
        │  (4.2.8)      │                                                           │         │
        └───────┬───────┘                                                           │         │
                │                                                                   │         │
                ▼                                                                   │         │
        ┌───────────────┐                                                           │         │
        │World Evolution│                                                           │         │
        │    (8.3)      │───────────────────────────────────────────────────────────┘         │
        └───────────────┘                                                                     │
                │                                                                             │
                │ (глобальное изменение мира)                                                 │
                │                                                                             │
                └─────────────────────────────────────────────────────────────────────────────┘


**Три ключевые петли:**

| Петля | Когда | Что делает |
|:---|:---|:---|
| **Entity → Harvest** | При сборе | Сущности добавляют теги ресурсу |
| **Entity → World** | При сборе | Сущности изменяют мир (локально) |
| **Harvest → World** | При сборе | Сбор изменяет мир (локально) |
| **World Evolution** | После алхимии | Алхимия изменяет мир (глобально) |
| **World → Entity** | После изменения мира | Пересчёт присутствия сущностей |

---

### Конфигурационные параметры

| Константа | Значение | Описание |
|-----------|----------|----------|
| `PRESENCE_THRESHOLD` | 0.1 | Порог присутствия сущности |
| `K_ENTITY_INFLUENCE` | 0.5 | Базовый множитель влияния на сбор |
| `K_ENTITY_INFLUENCE_MAX` | 0.3 | Максимальное влияние одной сущности |
| `K_ENTITY_WORLD_INFLUENCE` | 0.1 | Множитель влияния сущности на мир |
| `K_HARVEST_IMPACT` | 0.1 | Множитель влияния сбора на мир |
| `K_ATTITUDE_CORRUPTION_PENALTY` | 0.5 | Штраф к отношению от памяти коррупции |
| `K_GROWTH_BASE` | 0.1 | Базовая скорость роста сущности |
| `K_DECAY_BASE` | 0.05 | Базовая скорость увядания |
| `K_GROWTH_THRESHOLD` | 0.7 | Порог добавления тега `mature` |
| `K_DECAY_THRESHOLD` | 0.7 | Порог добавления тега `rotten` |
| `K_GROWTH_WEAK` | 0.3 | Порог добавления тега `young` |
| `TOXICITY_SENSITIVITY_LIGHT` | 1.2 | Чувствительность светлых HOST |
| `TOXICITY_SENSITIVITY_NEUTRAL` | 1.0 | Чувствительность нейтральных HOST |
| `TOXICITY_SENSITIVITY_DARK` | 0.4 | Чувствительность тёмных HOST |
| `TOXICITY_SENSITIVITY_RESTLESS` | 0.3 | Чувствительность RESTLESS |
| `DISTURBANCE_SENSITIVITY` | 0.5 | Чувствительность к нарушенности |
| `FERTILITY_SENSITIVITY_HOST` | 1.0 | Чувствительность HOST к плодородию |
| `FERTILITY_SENSITIVITY_RESTLESS` | 0.2 | Чувствительность RESTLESS к плодородию |

### Инварианты системы сущностей

- Entity не содержит axes и meta
- Entity не проходит через Process System
- Влияние Entity на мир слабее влияния алхимии (коэффициент 0.1)
- Все преобразования детерминированы
- Отсутствуют ветвления в математических операциях (все условия через параметры)
- Entity не нарушает единую модель Vector
- Distortion не передаётся через Entity
- Присутствие сущности пересчитывается только при изменении World State, без тиков
- Нет трансформации сущностей — только смена состава при смене биома


## 4.3 Ресурсы (до обработки)

Resource (pre-process) — структурированное представление источника перед обработкой.

Содержит:
- BehaviorTags
- количественные параметры (`intensity`, `quality`)
- локальные modifiers (если есть)

Ресурс:
- ещё не является Vector
- не содержит axes и meta
- является входной единицей для системы
- `distortion` из ресурса игнорируется (обнуляется)

## 4.4 Поведенческие теги (BehaviorTags)

BehaviorTag — атомарное описание свойства.

Характеристики:
- не содержит числового значения
- не зависит от контекста
- может комбинироваться с другими тегами

Примеры:
- `healing`, `poison`, `night`, `forest`, `water` и др.
**Примечание - Тег `water` не используется для воды-основы; вода обрабатывается отдельно (см. раздел 5.8)**

BehaviorTags являются базой для последующего преобразования. Полная библиотека тегов и их маппинг приведены в разделе 10.5.

## 4.5 Отображение в оси (Axis Mapping)

Преобразует BehaviorTags в числовое представление.

Функция:
BehaviorTag → Axes

Каждый тег задаёт вклад в оси:
axes += mapping(tag) × weight

где:
- `mapping(tag)` — предопределённый вектор приращений (Δbody, Δmind, Δspirit, Δnature)
- `weight` — сила влияния (обычно `intensity` ресурса)

Дополнительно теги могут влиять на `potency` и `corruption` (аддитивные бонусы).

Требования:
- mapping фиксирован (см. таблицу в разделе 10.5)
- не зависит от контекста
- не содержит условий

Результат: формируется начальный вектор осей.

## 4.6 Система сборки входных данных (Input Assembly)

Объединяет все источники в единую структуру **GameplayParams**.

### Этапы

**1. Сбор источников**

Собираются:
- Resources (ингредиенты)
- Entities (активные сущности в локации)
- Memory System (clarity, corruption_memory, respect_score)
- Pressure (давление процесса и среды)

Контекст (time, world, ecosystem) передаётся отдельно и не участвует в вычислениях на этом этапе.

**2. Сбор BehaviorTags от всех источников**

AllTags = []

# Теги от ресурсов с их весами
for resource in resources:
    for tag, weight in resource.tags:
        AllTags.append((tag, weight * resource.intensity))

# Теги от сущностей с ограничением
for entity in entities_in_location:
    influence = entity.intensity * K_ENTITY_INFLUENCE
    influence = clamp(influence, 0, K_ENTITY_INFLUENCE_MAX)
    for tag in entity.tags:
        AllTags.append((tag, influence))

**3. Поведенческие теги → оси (BehaviorTags → Axes)**

Для каждого источника:
`axes += mapping(tag) × weight`
где:
- `mapping(tag)` — фиксированный вклад в оси
- `weight` — интенсивность (например, intensity ресурса)

axes = {body: 0, mind: 0, spirit: 0, nature: 0}

for tag, weight in AllTags:
    if tag in AXIS_MAPPING:
        axes.body   += AXIS_MAPPING[tag].body * weight
        axes.mind   += AXIS_MAPPING[tag].mind * weight
        axes.spirit += AXIS_MAPPING[tag].spirit * weight
        axes.nature += AXIS_MAPPING[tag].nature * weight

**4. Агрегация осей**

`axes = Σ(axes_i × weight_i)`
где веса определяются Alchemy System (см. раздел 6.2).

**5. Вычисление meta-параметров**

**Potency** — средневзвешенное с бонусами от тегов
potency = weighted_mean(potency_i) + bonus_from_tags

**Purity** — однородность осей
variance = (
    (axes.body - mean)² +
    (axes.mind - mean)² +
    (axes.spirit - mean)² +
    (axes.nature - mean)²
) / 4
purity = 1 - variance

**Stability** — максимальный разброс осей
max_diff = max(axes) - min(axes)
stability = 1 - (max_diff / 2)

**Resonance** — согласованность с гармонией
resonance = (sum(axes) + 4) / 8

**Corruption** — средневзвешенное с бонусами от тегов
corruption = weighted_mean(corruption_i) + bonus_from_tags

Уточнения

- `variance(axes)` вычисляется с равными весами (1/4)
- `resonance` не clamp'ится на этом этапе
- все значения могут выходить за диапазоны

**6. Формирование modifiers**

Формируются:
- `clarity` — из Memory System
- `pressure` — из процесса и среды
- `corruption_memory` — из Memory System

modifiers = {
    "clarity": memory.clarity,
    "pressure": pressure_value,
    "corruption_memory": memory.corruption_memory,
    "respect_score": memory.respect_score
}

Modifiers:
- не входят в Vector
- не нормализуются
- используются только в Process System

**7. Передача контекста**

Контекст (time, world, ecosystem, entities):
- не участвует в вычислениях
- передаётся как отдельная структура

Контекст включает:
- time — time_of_day, season, moon_phase
- world — toxicity, altitude
- ecosystem — fertility, moisture, competition, disturbance
- entities — список активных сущностей в локации (для использования в Process System)

**Нормализация**
- clamp **не применяется**
- допускаются экстремальные значения
- финальная нормализация выполняется только после Process System

**Результат**

`GameplayParams = {axes, meta, modifiers, context}`

**Ограничения**
- отсутствуют ветвления (if/else)
- все операции линейны или агрегирующие
- отсутствуют нелинейные преобразования
- distortion из источников игнорируется

## Инварианты этапа

- формируется единый входной формат GameplayParams
- отсутствует логика результата (нет ветвлений)
- не создаются новые типы параметров
- структура Vector формируется, но ещё не финализирована
- Entities влияют только через BehaviorTags, не внося axes/meta напрямую
- влияние Entities ограничено коэффициентом K_ENTITY_INFLUENCE_MAX
- все операции линейны или агрегирующие
- отсутствует clamp на этом этапе

Результат этапа — полностью подготовленные входные данные для Process System.