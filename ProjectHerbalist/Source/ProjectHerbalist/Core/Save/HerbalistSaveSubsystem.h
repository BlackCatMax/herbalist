// HerbalistSaveSubsystem.h
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Core/BiomeGraph/BiomeGraphTypes.h"
#include "HerbalistSaveSubsystem.generated.h"

class AGridWorldManager;
class UHerbalistSaveGame;

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

    // Версия формата, которую пишет эта сборка (история -- у SaveGame).
    static constexpr int32 CurrentSaveVersion = 8;

    // Можно ли применить сейв к менеджеру (разметка мира, этап 8): версия не
    // новее сборки, разметка совместима, без разметки -- тот же размер сетки.
    // Статическая тем же доводом, что MigrateBiomeGraphNodesV1ToV2: LoadGame в
    // автотесте не вызвать. OutReason -- причина отказа для лога.
    static bool CheckSaveApplicable(const UHerbalistSaveGame& Save, const AGridWorldManager& Manager, FString& OutReason);

    // Капища, ориентиры сущностей и точки интереса, чья клетка задана, но лежит
    // за сеткой -- после уборки плиток ландшафта (ревью этапа 8а).
    static int32 CountSitesOutsideGrid(const AGridWorldManager& Manager);

    // Места сейва, которым сетка обязана дать страницу (2026-09-13): капища
    // (капища за краем сетки -- с четырьмя соседними клетками), ориентиры
    // сущностей, точки интереса. Курганов нет -- им страница не нужна.
    // Незаданные -- как есть, AGridWorldManager::EnsureGridCoversSites их
    // пропускает.
    static TArray<FIntPoint> CollectSaveSiteCells(const UHerbalistSaveGame& Save, const AGridWorldManager& Manager);

    UFUNCTION(BlueprintCallable, Category = "Herbalist|Save")
    bool SaveGame(const FString& SlotName = TEXT(""));

    UFUNCTION(BlueprintCallable, Category = "Herbalist|Save")
    bool LoadGame(const FString& SlotName = TEXT(""));
};
