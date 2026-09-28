// PcgTreesSetupCommandlet.h
//
// Деревья биомов (2026-09-28, решение пользователя: отдельный граф на том же
// BP_BiomeVolume и сплайне, запекание в редакторе, по старой схеме
// коммандлетами). Три шага, каждый -- только если ещё не сделан:
//
//   1. /Game/Data/DT_BiomeTrees (строки FHerbalistBiomeTreeRow) -- из
//      herbalist_docs/CSV_tabs/biome_trees.json. Таблица уже есть -- не
//      трогается (её правят в редакторе); -refilltable -- заполнить заново.
//   2. /Game/PCG/PCG_Trees -- граф: сплайн региона (фильтр Original, тег
//      компонента Biome) -> поверхность -> Surface Sampler (плотность на м²
//      мира: выборка по внутренности сплайна считает шаг в локальных единицах
//      и зависит от масштаба объёма) -> минус вода (сплайны с тегом Water) ->
//      на ландшафт -> не на покраске Ground (как у травы, Ground < 0.8) ->
//      Herbalist Biome Trees -> Self Pruning -> Static Mesh Spawner (меш из
//      атрибута TreeMesh). Непустой граф не трогается.
//   3. BP_BiomeVolume -- компонент PCG_Trees с этим графом, генерация по
//      запросу: в игре граф не работает, деревья запекаются билдером мира
//      (-run=WorldPartitionBuilderCommandlet ... -Builder=PCGWorldPartitionBuilder
//      -IncludeGraphNames=PCG_Trees, TOOLS_REFERENCE.md).
//
// Запуск: UnrealEditor-Cmd.exe <uproject> -run=PcgTreesSetup [-refilltable]
#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "PcgTreesSetupCommandlet.generated.h"

class UBlueprint;
class UDataTable;
class UPCGGraph;

UCLASS()
class UPcgTreesSetupCommandlet : public UCommandlet
{
    GENERATED_BODY()

public:
    virtual int32 Main(const FString& Params) override;

    // Плотность точек сэмплера на 100 м²; узел деревьев знает её же.
    static constexpr float SamplerTreesPer100SquareMeters = 4.0f;
    // Как у травы: на покраске тропы (слой Ground) деревьев нет.
    static constexpr float GroundLayerMax = 0.8f;

    // Собрать граф в пустом графе. 1 -- собран, 0 -- граф не пуст, не трогали,
    // -1 -- ошибка (узел не создался, связь не встала). Открыто для тестов.
    static int32 BuildTreesGraph(UPCGGraph* Graph);
    // Строки из JSON в пустую (или очищенную) таблицу. Число строк, -1 -- ошибка.
    static int32 FillTreeTable(UDataTable* Table, const FString& JsonText);
    // Компонент PCG_Trees в BP_BiomeVolume. 1 -- добавлен/исправлен, 0 -- уже был.
    static int32 EnsureTreesComponent(UBlueprint* Blueprint, UPCGGraph* Graph);
};
