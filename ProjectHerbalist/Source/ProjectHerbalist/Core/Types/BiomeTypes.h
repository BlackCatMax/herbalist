// BiomeTypes.h
#pragma once

#include "CoreMinimal.h"
#include "Core/Types/HerbalistCoreTypes.h"

struct FBiomeRow;

class PROJECTHERBALIST_API FBiomeDefaults
{
public:
    static void SetBiomeTable(UDataTable* InTable);
    static const FBiomeRow* GetBiomeRow(EBiomeType Biome);

    static FRealState GetDefaultState(EBiomeType Biome);
    static FEnvironment GetDefaultEnvironment(EBiomeType Biome);
    static FRealState GetDefaultWaterState(EBiomeType Biome);

    static FName BiomeTypeToName(EBiomeType Biome);
    // NameToBiomeType удалена 2026-09-02 (чистка мёртвого кода) — обратное
    // преобразование не звалось ниоткуда, вместе с ней ушёл и обслуживавший
    // только её NameToBiomeMap в .cpp. Прямое BiomeTypeToName выше живёт.
    static TArray<EBiomeType> GetAllBiomeTypes();
};
