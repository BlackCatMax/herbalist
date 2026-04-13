# 🔒 24. Storage Layer (Инвентарь и порча) — UPDATED

---

## 🔒 24.1 Назначение

Storage Layer описывает:

```text
эволюцию состояния предмета во времени без участия игрока
```

---

👉 это:

* та же система трансформации
* но driven временем (`dt`)

---

---

## 🔒 24.2 Ключевой принцип (CORE)

---

```text
используется тот же pipeline, что и алхимия
```

---

👉 никаких отдельных правил:

```text
Inventory evolution = Δ pipeline
```

---

---

## 🔒 24.3 Структура данных

---

```python
@dataclass
class InventoryItem:
    vector: Vector   # (d, m, Meta)
    last_update_time: float
```

---

---

```python
@dataclass
class StorageContext:
    context: Context
    modifiers: Modifiers
    k_time: float
```

---

---

## 🔒 24.4 Эффективное время

---

```python
def compute_dt_effective(dt_real: float, k_time: float) -> float:
    return dt_real * k_time
```

---

---

👉 смысл:

```text
время → масштабирует Δ
```

---

---

## 🔒 24.5 Ключевое правило (КРИТИЧНО)

---

```text
scale_delta применяется ПОСЛЕ всех линейных стадий, ДО Morok
```

---

---

👉 полный порядок:

```text
fold (опционально, если нужно)
→ delta
→ context
→ water
→ axis_sign
→ interaction
→ scale_delta   ← ВАЖНО
→ morok
→ apply
→ resolve
→ zaryana
→ clamp
```

---

---

## 🔒 24.6 scale_delta (UPDATED)

---

```python
def scale_delta(d: Vector, factor: float) -> Vector:
```

---

Применение:

```text
Δd *= factor
Δm *= factor
ΔMeta *= factor
```

---

👉 важно:

```text
не создаёт новых нелинейностей
```

---

---

## 🔒 24.7 Основная функция

---

```python
def evolve_item(
    item: InventoryItem,
    now: float,
    storage_ctx: StorageContext
) -> InventoryItem:
```

---

---

### 🔹 Шаг 1: вычисление времени

```text
dt_real = now - item.last_update_time
dt_eff = compute_dt_effective(dt_real, k_time)
```

---

---

### 🔹 Шаг 2: подготовка состояния

```text
S = item.vector
Δ = delta(S)
```

---

---

### 🔹 Шаг 3: линейные стадии (симметрия)

---

```text
context(S, Δ)
water(S, Δ)
axis_sign(S, Δ)
interaction(S, Δ)
```

---

👉 инвариант:

```text
применяется одинаково к S и Δ
```

---

---

### 🔹 Шаг 4: масштабирование временем

---

```text
Δ = scale_delta(Δ, dt_eff)
```

---

---

### 🔹 Шаг 5: Morok

---

```text
Δ' = morok(Δ, S)
```

---

---

### 🔹 Шаг 6: применение

---

```text
S' = apply_delta(S, Δ')
```

---

---

### 🔹 Шаг 7: стабилизация

---

```text
conflict_resolution(S')
zaryana(S')
final_clamp(S')
```

---

---

### 🔹 Шаг 8: обновление item

---

```text
item.vector = S'
item.last_update_time = now
```

---

---

## 🔒 24.8 Роль modifiers

---

```text
modifiers → изменяют context или Δ
```

---

---

Примеры:

```text
temperature → влияет на k_time
container_type → влияет на moisture / stability
sealed → снижает distortion
```

---

---

⚙️ **SEMI**

---

---

## 🔒 24.9 Интерпретация

---

Storage — это:

> алхимия, растянутая во времени

---

---

👉 предмет:

* “живёт”
* изменяется
* может испортиться
* может стабилизироваться

---

---

## 🔒 24.10 Связь с Morok

---

```text
длительное хранение → накопление distortion
```

---

👉 через:

* repeated Δ
* corruption_memory (косвенно)

---

---

## 🔒 24.11 Инварианты

---

```text
используется тот же pipeline
```

```text
нет новых стадий
```

```text
нет отдельных правил “порчи”
```

```text
scale_delta — единственное отличие
```

---

---

## 🔒 24.12 Связь с системой

---

Теперь:

```text
Ingredient → Inventory → evolve_item → Result
```

---

---

👉 полностью согласовано с:

* алхимией
* миром
* Morok

---

---

# ⚙️ SEMI

---

```text
k_time
modifiers
```

---