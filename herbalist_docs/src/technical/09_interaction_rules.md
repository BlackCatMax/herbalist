# 🔒 9. Interaction Rules (фиксированный порядок) (CORE) — UPDATED

---

## 🔒 9.1 Общий принцип

Interaction применяется симметрично к:

```text id="i1k7m4"
S и Δ
```

---

Все операции выполняются:

```text id="u3k9m2"
над направлением d и Meta
```

с учётом нормализации.

---

### ❗ Критический инвариант

```text id="p8k2m6"
для alignment используются исходные значения d₀
(до начала Interaction, без self-feedback)
```

---

---

## 🔒 9.2 Подготовка

```text id="m2k8v5"
d₀ = d (snapshot направления)
m₀ = Meta (snapshot meta)
```

---

---

## 🔒 9.3 Последовательность стадий

---

### 1. Resonance gain

```text id="r7k3m9"
d = d * (1 + K_RES_GAIN · resonance)
```

---

### 2. Synergy

```text id="t5k9m1"
d = d * (1 + K_SYNERGY_GAIN · resonance)
```

---

👉 усиливает уже согласованные направления

---

### 3. Corruption decomposition

```text id="y4k2m8"
d = d * (1 - corruption · sign(d))
```

---

👉 подавляет компоненты в зависимости от их знака

---

### 4. Alignment (используются d₀)

```text id="n6k3m7"
align = avg(d₀_i)
d = d * (1 + resonance · align)
```

---

👉 важно:

* используется **исходное состояние**
* нет обратной связи внутри шага

---

---

## 🔒 9.4 Нормализация (обязательная)

После всех операций над направлением:

```text id="z8k2m4"
d = Normalize(d)
```

---

👉 это гарантирует:

```text id="q2k7m5"
|d| = 1
```

---

---

## 🔒 9.5 Meta coupling (🔒 snapshot semantics)

---

Все обновления вычисляются от:

```text id="w3k9m6"
m₀ = Meta до начала Interaction
```

---

### Формулы

```text id="c7k2m9"
potency'    = m₀.potency   - (1 - m₀.stability) * 0.3
stability'  = m₀.stability - m₀.potency * 0.4
purity'     = m₀.purity    - m₀.corruption
corruption' = m₀.corruption - K_PURITY_TO_CORRUPTION * m₀.purity
resonance'  = m₀.resonance - K_AXIS_DIVERGENCE * |d₀_body - d₀_nature|
```

---

### Применение

```text id="v6k3m1"
Meta ← (potency', purity', stability', resonance', corruption')
```

---

👉 критично:

* никакой зависимости от промежуточных значений
* всё считается из одного snapshot

---

---

## 🔒 9.6 Применение к Δ

Все стадии применяются **симметрично к Δ**:

```text id="x9k2m5"
Δd проходит те же стадии → Normalize
ΔMeta обновляется через snapshot ΔMeta₀
```

---

---

## 🔒 9.7 Связь с magnitude (m)

Interaction напрямую **не изменяет m**:

```text id="b5k8m2"
m — не модифицируется в этом блоке
```

---

👉 но влияет косвенно через:

* Meta (potency, stability)
* дальнейшие стадии pipeline

---

---

## 🔒 9.8 Инварианты

После Interaction:

```text id="k7m2v9"
|d| = 1
Meta обновлены через snapshot
```

---

---

# ⚙️ SEMI

---

```text id="n4k8m3"
K_RES_GAIN
K_SYNERGY_GAIN
K_PURITY_TO_CORRUPTION
K_AXIS_DIVERGENCE
0.3, 0.4
```

---