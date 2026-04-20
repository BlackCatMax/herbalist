# 🔒 10. Morok (искажение Δ) (CORE) — UPDATED

---

## 🔒 10.1 Назначение

Morok описывает **нелинейное искажение приращения Δ**:

```text id="m1k9v3"
Δ → Δ'
```

---

Применяется:

```text id="p4k2m8"
только к Δ
```

и зависит:

```text id="x7k3m5"
только от S_real (snapshot)
```

---

## 🔒 10.2 Инварианты

---

1. Morok не изменяет `S_real` напрямую
2. Morok применяется **после стадий 3–6 pipeline**
3. Morok является **единственным источником сильной нелинейности**
4. Morok согласован с `distortion` в Meta

---

---

## 🔒 10.3 Интенсивность искажения

---

```text id="k8m2v7"
λ_raw =
corruption
· (1 - purity)
· (1 + pressure)
· (1 + corruption_memory)
· (1 - clarity)
```

---

```text id="z5k3m9"
λ = clamp(λ_raw, 0, 1)^K_MOROK_POWER
```

---

---

## 🔒 10.4 Канонический smoothstep (CORE)

---

```text id="s2k9m4"
function smoothstep(edge0, edge1, x):
    t = clamp((x - edge0) / (edge1 - edge0), 0, 1)
    return t * t * (3 - 2 * t)
```

---

---

## 🔒 10.5 Применение smoothstep

---

```text id="u7k3m2"
s = smoothstep(0.2, 0.8, λ)
w = s * s * (3 - 2 * s)
```

---

---

## 🔒 10.6 Каноническая функция искажения

---

```text id="r3k8m6"
f(x) = x
     + λ·sign(x)(1 - |x|)
     + w·λ²(x - sign(x)·K_MOROK_SHIFT)
```

---

---

## 🔒 10.7 Применение к Δ (UPDATED CORE)

---

Morok применяется к:

```text id="q9k2m4"
Δd (через компоненты направления)
Δm
ΔMeta
```

---

---

### 🔹 Для направления (Δd)

---

Искажение применяется **по компонентам**, затем нормализация:

```text id="y4k7m2"
Δd_i = f(Δd_i)
Δd = Normalize(Δd)
```

---

👉 критично:

* искажение происходит в пространстве компонент
* геометрия восстанавливается через Normalize

---

---

### 🔹 Для magnitude (Δm)

---

```text id="t6k3m8"
Δm = f(Δm)
```

---

---

### 🔹 Для Meta (ΔMeta)

---

```text id="v2k9m1"
ΔMeta_j = f(ΔMeta_j)
```

---

---

## 🔒 10.8 Связь с distortion

---

```text id="n5k8m3"
distortion = λ
```

---

---

👉 это значение:

* записывается в Meta
* используется в следующем шаге (`last_distortion`)

---

---

## 🔒 10.9 Связь с восприятием (ВАЖНО)

---

Morok влияет на систему на двух уровнях:

---

### 1. Через Δ (этот блок)

```text id="p7k2m6"
Δ → Δ'
```

---

### 2. Через восприятие (вне этого блока)

```text id="x3k9m4"
S_perceived = Distort(S_real, Morok)
```

---

---

👉 это означает:

* игрок уже действует в искажённом мире
* Morok дополнительно искажает результат

---

---

## 🔒 10.10 Инварианты после Morok

---

```text id="m8k2v5"
Δd нормализуется
λ ∈ [0,1]
```

---

---

# ⚙️ SEMI

---

```text id="z3k7m1"
K_MOROK_POWER
K_MOROK_SHIFT
smoothstep thresholds (0.2, 0.8)
коэффициенты λ_raw
```

---