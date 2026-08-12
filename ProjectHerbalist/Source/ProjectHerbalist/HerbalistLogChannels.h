// HerbalistLogChannels.h
//
// Централизованные категории логирования проекта. LogHerbalist (общая
// категория) остаётся в ProjectHerbalist.h для модульных/разрозненных
// сообщений — всё, что относится к конкретной подсистеме, должно идти
// через одну из категорий ниже, чтобы в Output Log / `log LogX Verbose`
// можно было фильтровать по подсистеме, а не выгребать всё разом.
#pragma once

#include "CoreMinimal.h"

// Pipeline, Snapshot, Delta, Command Graph, Trace/Replay, Perception —
// детерминированное симуляционное ядро (Core/Simulation).
DECLARE_LOG_CATEGORY_EXTERN(LogHerbalistSimulation, Log, All);

// Биомный граф: распространение полей, память, footprint (Core/BiomeGraph, BiomeTypes).
DECLARE_LOG_CATEGORY_EXTERN(LogHerbalistBiome, Log, All);

// Сбор ресурсов: HarvestService, ресурсные акторы, харвест-команды GridWorldManager.
DECLARE_LOG_CATEGORY_EXTERN(LogHerbalistHarvest, Log, All);

// Алхимия/варка: AlchemySubsystem, контейнеры/столы, Apply-команды GridWorldManager.
DECLARE_LOG_CATEGORY_EXTERN(LogHerbalistAlchemy, Log, All);

// Инвентарь и его компоненты.
DECLARE_LOG_CATEGORY_EXTERN(LogHerbalistInventory, Log, All);

// Grid/World: инициализация, тик, отладочная отрисовка (Core/World).
DECLARE_LOG_CATEGORY_EXTERN(LogHerbalistWorld, Log, All);

// UI-виджеты.
DECLARE_LOG_CATEGORY_EXTERN(LogHerbalistUI, Log, All);

// PlayerController: ввод, взаимодействие, тестовые Exec-команды.
DECLARE_LOG_CATEGORY_EXTERN(LogHerbalistPlayer, Log, All);

// Загрузка/резолв справочных данных: реестры ингредиентов, типов воды, DataTable.
DECLARE_LOG_CATEGORY_EXTERN(LogHerbalistData, Log, All);
