# 🔒 5. Δ (приращение) (CORE) — UPDATED

---

## 🔒 5.1 Назначение

Функция `delta` преобразует агрегированное состояние:

```text id="a1k9d3"
S₀ = (d₀, m₀, Meta₀)
```

в приращение:

```text id="b7m4q2"
ΔS = (Δd, Δm, ΔMeta)
```

---

## 🔒 5.2 Общий принцип

Δ формируется из:

* структуры `S₀`
* Meta-параметров
* накопленного искажения (`last_distortion`)
* системного намерения (`Intent_system`)

---

# 🔒 5.3 Приращение направления (Δd)

---

Базовая формула:

```text id="d4k2v8"
Δd_raw = d₀ * potency + Bootstrap_d
```

---

### Bootstrap-компонент

```text id="k8v3m1"
Bootstrap_d = DirectionalSign(d₀) * BOOTSTRAP_GAIN * last_distortion
```

---

### Определение DirectionalSign

```text id="p3x8n6"
DirectionalSign(d₀) = sign(d₀_i) для каждой компоненты
```

---

👉 сохраняет твою идею:

* усиление направления
* “подталкивание” даже при слабых значениях

---

### Нормализация

```text id="z7m2q5"
Δd = Normalize(Δd_raw)
```

---

## 📌 Интерпретация

* `potency` усиливает текущее направление
* `bootstrap` добавляет “инерцию искажения”
* `last_distortion` переносит влияние Morok

---

# 🔒 5.4 Приращение magnitude (Δm)

---

Базовая формула:

```text id="q9m4v1"
Δm = (m₀ - m_ref) * potency + Bootstrap_m
```

---

### Где:

```text id="n2k8r6"
m_ref = 0.5 * M_max
```

---

### Bootstrap:

```text id="u6v3x9"
Bootstrap_m = sign(m₀ - m_ref) * BOOTSTRAP_GAIN * last_distortion
```

---

## 📌 Интерпретация

* если `m₀ > m_ref` → рост усиливается
* если `m₀ < m_ref` → спад усиливается
* distortion раскачивает систему

---

👉 это даёт:

* динамику “перегрузки” и “истощения”
* нестабильность при высоком Morok

---

# 🔒 5.5 Приращение Meta (ΔMeta)

---

```text id="x8k2m5"
ΔMeta_j = (Meta₀_j - Meta_ref_j) * potency
          + sign(Meta₀_j - Meta_ref_j) * BOOTSTRAP_GAIN * last_distortion
```

---

Meta_ref:

```text id="r5n1q8"
обычно 0.5 или системное равновесие
```

---

## 🔒 5.6 Связь с Intent_system

---

Δ модифицируется через согласованность:

```text id="y3k7m2"
ΔS = ΔS * Alignment(Intent_system, S₀)
```

---

где:

```text id="m4v9q1"
Alignment ∈ [0,1]
```

---

👉 эффект:

* согласованные действия усиливаются
* рассогласованные — ослабляются

---

# 🔒 5.7 Связь с Morok

---

```text id="c8n5v2"
last_distortion = f(Morok, distortion)
```

---

👉 Morok влияет через:

* bootstrap
* alignment
* входные данные (S_perceived → fold)

---

# 🔒 5.8 Итог

```text id="z1m3q9"
ΔS = (Δd, Δm, ΔMeta)
```

---

# ⚙️ SEMI

---

```text id="u9k2m7"
BOOTSTRAP_GAIN
```

---

Определяет:

* насколько система “разгоняется”
* уровень нестабильности
* чувствительность к Morok

---
---