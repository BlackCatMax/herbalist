#pragma once
#include "CoreMinimal.h"
#include "Core/Types/HerbalistCoreTypes.h"

class UIngredientRegistrySubsystem;

PROJECTHERBALIST_API FText GeneratePotionName(const FRealState& State);

// Единая точка отображаемого имени предмета инвентаря — раньше каждый виджет
// (AlchemySlotWidget/InventorySlotWidget) разрешал Ash/BoiledWater/Water/Potion
// по-своему; один знал ветку "Ash" -> "Зола", другой нет, и то же самое
// IngredientID показывалось по-разному в двух местах интерфейса
// (AUDIT_AND_REFACTORING_PLAN §2.4). Registry может быть nullptr — тогда для
// незнакомых ID возвращается сырой IngredientID.ToString(), как и раньше.
PROJECTHERBALIST_API FString GetItemDisplayName(const FInventoryItem& Item, UIngredientRegistrySubsystem* Registry);
