// HerbalistMaterialFunctionGraphs.h
//
// Функции материалов, связывающие шейдеры с картами мира (2026-09-16, бэклог:
// ROADMAP.md «PCG/материалы» и «Тропы», схема нод -- CHANGELOG.md, запись
// «хвосты: что подтверждено в движке, схема травы на тропе»). До этого каждый
// материал (M_landscape, M_Foliage_Master, M_Floor) собирал UV карт вручную.
//
//   MF_SampleWorldState   -- RT_WorldStateMap по мировой позиции: четыре оси
//                            клетки, UV окна и маска «внутри окна» (за окном
//                            карта отдаёт крайние тексели, см. ROADMAP.md,
//                            «Разметка мира»).
//   MF_SampleTrample      -- RT_TrampleMap: вытоптанность с затуханием к краю
//                            окна вокруг игрока (шаг 1 схемы).
//   MF_TrampleCompressWPO -- трава на тропе прижимается к основанию, ветер
//                            слабеет; за переключателем Trampleable (шаг 2).
//
// Сборка графа вынесена из коммандлета, чтобы автотест проверял тот же код
// на временных объектах.
#pragma once

#include "CoreMinimal.h"

class UMaterialFunction;
class UMaterialParameterCollection;
class UTexture;

namespace HerbalistMaterialFunctions
{
    inline const TCHAR* CollectionPath = TEXT("/Game/Materials/MPC_WorldStateFields");
    inline const TCHAR* WorldStateMapPath = TEXT("/Game/Materials/RT_WorldStateMap");
    inline const TCHAR* TrampleMapPath = TEXT("/Game/Materials/RT_TrampleMap");
    inline const TCHAR* FunctionsFolder = TEXT("/Game/Materials/Functions");

    inline const TCHAR* SampleWorldStateName = TEXT("MF_SampleWorldState");
    inline const TCHAR* SampleTrampleName = TEXT("MF_SampleTrample");
    inline const TCHAR* TrampleCompressName = TEXT("MF_TrampleCompressWPO");

    // Имя переключателя -- то же, что в схеме бэклога и в инстансах травы.
    inline const TCHAR* TrampleableSwitchName = TEXT("Trampleable");

    struct FSources
    {
        UMaterialParameterCollection* Collection = nullptr;
        UTexture* WorldStateMap = nullptr;
        UTexture* TrampleMap = nullptr;
    };

    // Строят граф в пустой функции. false -- чего-то не хватает (параметра в
    // MPC, текстуры), причина в логе, функция недостроена.
    bool BuildSampleWorldState(UMaterialFunction* Function, const FSources& Sources);
    bool BuildSampleTrample(UMaterialFunction* Function, const FSources& Sources);
    bool BuildTrampleCompressWPO(UMaterialFunction* Function, UMaterialFunction* SampleTrample);

    // Удаляет все узлы функции. false -- что-то осталось. Своя, а не
    // UMaterialEditingLibrary::DeleteAllMaterialExpressionsInFunction: та удаляет
    // из массива, обходя его же, и оставляет каждый второй узел (ревью 2026-09-16).
    bool ClearMaterialFunction(UMaterialFunction* Function);

    // Id входов и выходов по имени. Вызов функции в материале связан с её
    // входами и выходами по Id, и перестройка графа без восстановления Id
    // молча обрывала бы подключения в материалах.
    struct FFunctionPinIds
    {
        TMap<FName, FGuid> Inputs;
        TMap<FName, FGuid> Outputs;
    };
    FFunctionPinIds CaptureFunctionPinIds(const UMaterialFunction* Function);
    void RestoreFunctionPinIds(UMaterialFunction* Function, const FFunctionPinIds& PinIds);
}
