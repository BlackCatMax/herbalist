🧱 ФИКСАЦИЯ CORE: КОНТРАКТ
🔒 0. Общий принцип

Core — это:

детерминированная
дискретная
замкнутая система

👉 Если что-то это нарушает — это баг, а не “фича”

🔴 1. ДЕТЕРМИНИЗМ (ОБЯЗАТЕЛЬНО)
Должно быть:
 Используется только FRngState
 Нет rand(), std::random, FMath::Rand
 RNG всегда передаётся явно
Важные правила:
✔ Локальный RNG
FRngState localRng = BranchRng(SimulationRng, uniqueId);
 Привязка к:
BiomeIndex
LogicalTick
✔ Восприятие
FRngState perceptionRng = BranchRng(SimulationRng, perceptionId);
 Не влияет на основной RNG
 Не мутирует SimulationRng
❌ Запрещено:
 Использовать RNG без seed
 Использовать глобальный RNG напрямую
 Зависеть от порядка вызовов
🔴 2. ВРЕМЯ (DISCRETE ONLY)
Должно быть:
 uint32 CurrentTick
 FAction::LogicalTick
 Simulation::Step() увеличивает тик
❌ Запрещено:
 float time в Core логике
 deltaTime-зависимое поведение
 Tick из UE влияет на Core
🔴 3. СОСТОЯНИЕ (STATE INTEGRITY)
Должно быть:
✔ Нулевая дельта
FRealState::Zero()
 Используется в PendingDeltas
✔ Direction
 Никогда не clamp’ится
 Всегда нормализуется после изменений
Math::Normalize(Direction);
✔ Magnitude
 Использует saturation
if (delta >= 0)
    M += delta * (1 - M);
else
    M += delta * M;
✔ Meta
 Всегда через Clamp01
 После каждого изменения
❌ Запрещено:
 Clamp для Direction
 Линейный рост Magnitude
 Пропуск Normalize
🔴 4. PIPELINE (НЕИЗМЕНЯЕМЫЙ ПОРЯДОК)
Обязательный порядок:
Aggregate →
Contextualize →
ApplyMorok →
ApplyZaryana →
ApplyDelta
Должно быть:
 Morok ДО Zaryana
 DistortPerception отдельно
❌ Запрещено:
 Менять порядок
 Вставлять новые стадии без ревизии
 Применять Zaryana после ApplyDelta
🔴 5. MOROK (КРИТИЧЕСКИЕ ИНВАРИАНТЫ)
Должно быть:
 Добавляет шум через RNG
 Делает корреляцию осей
 НЕ использует Clamp для Direction
 Всегда вызывает Normalize
❌ Запрещено:
 Убирать Normalize
 Делать Morok пост-эффектом
 Прямо менять Core (только Delta)
🔴 6. ZARYANA
Должно быть:
 Работает как контр-процесс
 Усиливает доминирующую ось
 Использует epsilon при сравнении float
>= maxVal - eps
❌ Запрещено:
 Делать её “очисткой после факта”
 Убирать влияние на Direction
🔴 7. PROPAGATION (СОХРАНЕНИЕ ЭНЕРГИИ)
Должно быть:
buffer = Pending * factor
Pending *= (1 - factor)
 Передача соседям из buffer
 Нет удвоения энергии
❌ Запрещено:
 Копировать дельту без уменьшения
 Усиливать энергию при распространении
🔴 8. ПАМЯТЬ БИОМА
Должно быть:
 AccumulatedDistortion
 StabilityMemory
 Обновление зависит от Intent
❌ Запрещено:
 Прямое обнуление памяти
 Игнорирование Intent
🔴 9. ВОСПРИЯТИЕ (PERCEPTION LAYER)
Должно быть:
 Отдельный RNG
 НЕ влияет на Core
 Использует Morok + Zaryana
❌ Запрещено:
 Мутировать Simulation
 Использовать основной RNG
🔴 10. UE ИНТЕГРАЦИЯ (ЖЁСТКАЯ ИЗОЛЯЦИЯ)
Должно быть:
 UHerbalistWorldSubsystem
 Нет Tick
 Только AdvanceSimulation()
✔ Доступ:
const Simulation& GetSimulation()
❌ Запрещено:
 Изменять Core из Blueprint
 Вызывать Step автоматически
 Хранить состояние в UE
🔴 11. ТЕСТОВЫЙ КОНТРАКТ
Перед любым изменением:
 Test 1 — Манчкин
 Test 2 — Разрушение/восстановление
 Test 3 — Перфекционист
 Test 4 — Градиенты
❌ Если любой тест падает:

👉 РАЗРАБОТКА ОСТАНАВЛИВАЕТСЯ

🧾 ФИНАЛЬНЫЙ СТАТУС

Если ВСЕ пункты выше соблюдены:

👉 Core считается:

стабильным
воспроизводимым
расширяемым
🚫 ЧТО ДАЛЬШЕ ДЕЛАТЬ НЕЛЬЗЯ

Пока Core зафиксирован:

❌ менять формулы
❌ добавлять новые параметры
❌ менять pipeline
❌ “чуть подкрутить баланс”
✅ ЧТО МОЖНО
✔ интеграция UE
✔ визуализация
✔ контент (ресурсы, рецепты)
✔ UI/UX
💀 Самое важное правило

Если тебе захочется “чуть поправить Core” во время контента
— значит ты не зафиксировал его достаточно жёстко.