// HerbalistLogChannels.h
//
// Централизованные категории логирования проекта. LogHerbalist (общая
// категория) остаётся в ProjectHerbalist.h для модульных/разрозненных
// сообщений — всё, что относится к конкретной подсистеме, должно идти
// через одну из категорий ниже, чтобы в Output Log / `log LogX Verbose`
// можно было фильтровать по подсистеме, а не выгребать всё разом.
//
// Упорядочено при полном аудите проекта 2026-08-31 (по прямому запросу
// пользователя, "внедрить централизованное логирование" — трактовано как
// "упорядочить существующее", не завести отдельную систему телеметрии,
// см. уточняющие вопросы того же прохода). Разведка (~204 вызова UE_LOG на
// момент аудита) нашла: проблема НЕ "слишком много категорий" — большинство
// уже используются дисциплинированно. Единственная реальная свалка —
// LogHerbalistWorld (крупнейшая, 44 вызова) вобрала в себя Save/Zaryana,
// которым не принадлежит по собственному описанию ("Grid/World: инициализация,
// тик, отладочная отрисовка"). Добавлены LogHerbalistSave/LogHerbalistZaryana
// ниже и вызовы перенесены на них; один вызов из Core/Storage/
// AlchemyTableActor.cpp (регистрация капища) перенесён на уже существующий
// LogHerbalistAlchemy — его собственное описание ("контейнеры/столы") уже
// включает эту область, заводить третью новую категорию под один вызов
// избыточно.
//
// Уровни (не было единой конвенции до этого прохода, выбор был "на глаз
// автора" — задокументировано, не переписано во всех ~204 местах, слишком
// большой blast radius для этого прохода):
//   Log       — обычное событие потока (успешное сохранение, спавн,
//               завершённая операция) — то, что интересно при обычном
//               чтении лога, не только при отладке.
//   Verbose   — то же самое, но слишком частое для обычного чтения
//               (срабатывает на каждый тик/каждую клетку) — видно только
//               при явном `log LogX Verbose`.
//   Warning   — операция не удалась, но игра продолжается корректно
//               (честный отказ: неизвестный ингредиент, промах мимо
//               сетки, отсутствующий менеджер) — не баг сам по себе, тот
//               же смысл, что "TestFalse(...) не крашит" в тестах.
//   Error     — операция не удалась ТАМ, где ожидалась гарантия (сейв не
//               десериализовался, DataTable не того типа) — сигнал
//               реальной проблемы, не обычного игрового отказа.
//
// Под-тег в скобках прямо в тексте сообщения ([Zaryana], [Community],
// [Domovoi], [Garden], [Ritual], [Shrine], [MemoryFragment], [Talk],
// [Entities]...) — сознательный, уже сложившийся паттерн для более тонкой
// идентификации ВНУТРИ категории, не случайные остатки отладки. Продолжать
// его для новых подсистем внутри уже существующих категорий (например,
// новая механика внутри LogHerbalistWorld) вместо того, чтобы заводить
// отдельную DECLARE_LOG_CATEGORY_EXTERN на каждую новую механику — теги
// дешевле категорий и не требуют правки этого заголовка.
#pragma once

#include "CoreMinimal.h"

// Pipeline, Snapshot, Delta, Command Graph, Trace/Replay, Perception —
// детерминированное симуляционное ядро (Core/Simulation).
DECLARE_LOG_CATEGORY_EXTERN(LogHerbalistSimulation, Log, All);

// Биомный граф: распространение полей, память, footprint (Core/BiomeGraph, BiomeTypes).
DECLARE_LOG_CATEGORY_EXTERN(LogHerbalistBiome, Log, All);

// Сбор ресурсов: ресурсные акторы, харвест-команды GridWorldManager.
DECLARE_LOG_CATEGORY_EXTERN(LogHerbalistHarvest, Log, All);

// Алхимия/варка: AlchemySubsystem, контейнеры/столы, Apply-команды GridWorldManager.
DECLARE_LOG_CATEGORY_EXTERN(LogHerbalistAlchemy, Log, All);

// Инвентарь и его компоненты.
DECLARE_LOG_CATEGORY_EXTERN(LogHerbalistInventory, Log, All);

// Grid/World: инициализация, тик, отладочная отрисовка (Core/World) —
// НЕ Save/Zaryana/Community/Garden/Entities/Ritual — те либо ниже, либо
// используют [SubTag] внутри этой же категории по историческим причинам
// (см. комментарий у файла).
DECLARE_LOG_CATEGORY_EXTERN(LogHerbalistWorld, Log, All);

// UI-виджеты.
DECLARE_LOG_CATEGORY_EXTERN(LogHerbalistUI, Log, All);

// PlayerController: ввод, взаимодействие, тестовые Exec-команды.
DECLARE_LOG_CATEGORY_EXTERN(LogHerbalistPlayer, Log, All);

// Загрузка/резолв справочных данных: реестры ингредиентов, типов воды, DataTable.
DECLARE_LOG_CATEGORY_EXTERN(LogHerbalistData, Log, All);

// Сохранение/загрузка (Core/Save) — 2026-08-31, выделено из LogHerbalistWorld
// (см. комментарий у файла): сейв — не "мир", отдельный жизненный цикл со
// своими ошибками (несовпадение размера сетки, битый слот).
DECLARE_LOG_CATEGORY_EXTERN(LogHerbalistSave, Log, All);

// Заряна: фрагменты памяти, Buyan, GlobalPerceptionClarity (Core/Zaryana,
// Core/World/GridWorldManagerZaryana.cpp) — 2026-08-31, выделено из
// LogHerbalistWorld тем же доводом, что и Save выше: отдельная,
// содержательно самостоятельная нарративная подсистема, не "мир".
DECLARE_LOG_CATEGORY_EXTERN(LogHerbalistZaryana, Log, All);
