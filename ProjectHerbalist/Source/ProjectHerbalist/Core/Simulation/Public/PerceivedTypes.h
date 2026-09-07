// Core/Simulation/Public/PerceivedTypes.h
#pragma once

#include "CoreMinimal.h"
#include "Core/Types/HerbalistCoreTypes.h"
#include "PerceivedTypes.generated.h"

USTRUCT(BlueprintType)
struct PROJECTHERBALIST_API FPerceivedCell
{
    GENERATED_BODY()

    // Явный инициализатор обязателен: без него UE при загрузке класса пишет
    // "StructProperty FPerceivedCell::Coord is not initialized properly" --
    // FIntPoint не имеет конструктора по умолчанию, обнуляющего поля, и
    // значение приходило бы из мусора стека. Замечено 2026-09-07 в
    // PIE-логе пользователя (единственная настоящая ошибка проекта в нём).
    UPROPERTY(BlueprintReadWrite)
    FIntPoint Coord = FIntPoint::ZeroValue;

    UPROPERTY(BlueprintReadWrite)
    FRealState PerceivedState;

    UPROPERTY(BlueprintReadWrite)
    bool bIsVisible = true;
};

USTRUCT(BlueprintType)
struct PROJECTHERBALIST_API FPerceivedWorld
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite)
    TMap<FIntPoint, FPerceivedCell> Cells;

    UPROPERTY(BlueprintReadWrite)
    int32 WorldSeed = 0;
};

// Обычная C++ структура, не видна UHT
struct FPerceivedInventory
{
    TMap<int32, TArray<FInventoryItem>> ContainerContents;
};