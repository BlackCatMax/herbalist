// ResourceBalanceRow.h
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Core/Types/HerbalistCoreTypes.h"
#include "ResourceBalanceRow.generated.h"

// Биомы по ГДД (13 типов) - используется только в C++, не BlueprintType (иначе нужен uint8)
UENUM()
enum class EBiomeMask : int32
{
    None = 0,
    Tundra = 1 << 0,
    ForestTundra = 1 << 1,
    NorthernTaiga = 1 << 2,
    MiddleTaiga = 1 << 3,
    SouthernTaiga = 1 << 4,
    MixedForest = 1 << 5,
    BroadleafForest = 1 << 6,
    ForestSteppe = 1 << 7,
    Steppe = 1 << 8,
    SemiDesert = 1 << 9,
    Floodplain = 1 << 10,
    RaisedBog = 1 << 11,
    LowlandBog = 1 << 12,
};
ENUM_CLASS_FLAGS(EBiomeMask)

UENUM(BlueprintType, meta = (Bitflags, UseEnumValuesAsMaskValuesInEditor = "true"))
enum class ESeasonMask : uint8
{
    None = 0,
    Spring = 1 << 0,
    Summer = 1 << 1,
    Autumn = 1 << 2,
    Winter = 1 << 3,
};
ENUM_CLASS_FLAGS(ESeasonMask)

UENUM(BlueprintType, meta = (Bitflags, UseEnumValuesAsMaskValuesInEditor = "true"))
enum class ETimeOfDayMask : uint8
{
    None = 0,
    Morning = 1 << 0,
    Day = 1 << 1,
    Evening = 1 << 2,
    Night = 1 << 3,
};
ENUM_CLASS_FLAGS(ETimeOfDayMask)

USTRUCT(BlueprintType)
struct FWeatherModifiers
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FConditionModifier Rain;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FConditionModifier Thunderstorm;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FConditionModifier Fog;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FConditionModifier Clear;
};

USTRUCT(BlueprintType)
struct PROJECTHERBALIST_API FResourceBalanceRow : public FTableRowBase
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Balance")
    FName PrimaryAssetId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Balance")
    EResourceType ResourceType = EResourceType::None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Balance")
    int32 RarityWeight = 0;

    // Биомы (битовая маска) - просто int32, без BitmaskEnum в Blueprint
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Balance")
    int32 BiomeMask = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Balance", meta = (Bitmask, BitmaskEnum = "/Script/ProjectHerbalist.ESeasonMask"))
    int32 SeasonMask = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Balance", meta = (Bitmask, BitmaskEnum = "/Script/ProjectHerbalist.ETimeOfDayMask"))
    int32 TimeOfDayMask = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Balance")
    FWeatherModifiers WeatherModifiers;
};