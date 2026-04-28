// Core/Simulation/Public/CommandTypes.h
#pragma once

#include "CoreMinimal.h"
#include "Core/Types/HerbalistCoreTypes.h"      // FRealState, FIntent, FRngState
#include "Core/Types/BiomeTypes.h"              // EBiomeType

/**
 * Примитивы Command Algebra.
 * Каждый примитив представляет одно атомарное намерение.
 * Команды не выполняют действия сами – они только описывают, что должно случиться.
 * Исполнение возлагается на интерпретатор (будет реализован позже).
 */

// ---------------------------------------------------------
// Базовые перечисления для примитивов
// ---------------------------------------------------------

// Тип примитива (для отладки / сериализации)
enum class ECommandPrimitive : uint8
{
    None,
    Query,          // запрос информации (не меняет мир)
    Transfer,       // перемещение предметов
    Apply,          // применение зелья / предмета к клетке
    Harvest,        // сбор ресурса
    Talk,           // диалог (заглушка)
    Wait            // пауза
};

// ---------------------------------------------------------
// Аргументы примитивов
// ---------------------------------------------------------

// Q – запрос (какие клетки/предметы затронуты)
struct FQueryCommand
{
    ECommandPrimitive Type = ECommandPrimitive::Query;
    TArray<FIntPoint> TargetCells;          // клетки, к которым применяется запрос
    TArray<int32> TargetInventorySlots;     // слоты инвентаря
};

// T – перемещение (Transfer)
struct FTransferCommand
{
    ECommandPrimitive Type = ECommandPrimitive::Transfer;
    int32 SourceContainerID = 0;
    int32 TargetContainerID = 0;
    FName IngredientID;                    // какой предмет перемещаем
    int32 Amount = 1;
};

// D – применить зелье/предмет к клетке (Apply)
struct FApplyCommand
{
    ECommandPrimitive Type = ECommandPrimitive::Apply;
    FIntPoint TargetCell;
    FName IngredientID;
    int32 Amount = 1;
    FIntent Intent;                       // намерение (из алхимической системы)
};

// S – сбор ресурса (Harvest)
struct FHarvestCommand
{
    ECommandPrimitive Type = ECommandPrimitive::Harvest;
    FIntPoint TargetCell;
    FName IngredientID;                   // что собираем (если конкретный ресурс)
    int32 Amount = 1;
};

// B – базовое действие / диалог (Talk) – пока заглушка
struct FTalkCommand
{
    ECommandPrimitive Type = ECommandPrimitive::Talk;
    FName DialogueID;
};

// ---------------------------------------------------------
// Контейнер команды (одна запись в графе команд)
// ---------------------------------------------------------
struct FCommandEntry
{
    ECommandPrimitive Primitive = ECommandPrimitive::None;

    // Один из вариантов (только один активен)
    FQueryCommand       Query;
    FTransferCommand    Transfer;
    FApplyCommand       Apply;
    FHarvestCommand     Harvest;
    FTalkCommand        Talk;

    // Мета-информация
    uint32 CommandID = 0;               // уникальный ID (может использоваться для отмены)
    float ExecutionOrder = 0.0f;        // порядок выполнения (меньше – раньше)
    bool bCancelled = false;            // флаг отмены
};

// ---------------------------------------------------------
// Граф команд (контейнер на один тик)
// ---------------------------------------------------------
struct FCommandGraph
{
    // Упорядоченный список команд
    TArray<FCommandEntry> Commands;

    // Метаинформация
    int32 TickID = 0;                    // номер тика (для отладки)
    double Timestamp = 0.0;             // игровое время создания графа
    bool bIsValid = true;               // флаг корректности (можно использовать для валидации)

    // Вспомогательные методы
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