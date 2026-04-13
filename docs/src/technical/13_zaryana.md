# 🔒 13. Zaryana (структурирование) (CORE) — UPDATED

---

## 🔒 13.1 Назначение

Zaryana выполняет:

* структурирование состояния
* снижение последствий искажения
* усиление “чётких” форм

---

Работает как:

```text id="k3m9v2"
анти-Morok слой
```

---

---

## 🔒 13.2 Коэффициент Z

---

```text id="p7k2m5"
Z = (1 - distortion)
    · (1 - K_ZARYANA_CORR · |corruption - 0.5|)
```

---

---

### Интерпретация:

* высокий distortion → Z уменьшается
* крайние значения corruption → Z уменьшается
* максимальный эффект при:

  * низком Morok
  * умеренной corruption

---

---

## 🔒 13.3 Преобразование направления (UPDATED)

---

Функция:

```text id="v4k8m1"
ψ(x) = sign(x) · |x|^K_ZARYANA_POWER
```

---

Применение к направлению:

```text id="t2m7k3"
d_i = lerp(d_i, ψ(d_i), Z)
```

---

---

### Нормализация (обязательная)

```text id="n8k3m6"
d = Normalize(d)
```

---

---

## 📌 Интерпретация

* усиливает выраженные компоненты
* подавляет слабые
* делает направление “чётче”

---

---

## 🔒 13.4 Применение к magnitude (m)

---

Zaryana влияет на “силу” состояния:

```text id="z5k2m9"
m = lerp(m, m^K_ZARYANA_POWER, Z)
```

---

👉 это:

* стабилизирует сильные состояния
* подавляет слабые

---

---

## 🔒 13.5 Преобразование Meta

---

Для всех Meta, кроме distortion:

```text id="y7k2m4"
potency    = lerp(potency,    potency^K_ZARYANA_POWER,    Z)
purity     = lerp(purity,     purity^K_ZARYANA_POWER,     Z)
stability  = lerp(stability,  stability^K_ZARYANA_POWER,  Z)
resonance  = lerp(resonance,  resonance^K_ZARYANA_POWER,  Z)
corruption = lerp(corruption, corruption^K_ZARYANA_POWER, Z)
```

---

---

## 🔒 13.6 Ограничение (CORE)

---

```text id="m4k8v2"
distortion не изменяется
```

---

👉 критично:

* Morok нельзя “стереть” напрямую
* только косвенно через систему

---

---

## 🔒 13.7 Применение к Δ

---

Zaryana применяется симметрично к:

```text id="u4k9m2"
Δd, Δm, ΔMeta
```

---

```text id="x8k3m5"
Δd_i = lerp(Δd_i, ψ(Δd_i), Z)
Δd = Normalize(Δd)

Δm = lerp(Δm, Δm^K_ZARYANA_POWER, Z)
ΔMeta — аналогично
```

---

---

## 🔒 13.8 Инварианты

После применения:

```text id="k9m2v7"
|d| = 1
```

---

---

## 🔒 13.9 Связь с системой

---

Zaryana:

* идёт **после Morok и Collapse**
* не отменяет искажение
* но делает результат **структурированным**

---

---

# ⚙️ SEMI

---

```text id="p2k8m6"
K_ZARYANA_POWER
K_ZARYANA_CORR
```

---