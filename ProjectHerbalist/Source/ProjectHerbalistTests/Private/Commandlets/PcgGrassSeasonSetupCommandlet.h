// PcgGrassSeasonSetupCommandlet.h
//
// Два шага, оба идемпотентны. (1) Исключение травы на покраске Ground --
// см. FixGroundExclusion ниже. (2) Вставляет «Sample Herbalist Cell» в /Game/PCG/PCG_Grass (2026-09-17, этап 5
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

    // Вставка в граф без сохранения (тест на копии) -- перед Attribute Noise,
    // что бы ни стояло до него. 1 -- вставлен, 0 -- уже был, -1 -- у Attribute
    // Noise нет входящей связи или узел не связался.
    static int32 InsertSeasonSampler(UPCGGraph* Graph);

    // Исключение травы на покраске слоя Ground (2026-09-19, вопрос
    // пользователя: трава росла на исключённой покраске). Было: точки
    // ландшафта Surface Sampler (0.5 на м², полуразмер 25 см, со случайным
    // сдвигом) -> Filter Attribute Elements (Ground = @Last: постоянный порог
    // 0.8 выключен) -> OutsideFilter -> Difference из травы. Сравнение с
    // @Last вместо порога и редкие точки-вычитатели (квадрат 50x50 см на ~2 м²)
    // -- трава оставалась почти везде. Стало: точкам травы после World
    // Raycast проецируется ландшафт (вес слоя Ground в атрибут), тот же фильтр
    // с постоянным порогом и оператором «меньше» пропускает только точки, где
    // Ground < порога (порог пользователя, 0.8, сохранён); старая ветка
    // вычитания убрана. 1 -- исправлено, 0 -- уже, -1 -- граф не узнан.
    static int32 FixGroundExclusion(UPCGGraph* Graph);
};
