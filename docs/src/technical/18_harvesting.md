# 🔒 18. Сбор ресурсов (детерминированный) — UPDATED

---

## 🔒 18.1 Назначение

Система сбора ресурсов:

* генерирует ингредиенты
* полностью детерминирована
* зависит от:

  * координаты
  * состояния биома
  * градиента

---

```text id="k2m9v4"
Ingredient = f(BiomeState, Gradient, Seed)
```

---

---

## 🔒 18.2 Детерминизм (CORE)

---

### Источник seed

```python
seed = deterministic_hash(
    world_seed,
    coord[0],
    coord[1],
    biome.time_of_day,
    biome.season
)
```

---

### Инвариант

```text id="p7k3m2"
одинаковый seed → одинаковый результат
```

---

👉 гарантирует:

* воспроизводимость
* отсутствие “скрытого рандома”

---

---

## 🔒 18.3 Генерация ингредиента (UPDATED)

---

```python
def generate_ingredient(
    biome: BiomeState,
    grad: Dict[str, float],
    world_seed: int,
    coord: Coord
) -> Tuple[Direction, Magnitude, Meta]:
```

---

👉 теперь:

```text id="v4k8m1"
Direction = d
Magnitude = m
Meta = Meta
```

---

---

## 🔒 18.4 Формирование направления (d)

---

Направление зависит от:

* параметров биома
* градиента

---

```text id="t2m7k5"
d_raw = f(biome, grad, seed)
d = Normalize(d_raw)
```

---

---

### Интерпретация:

* биом задаёт “базовый архетип”
* градиент — “направление мутации”

---

---

## 🔒 18.5 Формирование magnitude (m)

---

```text id="n8k3m6"
m = f(
    fertility,
    (1 - toxicity),
    gradient_magnitude,
    seed
)
```

---

---

👉 смысл:

* плодородие → усиливает
* токсичность → ослабляет
* градиент → добавляет нестабильность

---

---

## 🔒 18.6 Формирование Meta

---

```text id="z5k2m9"
Meta = f(
    biome,
    grad,
    seed
)
```

---

---

### Примеры зависимостей:

* purity ← (1 - toxicity)
* corruption ← accumulated_distortion
* resonance ← history_resonance
* stability ← stability_memory

---

---

## 🔒 18.7 Использование normal_random

---

```python
value = normal_random(seed + offset, mean, std)
```

---

Инварианты:

```text id="y7k2m4"
все случайности → только через seed
```

---

---

## 🔒 18.8 Связь с pipeline

---

```text id="m4k8v2"
Ingredient → fold → S₀ → Δ → pipeline
```

---

👉 ингредиенты:

* не применяются напрямую
* всегда проходят через систему

---

---

## 🔒 18.9 Влияние сбора на мир (CORE)

---

```python
def apply_harvest_impact(
    biome: BiomeState,
    grad: Dict[str, float],
    intensity: float = 0.1
) -> BiomeState:
```

---

---

## 🔒 18.10 Принцип воздействия

---

Сбор — это:

```text id="u4k9m2"
Action → Intent → Δ → BiomeState update
```

---

---

### ❗ Критично

Сбор НЕ изменяет BiomeState напрямую.

---

👉 он создаёт:

```text id="x8k3m5"
ΔBiome
```

---

---

## 🔒 18.11 Пример воздействия

---

```text id="k9m2v7"
fertility -= intensity * (1 + gradient_magnitude)
toxicity  += intensity * extraction_factor
disturbance += intensity
```

---

---

## 🔒 18.12 Связь с градиентом

---

```text id="p2k8m6"
gradient усиливает воздействие
```

---

👉 если зона нестабильна:

* сбор сильнее “ломает” её

---

---

## 🔒 18.13 Инварианты

---

```text id="v3k7m1"
сбор детерминирован
```

```text id="t8k2m4"
все изменения проходят через Δ
```

```text id="q4k9m2"
нет прямых мутаций состояния
```

---

---

# ⚙️ SEMI

---

Настраиваются:

```text id="z1k9m3"
формулы генерации d, m, Meta
коэффициенты влияния harvest
```

---

---

# 🧪 OPEN

---

* распределения
* визуал
* названия
* редкость

---