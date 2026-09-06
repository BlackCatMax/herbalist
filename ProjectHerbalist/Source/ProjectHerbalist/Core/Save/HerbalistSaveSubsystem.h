// HerbalistSaveSubsystem.h
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Core/BiomeGraph/BiomeGraphTypes.h"
#include "HerbalistSaveSubsystem.generated.h"

// Координатор сохранений v1 (CHANGELOG.md 2026-08-24): собирает состояние из
// AGridWorldManager + компонентов игрока в один UHerbalistSaveGame и обратно.
// GameInstanceSubsystem, как реестры ингредиентов/воды — тот же паттерн:
// не привязан к конкретному актору, переживает смену уровня.
UCLASS()
class PROJECTHERBALIST_API UHerbalistSaveSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:

    // Миграция сейва v1 -> v2 (2026-09-07). Вынесена в статический метод
    // намеренно: сама подсистема -- UGameInstanceSubsystem, а в
    // editor-мире автотестов GameInstance нет вовсе (то же известное
    // ограничение, что у ActivateWard/TradeWithCommunity), поэтому
    // круговой Save/Load-тест здесь невозможен. Чистая функция над картой
    // узлов проверяется напрямую -- см.
    // Herbalist.Save.BiomeGraphV1NodesMigrateToDeviations.
    //
    // В v1 MorokField хранил АБСОЛЮТНЫЙ уровень Морока биома, в v2 --
    // знаковое ОТКЛОНЕНИЕ от природного Distortion этого биома.
    // ZaryanaField в v1 был другой величиной целиком (1 - Distortion),
    // осмысленного соответствия нет -- обнуляется.
    static void MigrateBiomeGraphNodesV1ToV2(TMap<FName, FBiomeGraphNode>& Nodes);
    static const FString DefaultSlotName;

    UFUNCTION(BlueprintCallable, Category = "Herbalist|Save")
    bool SaveGame(const FString& SlotName = TEXT(""));

    UFUNCTION(BlueprintCallable, Category = "Herbalist|Save")
    bool LoadGame(const FString& SlotName = TEXT(""));
};
