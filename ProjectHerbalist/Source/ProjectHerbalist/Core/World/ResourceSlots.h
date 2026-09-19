// Core/World/ResourceSlots.h
//
// Слоты ресурсов (2026-09-19, этап 5б docs/research/DESIGN_Living_Vegetation_Research.md
// §4.1, решения пользователя): PCG решает, где может стоять растение, C++ --
// что там растёт и что с ним происходит. Граф слотов запекается в редакторе
// (узел «Write Herbalist Resource Slots», прогон -- PCG World Partition
// builder), точки лежат в ассете карты UHerbalistResourceSlots. В игре PCG для
// ресурсов не нужен: менеджер раскладывает слоты по клеткам и ставит ресурс в
// слот клетки; клетка без слотов -- прежний случайный разброс.
//
// Вид места решает пул видов: Water (плоскость воды внутри сплайна биома воды)
// и Shore (полоса кромки у берега) -- водные растения (bGrowsOnWater), Land --
// обычные.
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "ResourceSlots.generated.h"

UENUM(BlueprintType)
enum class EResourceSlotKind : uint8
{
    Land,    // суша
    Shore,   // полоса кромки у берега -- водные растения
    Water    // плоскость воды -- водные растения
};

USTRUCT(BlueprintType)
struct PROJECTHERBALIST_API FHerbalistResourceSlot
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Slots")
    FVector Location = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Slots")
    EResourceSlotKind Kind = EResourceSlotKind::Land;
};

// Слоты одного источника (актора с графом слотов): перезапекание источника
// заменяет только его набор.
USTRUCT()
struct PROJECTHERBALIST_API FHerbalistResourceSlotSet
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, Category = "Slots")
    FString SourceKey;

    UPROPERTY(VisibleAnywhere, Category = "Slots")
    TArray<FHerbalistResourceSlot> Slots;
};

UCLASS(BlueprintType)
class PROJECTHERBALIST_API UHerbalistResourceSlots : public UDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(VisibleAnywhere, Category = "Slots")
    TArray<FHerbalistResourceSlotSet> Sets;

    // Заменяет набор источника (пустой список -- убирает набор).
    void ReplaceSet(const FString& SourceKey, TArray<FHerbalistResourceSlot>&& Slots);

    int32 CountSlots() const;
};

namespace HerbalistResourceSlots
{
    // Ассет слотов карты: /Game/Data/ResourceSlots/RS_<имя карты>, имя -- без
    // префикса PIE.
    PROJECTHERBALIST_API FString AssetPackagePathForMap(const FString& MapPackageName);

    // Годится ли место виду: водному -- вода и кромка, земному -- суша и
    // кромка (кромка лежит на ландшафте).
    PROJECTHERBALIST_API bool SlotSuitsSpecies(EResourceSlotKind Kind, bool bAquaticSpecies);

    // «Land» / «Shore» / «Water» (регистр не важен); прочее -- Land.
    PROJECTHERBALIST_API EResourceSlotKind ParseKind(const FString& Text);
}
