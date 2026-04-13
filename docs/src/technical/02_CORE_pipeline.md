# 🔒 3. Канонический pipeline (CORE) — UPDATED

---

## 🔒 3.1 Общий принцип

Pipeline описывает вычисление приращения состояния `ΔS` и обновление `S_real` в рамках одного шага симуляции.

Все вычисления выполняются:

```text id="z6m8i1"
из snapshot состояния S_real(t)
```

---

## 🔒 3.2 Глобальный цикл (интеграция CORE)

Перед входом в pipeline:

```text id="e0r4jo"
S_perceived = Distort(S_real, Morok)
```

Игрок формирует действие:

```text id="qrb8c3"
Action = f(S_perceived)
```

---

### Намерение:

```text id="9l5x9o"
Intent_player = F(Action(S_perceived))
Intent_system = F(Action(S_real))
```

---

👉 Далее pipeline работает с:

```text id="0m9c6l"
S = S_real
Δ = начальное приращение из Action / fold
```

---

## 🔒 3.3 Каноническая последовательность стадий

```
1. fold → S₀
2. Δ = delta(S₀)

3. context(S, Δ)
4. water(S, Δ)
5. axis_sign(S, Δ)
6. interaction(S, Δ)

7. Δ' = morok_effect(Δ, S, Intent_system, Meta)

8. S' = apply_delta(S, Δ')

9. conflict_resolution(S')
10. zaryana(S')

11. final_clamp
```

---

## 🔒 3.4 Семантика стадий

---

### 1–2. Формирование Δ

```text id="u4m3f8"
S₀ = fold(Action, Context)
Δ = delta(S₀)
```

---

👉 Δ формируется из:

* действия игрока
* контекста
* внутренней логики системы

---

### 3–6. Симметричные преобразования (CORE INVARIANT)

```text id="d0c9lh"
context(S, Δ)
water(S, Δ)
axis_sign(S, Δ)
interaction(S, Δ)
```

---

### Инвариант:

```text id="6y7x2r"
эти стадии применяются одинаково к S и Δ
```

---

👉 это означает:

* трансформация пространства
* а не отдельных значений

---

## 🔒 3.5 Morok (UPDATED CORE)

Старая версия:

```text id="d4qg3r"
Δ' = morok(Δ, S)
```

---

### Новая интерпретация:

```text id="c1x3y9"
Δ' = morok_effect(Δ, S, Intent_system, ΔIntent, Meta)
```

---

Morok влияет через:

1. искажение входа (уже произошло через S_perceived)
2. рассогласование Intent
3. Meta (corruption, distortion)

---

👉 важно:

Morok **не является отдельной фазой “ломания Δ”**,
а частью общей функции вычисления Δ'

---

## 🔒 3.6 Применение Δ

```text id="gq1k9z"
S' = apply_delta(S, Δ')
```

---

Разложение:

```text id="8o4k7c"
d' = Normalize(d + Δd)
m' = clamp(m + Δm - Decay(m), 0, M_max)
Meta' = Meta + ΔMeta
```

---

## 🔒 3.7 Разрешение конфликтов

```text id="8ymzqg"
conflict_resolution(S')
```

---

Используется для:

* устранения противоречий
* стабилизации состояний
* приведения к допустимому пространству

---

## 🔒 3.8 Zaryana

```text id="2m0k9s"
zaryana(S')
```

---

Функция:

* повышает согласованность
* уменьшает отклонение от S₀
* стабилизирует систему

---

## 🔒 3.9 Финализация

```text id="5j3k8f"
final_clamp(S')
```

---

Гарантирует:

```text id="q2r5t7"
|d| = 1
0 ≤ m ≤ M_max
Meta в допустимых пределах
```

---

## 🔒 3.10 Итог

```text id="p8x2k1"
S_real(t+1) = S'
```

---

# 🔒 3.11 Инварианты pipeline (UPDATED)

---

1. Все стадии 3–6 применяются **симметрично к S и Δ**
2. Все вычисления выполняются из **snapshot S_real(t)**
3. Morok:

   * влияет через восприятие
   * влияет через Intent
   * не является изолированным “ломателем Δ”
4. Δ применяется к S **один раз**
5. Все ограничения применяются **только в конце**

---
---