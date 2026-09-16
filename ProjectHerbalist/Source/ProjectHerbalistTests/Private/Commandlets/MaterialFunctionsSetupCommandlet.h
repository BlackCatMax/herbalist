// MaterialFunctionsSetupCommandlet.h
//
// Собирает функции материалов для карт мира (2026-09-16), см.
// HerbalistMaterialFunctionGraphs.h: MF_SampleWorldState, MF_SampleTrample,
// MF_TrampleCompressWPO и слой сезона (MF_SeasonWeights, MF_SeasonColor,
// MF_LeafDrop, MF_GrassSquash, MF_FlowerOpen) в /Game/Materials/Functions.
//
// Существующую функцию не трогает (её могли поправить в редакторе); -rebuild
// перестраивает граф заново. Материалы не трогает: подключение -- в
// docs/reference/TOOLS_REFERENCE.md.
//
// Нужны RT_WorldStateMap, RT_TrampleMap и параметры в MPC_WorldStateFields --
// сначала -run=WorldStateMapSetup, -run=TrampleMapSetup и -run=TimeDisplaySetup;
// снег MF_GrassSquash -- из коллекции Ultra Dynamic Weather.
//
// -verify компилирует каждую функцию во временном материале. Только без
// -nullrhi и с -AllowCommandletRendering: иначе коммандлет не создаёт ресурсы
// рендера материалов, и проверять нечего.
//
// Запуск: UnrealEditor-Cmd.exe <uproject> -run=MaterialFunctionsSetup [-rebuild] [-verify -AllowCommandletRendering]
#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "MaterialFunctionsSetupCommandlet.generated.h"

UCLASS()
class UMaterialFunctionsSetupCommandlet : public UCommandlet
{
    GENERATED_BODY()

public:
    virtual int32 Main(const FString& Params) override;
};
