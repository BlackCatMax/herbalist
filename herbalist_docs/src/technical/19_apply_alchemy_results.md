# 🔒 19. Применение результата алхимии к биому — UPDATED

---

## 🔒 19.1 Назначение

Этот блок описывает, как результаты алхимии (в том числе ингрeдиенты, взаимодействующие в рамках алхимических процессов) воздействуют на биом:

* **coherence** — основной модификатор результата
* учитывается **сопротивление**
* разрешено **усиление воздействия** через внешние модификаторы (например, капища)

---

## 🔒 19.2 Принцип работы

Алхимический результат влияет на **BiomeState** через:

* **coherence** (мера "погодности" результата для среды)
* **сопротивление** биома
* **усиление** (например, через капища)

---

## 🔒 19.3 Коэффициент **coherence**

---

Результат алхимии имеет **coherence**, который измеряет его соответствие текущему состоянию биома.

```python
def compute_coherence(result: ResultVector) -> float:
    # расчет коэффициента coherence
    return coherence_value
```

---

**Применение coherence**:

```text id="m3k2v6"
coherence ∈ [0, 1]
```

* **coherence = 1** — полный эффект алхимического действия
* **coherence = 0** — полное отсутствие эффекта

---

---

## 🔒 19.4 Применение к BiomeState

---

Результат алхимии влияет на:

```text id="p7k3m5"
BiomeState → (toxicity, fertility, moisture, competition, disturbance, etc.)
```

---

Применение:

```python
def apply_result_to_biome(biome: BiomeState, result: ResultVector,
                          shrine_active: bool = False) -> None:
    effective_factor = max(0.0, 1.0 - biome.resistance)
    
    # Усиление через капище
    if shrine_active:
        effective_factor *= 1.5   # разрешённое усиление
    
    # Изменение параметров биома с учётом эффекта алхимии
    biome.toxicity += result.toxicity * effective_factor
    biome.fertility += result.fertility * effective_factor
    biome.moisture += result.moisture * effective_factor
    biome.competition += result.competition * effective_factor
    biome.disturbance += result.disturbance * effective_factor
```

---

### 1. Принцип работы

1. **resistance** биома снижает **эффективность** воздействия:

   ```text id="z5k8m2"
   effective_factor = max(0.0, 1.0 - biome.resistance)
   ```

2. При активации **капища** (или других усилителей) коэффициент увеличивается:

   ```text id="n4k8m5"
   effective_factor *= 1.5  # усиление
   ```

---

### 2. Сопротивление (CORE)

---

```text id="t9k8m1"
sопротивление biome влияет на силу эффекта алхимии.
```

---

### 3. Усиление через внешние модификаторы (например, капища)

```text id="w7k3m2"
усиление может увеличивать влияние на параметры.
```

---

## 🔒 19.5 Инварианты

---

1. **coherence** всегда находится в диапазоне [0, 1].
2. Эффект изменения параметров биома детерминирован через:

```text id="r4k5m1"
resistance, coherence, и внешние модификаторы
```

---

## 🔒 19.6 Применение в pipeline

---

Применение результатов алхимии и модификации происходит в следующем порядке:

```text id="x3k8m4"
BiomeState → ΔBiomeState → apply_result_to_biome → BiomeState(t+1)
```

---

---

## 🔒 19.7 Связь с Morok и Zaryana

---

Результат алхимии можно использовать для **изменения мира**:

* Если результат взаимодействует с **Morok**, это влияет на **distortion** в **BiomeState**.
* Применение через **Zaryana** может привести к **стабилизации** изменений.

---

## 🔒 19.8 SEMI

---

Настраиваем:

```text id="y7k8m3"
Коэффициенты влияния: 
- на параметры (toxicity, fertility, etc.)
- на силу взаимодействия (эффективность)
```

---