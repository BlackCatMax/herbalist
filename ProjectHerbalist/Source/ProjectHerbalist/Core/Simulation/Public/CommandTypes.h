// Core/Simulation/Public/CommandTypes.h
#pragma once

#include "CoreMinimal.h"
#include "Core/Types/HerbalistCoreTypes.h"      // FRealState, FIntent, FRngState
#include "Core/Types/BiomeTypes.h"              // EBiomeType

enum class ECommandPrimitive : uint8
{
    None,
    Query,
    Transfer,
    Apply,
    Harvest,
    Wait
};

// Q – запрос (какие клетки/предметы затронуты)
struct FQueryCommand
{
    ECommandPrimitive Type = ECommandPrimitive::Query;
    TArray<FIntPoint> TargetCells;
    TArray<int32> TargetInventorySlots;
};

// T – перемещение (Transfer)
struct FTransferCommand
{
    ECommandPrimitive Type = ECommandPrimitive::Transfer;
    int32 SourceContainerID = 0;
    int32 TargetContainerID = 0;
    FName IngredientID;
    int32 Amount = 1;
};

// D – применить зелье/предмет к клетке (Apply), либо создать зелье (крафт)
struct FApplyCommand
{
    ECommandPrimitive Type = ECommandPrimitive::Apply;
    FIntPoint TargetCell;
    TArray<FInventoryItem> Ingredients;
    FIntent Intent;
    bool bIsCrafting = false;   // true = крафт в инвентарь, false = применение на клетку

    // Итог правильно исполненного ритуала (AGridWorldManager::TryAdvanceRitual,
    // Core/Alchemy/RitualTypes.h, 2026-08-30) -- градации опасности по числу
    // ингредиентов (ComputeApplyResult) для него не действуют: игрок сварил
    // по верному порядку/месту/времени, не закинул всё разом, котёл не
    // наказывает за укрощённую, а не проигнорированную сложность.
    bool bIsRitual = false;
};

// S – сбор ресурса (Harvest)
struct FHarvestCommand
{
    ECommandPrimitive Type = ECommandPrimitive::Harvest;
    FIntPoint TargetCell;
    FName IngredientID;
    int32 Amount = 1;

    // Базовые параметры ингредиента (IngredientTableRow::BaseState), резолвятся
    // вне Pipeline (у AHerbalistResourceActor уже есть на момент сбора — см.
    // AGridWorldManager::OnResourceCollected), чтобы Pipeline не обращался к реестрам.
    FRealState BaseState;

    // IngredientTableRow::Resilience — насколько трава сопротивляется характеру
    // места при сборе (0 = целиком принимает биом, 1 = остаётся собой).
    float Resilience = 0.f;

    // AGridWorldManager::GetMoonPhase(), резолвится вне Pipeline тем же
    // принципом, что BaseState/Resilience выше (15_Cycles_And_Shrines.md
    // §15.3: Растущая усиливает Body/Nature/Magnitude, Полнолуние —
    // Spirit/Potency/Resonance; см. GenerateHarvestResult в PipelineV2.cpp).
    EMoonPhase MoonPhase = EMoonPhase::NewMoon;
};

// Контейнер команды (одна запись в пакете команд). B/Talk убран 2026-08-30
// ("закрываем архитектурный долг") — не заглушка на будущее, а мёртвый код:
// ни один игровой путь никогда не создавал FTalkCommand, а сам ГДД
// (17_Hero_And_Community.md, "Никто не говорит ему «спасибо»") прямо
// отвергает диалог как механику в пользу тихой материальной платы. Тот же
// повод, что и у ExecutionOrder ниже — объявлено, не используется.
struct FCommandEntry
{
    ECommandPrimitive Primitive = ECommandPrimitive::None;

    FQueryCommand       Query;
    FTransferCommand    Transfer;
    FApplyCommand       Apply;
    FHarvestCommand     Harvest;

    uint32 CommandID = 0;
    bool bCancelled = false;
};

// Пакет команд одного тика — переименован из FCommandGraph 2026-08-30
// ("закрываем архитектурный долг", DESIGN_World_State.md §23): имя обещало
// граф зависимостей между командами, а внутри всегда был плоский
// TArray<FCommandEntry>, исполняемый по порядку добавления
// (PipelineV2.cpp::ExecutePipeline, обычный range-for по Commands). Заодно
// убран мёртвый ExecutionOrder (объявлен, нигде не читался и не
// сортировался по нему — реальный порядок исполнения всегда был порядком
// добавления, не этим полем). Честное имя вместо попытки построить
// настоящий граф зависимостей, которого никакая текущая команда не просит:
// все шесть примитивов (Query/Transfer/Apply/Harvest/Wait) независимы друг
// от друга внутри одного тика.
struct FCommandBatch
{
    TArray<FCommandEntry> Commands;

    int32 TickID = 0;
    double Timestamp = 0.0;
    bool bIsValid = true;

    void Clear()
    {
        Commands.Empty();
        bIsValid = true;
    }

    void AddCommand(const FCommandEntry& Cmd)
    {
        Commands.Add(Cmd);
    }

    int32 Num() const { return Commands.Num(); }
};