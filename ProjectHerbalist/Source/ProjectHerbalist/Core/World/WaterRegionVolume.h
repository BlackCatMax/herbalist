// Core/World/WaterRegionVolume.h
//
// Явно нарисованная форма воды (2026-09-02, прямой запрос пользователя):
// "делаем её как отдельный биом... я могу указывать, где будет вода, и она
// всегда превалирует над любым биомом... её вес всегда 1... биом воды,
// размещённый поверх других биомов, должен автоматически становиться тем
// типом воды, который для биома характерен". Тот же спавн-механизм
// (сплайн + point-in-polygon), что и у ABiomeRegionVolume -- наследуется
// напрямую, чтобы не дублировать UpdateCachedPoints/IsPointInside.
//
// Biome/MinResourcesPerCell/MaxResourcesPerCell/ResourceRegrowthTimeSeconds/
// WaterDensity, унаследованные от родителя, здесь НЕ используются (вода не
// спавнит ресурсы вовсе -- SpawnResourcesInCell в InitializeCells вызывается
// только для !Cell.bIsWater) -- остаются видимыми в Details как безвредный,
// не идеальный побочный эффект переиспользования готовой геометрии, не
// стоящий дублирования спланового кода ради чистоты панели.
//
// Клетка внутри формы этого региона получает bIsWater=true БЕЗУСЛОВНО (вес
// всегда 1, не участвует в вероятностной WaterDensity-раскладке обычных
// ABiomeRegionVolume) -- WaterTypeID при этом резолвится от УЖЕ
// определённого Cell.Biome (тот приходит от обычных, "земляных"
// ABiomeRegionVolume, покрывающих ту же клетку, независимо от воды) --
// тем же UWaterTypeRegistrySubsystem::GetRandomWaterType, что уже
// использует случайная (density-based) вода. AWaterRegionVolume НЕ
// участвует в проходе InitializeCells по земляным регионам (явно исключается
// по типу в GridWorldManagerCore.cpp) -- иначе унаследованный дефолтный
// Biome=MixedForest ошибочно застолбил бы себе долю в Cell.BiomeWeights,
// подменяя собой земляной биом, а не просто заливая его водой поверх.
#pragma once

#include "CoreMinimal.h"
#include "Core/World/BiomeRegionVolume.h"
#include "WaterRegionVolume.generated.h"

UCLASS(Blueprintable, BlueprintType)
class PROJECTHERBALIST_API AWaterRegionVolume : public ABiomeRegionVolume
{
    GENERATED_BODY()
};
