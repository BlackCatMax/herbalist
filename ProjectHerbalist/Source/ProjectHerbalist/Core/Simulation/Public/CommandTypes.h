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
    Talk,
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
    // AGridWorldManager::OnResourceCollected) — нужны для расчёта отклонения от
    // S0 как в UHarvestService::Harvest, без обращения к реестрам внутри Pipeline.
    FRealState BaseState;
};

// B – базовое действие / диалог (Talk) – пока заглушка
struct FTalkCommand
{
    ECommandPrimitive Type = ECommandPrimitive::Talk;
    FName DialogueID;
};

// Контейнер команды (одна запись в графе команд)
struct FCommandEntry
{
    ECommandPrimitive Primitive = ECommandPrimitive::None;

    FQueryCommand       Query;
    FTransferCommand    Transfer;
    FApplyCommand       Apply;
    FHarvestCommand     Harvest;
    FTalkCommand        Talk;

    uint32 CommandID = 0;
    float ExecutionOrder = 0.0f;
    bool bCancelled = false;
};

// Граф команд (контейнер на один тик)
struct FCommandGraph
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