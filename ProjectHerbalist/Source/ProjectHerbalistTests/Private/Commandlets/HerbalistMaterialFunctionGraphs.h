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
// Слой сезона и суток (2026-09-16, этап 3 docs/research/
// DESIGN_Living_Vegetation_Research.md §3): значения времени приходят готовыми
// из MPC_WorldStateFields (этап 1б), снег -- из коллекции Ultra Dynamic Weather.
//   MF_SeasonWeights -- веса сезонов, SeasonUDW, LeafDrop01.
//   MF_SeasonColor   -- цвет, подкрашенный по сезону (свежесть, сочность,
//                       желтизна, пожухлость).
//   MF_LeafDrop      -- маска листвы редеет по LeafDrop01 кучками, со
//                       сдвигом по экземпляру.
//   MF_GrassSquash   -- трава ложится к основанию: max(тропа, зима, снег UDW);
//                       обобщение MF_TrampleCompressWPO.
//   MF_FlowerOpen    -- раскрытость цветка по окну OpenPhase (веса фаз суток)
//                       и WPO закрытия лепестков.
//
// Сборка графа вынесена из коммандлета, чтобы автотест проверял тот же код
// на временных объектах.
#pragma once

#include "CoreMinimal.h"

class UMaterial;
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
    inline const TCHAR* SeasonWeightsName = TEXT("MF_SeasonWeights");
    inline const TCHAR* SeasonColorName = TEXT("MF_SeasonColor");
    inline const TCHAR* LeafDropName = TEXT("MF_LeafDrop");
    inline const TCHAR* GrassSquashName = TEXT("MF_GrassSquash");
    inline const TCHAR* FlowerOpenName = TEXT("MF_FlowerOpen");
    inline const TCHAR* PlayerPushName = TEXT("MF_PlayerPushWPO");

    // Коллекция Ultra Dynamic Weather: покрытие снегом материалов -- Snowy.
    inline const TCHAR* WeatherCollectionPath = TEXT("/Game/UltraDynamicSky/Materials/Weather/UltraDynamicWeather_Parameters");
    inline const TCHAR* WeatherSnowParameterName = TEXT("Snowy");

    // Имя переключателя -- то же, что в схеме бэклога и в инстансах травы.
    inline const TCHAR* TrampleableSwitchName = TEXT("Trampleable");
    // Трава и кусты расступаются перед игроком (MF_PlayerPushWPO, 2026-09-28).
    inline const TCHAR* PushableSwitchName = TEXT("Pushable");

    // Тропа в мастер-материалах (-wire, 2026-09-27). Трава -- сжатие в WPO,
    // ландшафт -- выборка по пикселю для Lerp к земле тропы.
    inline const TCHAR* GrassMaterialPaths[] = {
        TEXT("/Game/Stylized_PBR_Nature/Foliage/Materials/M_Foliage_Master"),
        TEXT("/Game/Stylized_Forest/Materials/plants/M_plants"),
    };
    inline const TCHAR* LandscapeMaterialPath = TEXT("/Game/Stylized_Forest/Materials/landscape/M_landscape");
    // Низкий покров ложится на тропе; кусты и листва деревьев на тех же
    // мастерах стоят (CHANGELOG.md, 2026-09-12, «схема травы на тропе»).
    inline const TCHAR* TrampleableInstancePaths[] = {
        TEXT("/Game/Stylized_PBR_Nature/Foliage/Materials/MI_Grass"),
        TEXT("/Game/Stylized_PBR_Nature/Foliage/Materials/MI_Clover"),
        TEXT("/Game/Stylized_PBR_Nature/Foliage/Materials/MI_Fern"),
        TEXT("/Game/Stylized_Forest/Materials/plants/MI_grass_01_Inst"),
        TEXT("/Game/Stylized_Forest/Materials/plants/MI_grass_02_Inst"),
        TEXT("/Game/Stylized_Forest/Materials/plants/MI_flower_01_Inst"),
        TEXT("/Game/Stylized_Forest/Materials/plants/MI_flower_02_Inst"),
    };
    // Расступаются перед игроком: низкий покров и кусты (решение пользователя
    // 2026-09-28: «как и кусты, деревья гнуть не надо»). Листва деревьев на
    // тех же мастерах -- нет.
    inline const TCHAR* PushableInstancePaths[] = {
        TEXT("/Game/Stylized_PBR_Nature/Foliage/Materials/MI_Grass"),
        TEXT("/Game/Stylized_PBR_Nature/Foliage/Materials/MI_Clover"),
        TEXT("/Game/Stylized_PBR_Nature/Foliage/Materials/MI_Fern"),
        TEXT("/Game/Stylized_PBR_Nature/Foliage/Materials/MI_Bush"),
        TEXT("/Game/Stylized_Forest/Materials/plants/MI_grass_01_Inst"),
        TEXT("/Game/Stylized_Forest/Materials/plants/MI_grass_02_Inst"),
        TEXT("/Game/Stylized_Forest/Materials/plants/MI_flower_01_Inst"),
        TEXT("/Game/Stylized_Forest/Materials/plants/MI_flower_02_Inst"),
        TEXT("/Game/Stylized_Forest/Materials/plants/MI_shrub_01_Inst"),
        TEXT("/Game/Stylized_Forest/Materials/plants/MI_shrub_flower_01_Inst"),
    };

    struct FSources
    {
        UMaterialParameterCollection* Collection = nullptr;
        UTexture* WorldStateMap = nullptr;
        UTexture* TrampleMap = nullptr;
        UMaterialParameterCollection* WeatherCollection = nullptr;
    };

    // Строят граф в пустой функции. false -- чего-то не хватает (параметра в
    // MPC, текстуры), причина в логе, функция недостроена.
    bool BuildSampleWorldState(UMaterialFunction* Function, const FSources& Sources);
    bool BuildSampleTrample(UMaterialFunction* Function, const FSources& Sources);
    bool BuildTrampleCompressWPO(UMaterialFunction* Function, UMaterialFunction* SampleTrample);

    bool BuildSeasonWeights(UMaterialFunction* Function, const FSources& Sources);
    bool BuildSeasonColor(UMaterialFunction* Function, const FSources& Sources);
    bool BuildLeafDrop(UMaterialFunction* Function, const FSources& Sources);
    bool BuildGrassSquash(UMaterialFunction* Function, const FSources& Sources, UMaterialFunction* SampleTrample);
    bool BuildFlowerOpen(UMaterialFunction* Function, const FSources& Sources);
    // Трава расступается перед игроком: вершина уходит от TramplePlayerPosition
    // по горизонтали пропорционально своей высоте над основанием экземпляра,
    // сила спадает к Radius, выше или ниже игрока на пару метров -- нет. За
    // переключателем Pushable (по умолчанию выключен).
    bool BuildPlayerPushWPO(UMaterialFunction* Function, const FSources& Sources);

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

    enum class EWireResult
    {
        AlreadyWired,   // уже так, материал не тронут
        Wired,
        Failed,         // причина в логе
    };

    // Трава: то, что было в World Position Offset (ветер), -> вход WPO вызова
    // MF_TrampleCompressWPO -> World Position Offset. Узлы материала не
    // трогаются, вызов встаёт последним. Вызов уже есть, но не в WPO --
    // Failed: подключено руками иначе, второй вызов считал бы тропу дважды.
    EWireResult WireTrampleCompressIntoWPO(UMaterial* Material, UMaterialFunction* Compress);
    // То же для любой функции со входом и выходом WPO: встаёт последней в
    // цепочку World Position Offset (2026-09-28: MF_PlayerPushWPO -- после сжатия).
    EWireResult WireFunctionIntoWPO(UMaterial* Material, UMaterialFunction* Function);

    // Ландшафт: вызовы MF_TrampleCompressWPO заменяются MF_SampleTrample.
    // Сжатие читает тропу в основании экземпляра, а у ландшафта это начало
    // компонента -- один тексель на весь компонент; выборке нужна позиция
    // пикселя (вход Position не подключён). Выход Trample переходит на
    // выборку, выход WPO -- на то, что было на его входе (переключатель в
    // ландшафте выключен, выход равен входу).
    EWireResult ReplaceCompressWithSampleTrample(UMaterial* Material, UMaterialFunction* Compress, UMaterialFunction* SampleTrample);
}
