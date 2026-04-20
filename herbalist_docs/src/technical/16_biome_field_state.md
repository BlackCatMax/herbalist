# 🔒 16. Пространственная структура мира (дискретизация поля) — UPDATED

---

## 🔒 16.1 Назначение

Мир представляется как дискретизированное поле:

```text id="k2m9v4"
World = {Coord → BiomeState}
```

---

где:

```text id="p7k3m2"
Coord = Tuple[int, int]
```

---

Каждая ячейка содержит локальное состояние среды, влияющее на:

* Context
* Morok
* Δ динамику

---

---

## 🔒 16.2 Структура BiomeState

---

```python
Coord = Tuple[int, int]

@dataclass
class BiomeState:
    toxicity: float          # [0,1]
    fertility: float         # [0,1]
    moisture: float          # [0,1]
    competition: float       # [0,1]
    disturbance: float       # [0,1]
    time_of_day: float       # [0,1]
    season: float            # [0,1]

    accumulated_distortion: float = 0.0
    stability_memory: float = 0.5
    history_purity: float = 0.5
    history_resonance: float = 0.5

    @property
    def resistance(self) -> float: ...
    
    def to_context(self) -> Context: ...
```

---

---

## 🔒 16.3 Связь с глобальным состоянием S

---

Каждая ячейка implicitly хранит:

```text id="v4k8m1"
S_real(x, y)
```

---

👉 BiomeState:

* не заменяет `S`
* а **модулирует его эволюцию**

---

---

## 🔒 16.4 Исторические параметры (CORE)

---

### accumulated_distortion

```text id="t2m7k5"
накопленное влияние Morok
```

---

Используется в:

```text id="n8k3m6"
corruption_memory (Morok)
```

---

---

### stability_memory

```text id="z5k2m9"
инерция устойчивости среды
```

---

Влияет на:

* collapse
* сопротивление изменениям

---

---

### history_purity / history_resonance

```text id="y7k2m4"
долгосрочная "чистота" и согласованность
```

---

Используются в:

* Meta coupling
* Zaryana
* события

---

---

## 🔒 16.5 Resistance (CORE → SEMI)

---

```text id="m4k8v2"
resistance = f(
    stability_memory,
    accumulated_distortion,
    disturbance,
    competition
)
```

---

### Интерпретация:

* высокое resistance → изменения подавляются
* низкое resistance → среда легко трансформируется

---

---

⚙️ **SEMI:** точная формула

---

---

## 🔒 16.6 Преобразование в Context

---

```text id="u4k9m2"
Context = BiomeState.to_context()
```

---

### Формально:

```text id="x8k3m5"
Context = {
    toxicity,
    fertility,
    moisture,
    competition,
    disturbance,
    time_of_day,
    season,
    pressure,
    clarity
}
```

---

---

### Производные параметры

---

#### pressure

```text id="k9m2v7"
pressure = resistance
```

---

---

#### clarity

```text id="p2k8m6"
clarity = 1 - accumulated_distortion
```

---

---

#### corruption_memory

```text id="v3k7m1"
corruption_memory = accumulated_distortion
```

---

---

## 🔒 16.7 Связь с pipeline

---

```text id="t8k2m4"
BiomeState → Context → pipeline (stage 3)
```

---

---

👉 используется в:

* Context
* Morok
* Collapse
* Zaryana

---

---

## 🔒 16.8 Динамика обновления

---

После применения Δ:

```text id="q4k9m2"
BiomeState(t) → BiomeState(t+1)
```

---

---

### Обновления:

---

#### накопление искажения

```text id="z1k9m3"
accumulated_distortion += distortion * α
```

---

---

#### обновление устойчивости

```text id="k6m2v8"
stability_memory = lerp(stability_memory, S.stability, β)
```

---

---

#### обновление истории

```text id="m3k8v1"
history_purity = lerp(history_purity, S.purity, γ)
history_resonance = lerp(history_resonance, S.resonance, γ)
```

---

---

⚙️ **SEMI:** коэффициенты α, β, γ

---

---

## 🔒 16.9 Инварианты

---

```text id="q6k2m8"
все параметры BiomeState ∈ [0,1]
```

---

---

## 🔒 16.10 Интерпретация

---

BiomeState — это:

> не просто параметры среды
> а **память мира + сопротивление + контекст**

---