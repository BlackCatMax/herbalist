# 🔒 23. Прогрессия и финальное состояние — UPDATED

---

## 🔒 23.1 Назначение

Глобальная прогрессия определяется через:

```text id="k2m9v4"
world_coherence
```

---

Это:

> интегральная мера “собранности” мира

---

---

## 🔒 23.2 Общая форма

---

```python
def world_coherence(biomes: Dict[Coord, BiomeState]) -> float: ...
```

---

```python
def is_buyan_accessible(biomes: Dict[Coord, BiomeState]) -> bool: ...
```

---

---

## 🔒 23.3 Определение world_coherence (UPDATED)

---

```text id="p7k3m2"
world_coherence = Mean_i(C_i)
```

---

где:

```text id="v4k8m1"
C_i = coherence локального состояния биома
```

---

---

## 🔒 23.4 Локальная coherence

---

```text id="t2m7k5"
C_i = f(
    distortion,
    stability_memory,
    history_purity,
    history_resonance
)
```

---

---

### Каноническая форма:

```text id="n8k3m6"
C_i =
(1 - accumulated_distortion)
· stability_memory
· history_purity
· history_resonance
```

---

---

## 🔒 23.5 Связь с CORE системой

---

Обрати внимание:

```text id="z5k2m9"
coherence напрямую связан с Zaryana и Morok
```

---

---

### Связи:

* `1 - distortion` → анти-Morok
* `stability` → collapse
* `purity` → очистка
* `resonance` → согласованность

---

---

👉 это НЕ новая метрика — это:

```text id="y7k2m4"
агрегат уже существующих переменных
```

---

---

## 🔒 23.6 Нормализация

---

```text id="m4k8v2"
world_coherence ∈ [0,1]
```

---

---

## 🔒 23.7 Доступ к Буяну

---

```python
def is_buyan_accessible(biomes: Dict[Coord, BiomeState]) -> bool:
    return world_coherence(biomes) >= THRESHOLD
```

---

---

### Порог:

```text id="u4k9m2"
THRESHOLD = 0.7
```

---

⚙️ **SEMI**

---

---

## 🔒 23.8 Интерпретация

---

Буян открывается, когда:

> мир становится **достаточно согласованным**

---

---

👉 важно:

* это не “очистка мира до идеала”
* а достижение **устойчивого равновесия**

---

---

## 🔒 23.9 Пространственный аспект (ВАЖНО)

---

Можно (и рекомендуется) учитывать не только среднее:

---

### Вариант усиления:

```text id="x8k3m5"
world_coherence =
w1 · mean(C_i)
+ w2 · min(C_i)
```

---

---

👉 это предотвращает:

* “локально чистый мир + одна сломанная зона”

---

---

⚙️ **SEMI**

---

---

## 🔒 23.10 Связь с действиями игрока

---

```text id="k9m2v7"
Player → Action → Δ → Biome → coherence → world_coherence
```

---

---

👉 полный цикл:

```text id="p2k8m6"
игрок не “заполняет шкалу”
он реально меняет мир
```

---

---

## 🔒 23.11 Инварианты

---

```text id="v3k7m1"
coherence выводится только из существующих параметров
```

```text id="t8k2m4"
нет отдельной “магической прогрессии”
```

```text id="q4k9m2"
вся прогрессия — следствие системы
```

---

---

# ⚙️ SEMI

---

Настраиваются:

```text id="z1k9m3"
формула C_i
агрегация world_coherence
THRESHOLD (по умолчанию 0.7)
```

---