# 🧪 21. Капища (Shrines) — UPDATED

---

## 🧪 21.1 Назначение

Капища — это:

```text id="k2m9v4"
внешние модификаторы системы
```

---

Они:

* не входят в CORE
* не нарушают pipeline
* усиливают или стабилизируют процессы

---

---

## 🧪 21.2 Структура

---

```python
@dataclass
class Shrine:
    active: bool = True
    power: float = 1.0
```

---

---

## 🧪 21.3 Общий принцип

---

Капища влияют на систему **опосредованно**, через модификацию:

```text id="p7k3m2"
Context
Δ
Biome interaction
```

---

---

## 🧪 21.4 Допустимые точки воздействия

---

### 1. Влияние на Context

---

```text id="v4k8m1"
pressure ↓
clarity ↑
```

---

Пример:

```text id="t2m7k5"
pressure *= (1 - shrine.power * α)
clarity  += shrine.power * β
```

---

---

### 2. Влияние на Morok

---

Капища НЕ изменяют Morok напрямую, но:

```text id="n8k3m6"
уменьшают входные параметры λ_raw
```

---

👉 через:

* clarity
* corruption_memory

---

---

### 3. Усиление Δ (разрешённое)

---

```text id="z5k2m9"
Δ *= (1 + shrine.power * γ)
```

---

👉 это тот самый случай:

```text id="y7k2m4"
effective_factor > 1 — разрешён
```

---

---

### 4. Влияние на BiomeState

---

```text id="m4k8v2"
resistance ↓
```

---

Пример:

```text id="u4k9m2"
resistance *= (1 - shrine.power * δ)
```

---

---

### 5. Влияние на diffusion

---

```text id="x8k3m5"
decay ↑ (локально)
```

---

👉 ускоряет распространение “правильного” состояния

---

---

## 🧪 21.5 Инварианты (ОЧЕНЬ ВАЖНО)

---

Капища НЕ могут:

```text id="k9m2v7"
вводить новые нелинейности
```

```text id="p2k8m6"
ломать deterministic pipeline
```

```text id="v3k7m1"
обходить Δ систему
```

```text id="t8k2m4"
изменять distortion напрямую
```

---

---

## 🧪 21.6 Пространственное влияние

---

Капища действуют локально:

```text id="q4k9m2"
radius → falloff
```

---

Пример:

```text id="z1k9m3"
effect = power * exp(-distance / R)
```

---

---

## 🧪 21.7 Связь с системой

---

Теперь:

```text id="k6m2v8"
Shrine
→ модифицирует Context / Δ / Biome
→ влияет на pipeline
```

---

---

## 🧪 21.8 Интерпретация

---

Капище — это:

> не “магическая кнопка”
> а **локальное изменение правил мира**

---