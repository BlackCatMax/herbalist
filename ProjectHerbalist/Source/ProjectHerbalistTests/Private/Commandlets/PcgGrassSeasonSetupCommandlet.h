// PcgGrassSeasonSetupCommandlet.h
//
// Вставляет «Sample Herbalist Cell» в /Game/PCG/PCG_Grass (2026-09-17, этап 5
// docs/research/DESIGN_Living_Vegetation_Research.md §4, правка графа
// разрешена пользователем): между World Raycast и Attribute Noise -- у точек
// уже окончательные позиции на ландшафте, а шум ещё не переписал плотность.
// Узел пишет точкам состояние клетки, MeshKey, SeasonKey, SeasonMeshKey и
// прореживает траву по доле сезона.
//
// Спавнеры не трогает: мешей по сезонам и для испорченных клеток в проекте
// нет, выбор по SeasonMeshKey (PCGMeshSelectorByAttribute) заводится, когда
// они появятся. Идемпотентен: узел уже в графе -- ничего не делает.
//
// Запуск: UnrealEditor-Cmd.exe <uproject> -run=PcgGrassSeasonSetup
#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "PcgGrassSeasonSetupCommandlet.generated.h"

class UPCGGraph;

UCLASS()
class UPcgGrassSeasonSetupCommandlet : public UCommandlet
{
    GENERATED_BODY()

public:
    virtual int32 Main(const FString& Params) override;

    // Вставка в граф без сохранения (тест на копии). 1 -- вставлен, 0 -- уже
    // был, -1 -- в графе нет связи World Raycast -> Attribute Noise.
    static int32 InsertSeasonSampler(UPCGGraph* Graph);
};
