// Source/ProjectHerbalistTests/Private/Tests/MaterialFunctionsTest.cpp
//
// Функции материалов для карт мира (2026-09-16, HerbalistMaterialFunctionGraphs.h).
// Графы строятся на временных объектах тем же кодом, что у коммандлета, и
// сверяются со схемой бэклога: входы и выходы, цепочки узлов от выходов (каналы,
// шаги маски окна, затухание, сжатие), параметры MPC, явный мип 0 (шейдер
// вершин), Instance & Particle Space у основания, позиция без смещений шейдера,
// переключатель Trampleable, перестройка графа; слой сезона и суток (этап 3
// DESIGN_Living_Vegetation_Research.md): веса сезона, подкраска, листопад,
// сжатие травы, раскрытие цветов. Компиляцию шейдера проверяет
// -run=MaterialFunctionsSetup -verify (TOOLS_REFERENCE.md) -- автотест в
// редакторском мире без рендера её не видит.

#include "Commandlets/HerbalistMaterialFunctionGraphs.h"

#include "Engine/Texture.h"
#include "Materials/MaterialFunction.h"
#include "Materials/MaterialParameterCollection.h"
#include "Materials/MaterialExpressionCollectionParameter.h"
#include "Materials/MaterialExpressionAdd.h"
#include "Materials/MaterialExpressionAppendVector.h"
#include "Materials/MaterialExpressionDotProduct.h"
#include "Materials/MaterialExpressionFrac.h"
#include "Materials/MaterialExpressionMax.h"
#include "Materials/MaterialExpressionSaturate.h"
#include "Materials/MaterialExpressionPerInstanceRandom.h"
#include "Materials/MaterialExpressionFunctionInput.h"
#include "Materials/MaterialExpressionFunctionOutput.h"
#include "Materials/MaterialExpressionLength.h"
#include "Materials/MaterialExpressionLocalPosition.h"
#include "Materials/MaterialExpressionMaterialFunctionCall.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "Materials/MaterialExpressionOneMinus.h"
#include "Materials/MaterialExpressionSmoothStep.h"
#include "Materials/MaterialExpressionStaticSwitchParameter.h"
#include "Materials/MaterialExpressionStep.h"
#include "Materials/MaterialExpressionSubtract.h"
#include "Materials/MaterialExpressionTextureSample.h"
#include "Materials/MaterialExpressionTransform.h"
#include "Materials/MaterialExpressionTransformPosition.h"
#include "Materials/MaterialExpressionWorldPosition.h"
#include "Misc/AutomationTest.h"

#if WITH_AUTOMATION_TESTS && WITH_EDITOR

namespace HerbalistMaterialFunctionsTest
{
    using namespace HerbalistMaterialFunctions;

    FSources LoadMaterialFunctionSources()
    {
        FSources Sources;
        Sources.Collection = LoadObject<UMaterialParameterCollection>(nullptr, CollectionPath);
        Sources.WorldStateMap = LoadObject<UTexture>(nullptr, WorldStateMapPath);
        Sources.TrampleMap = LoadObject<UTexture>(nullptr, TrampleMapPath);
        Sources.WeatherCollection = LoadObject<UMaterialParameterCollection>(nullptr, WeatherCollectionPath);
        return Sources;
    }

    UMaterialFunction* NewTransientMaterialFunction()
    {
        return NewObject<UMaterialFunction>(GetTransientPackage(), NAME_None, RF_Transient);
    }

    TSet<FName> FunctionInputNames(const UMaterialFunction* Function)
    {
        TArray<FFunctionExpressionInput> Inputs;
        TArray<FFunctionExpressionOutput> Outputs;
        Function->GetInputsAndOutputs(Inputs, Outputs);
        TSet<FName> Names;
        for (const FFunctionExpressionInput& Input : Inputs)
        {
            if (Input.ExpressionInput) Names.Add(Input.ExpressionInput->InputName);
        }
        return Names;
    }

    TSet<FName> FunctionOutputNames(const UMaterialFunction* Function)
    {
        TArray<FFunctionExpressionInput> Inputs;
        TArray<FFunctionExpressionOutput> Outputs;
        Function->GetInputsAndOutputs(Inputs, Outputs);
        TSet<FName> Names;
        for (const FFunctionExpressionOutput& Output : Outputs)
        {
            if (Output.ExpressionOutput) Names.Add(Output.ExpressionOutput->OutputName);
        }
        return Names;
    }

    template <typename T>
    TArray<T*> FunctionNodesOfType(UMaterialFunction* Function)
    {
        TArray<T*> Result;
        for (UMaterialExpression* Expression : Function->GetExpressionCollection().Expressions)
        {
            if (T* Typed = Cast<T>(Expression)) Result.Add(Typed);
        }
        return Result;
    }

    // Общие проверки: у каждого выхода есть источник, параметры MPC найдены по
    // Id, карты читаются с явным мипом.
    void CheckCommonMaterialFunctionWiring(FAutomationTestBase& Test, UMaterialFunction* Function, UTexture* ExpectedTexture)
    {
        for (UMaterialExpressionFunctionOutput* Output : FunctionNodesOfType<UMaterialExpressionFunctionOutput>(Function))
        {
            Test.TestTrue(*FString::Printf(TEXT("Выход %s подключён"), *Output->OutputName.ToString()), Output->A.IsConnected());
        }
        for (UMaterialExpressionCollectionParameter* Parameter : FunctionNodesOfType<UMaterialExpressionCollectionParameter>(Function))
        {
            Test.TestTrue(*FString::Printf(TEXT("Параметр %s найден в MPC по Id"), *Parameter->ParameterName.ToString()),
                Parameter->Collection && Parameter->Collection->GetParameterName(Parameter->ParameterId) == Parameter->ParameterName);
        }
        const TArray<UMaterialExpressionTextureSample*> Samples = FunctionNodesOfType<UMaterialExpressionTextureSample>(Function);
        if (ExpectedTexture && Test.TestEqual(TEXT("Одна выборка карты"), Samples.Num(), 1))
        {
            Test.TestEqual(TEXT("Читается нужная карта"), Samples[0]->Texture.Get(), ExpectedTexture);
            Test.TestEqual(TEXT("Явный мип -- годится для шейдера вершин"), Samples[0]->MipValueMode.GetValue(), TMVM_MipLevel);
            Test.TestEqual(TEXT("Мип 0, а не INDEX_NONE по умолчанию"), Samples[0]->ConstMipValue, 0);
            Test.TestTrue(TEXT("UV подключены"), Samples[0]->Coordinates.IsConnected());
        }
        // Позиция по умолчанию -- без смещений шейдера, иначе цикл через WPO.
        for (UMaterialExpressionWorldPosition* Position : FunctionNodesOfType<UMaterialExpressionWorldPosition>(Function))
        {
            Test.TestEqual(TEXT("Позиция по умолчанию без смещений шейдера"), Position->WorldPositionShaderOffset.GetValue(), WPT_ExcludeAllShaderOffsets);
        }
    }

    UMaterialExpressionFunctionOutput* FindFunctionOutput(UMaterialFunction* Function, const TCHAR* Name)
    {
        for (UMaterialExpressionFunctionOutput* Output : FunctionNodesOfType<UMaterialExpressionFunctionOutput>(Function))
        {
            if (Output->OutputName == FName(Name)) return Output;
        }
        return nullptr;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistMaterialFunctions_SampleWorldStateReadsWindowFrame,
    "Herbalist.MaterialFunctions.SampleWorldStateReadsWindowFrame",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistMaterialFunctions_SampleWorldStateReadsWindowFrame::RunTest(const FString& Parameters)
{
    using namespace HerbalistMaterialFunctionsTest;
    const FSources Sources = LoadMaterialFunctionSources();
    if (!TestNotNull(TEXT("MPC_WorldStateFields"), Sources.Collection) || !TestNotNull(TEXT("RT_WorldStateMap"), Sources.WorldStateMap)) return false;

    UMaterialFunction* Function = NewTransientMaterialFunction();
    if (!TestTrue(TEXT("Граф собран"), BuildSampleWorldState(Function, Sources))) return false;

    TestEqual(TEXT("Вход -- мировая позиция"), FunctionInputNames(Function).Array(), TArray<FName>{ FName(TEXT("WorldPosition")) });
    const TSet<FName> Outputs = FunctionOutputNames(Function);
    for (const TCHAR* Name : { TEXT("Distortion"), TEXT("Corruption"), TEXT("HarvestStress"), TEXT("ShrineInfluence"), TEXT("UV"), TEXT("InsideWindow") })
    {
        TestTrue(*FString::Printf(TEXT("Есть выход %s"), Name), Outputs.Contains(FName(Name)));
    }
    TestEqual(TEXT("Шесть выходов"), Outputs.Num(), 6);

    TSet<FName> ParameterNames;
    for (UMaterialExpressionCollectionParameter* Parameter : FunctionNodesOfType<UMaterialExpressionCollectionParameter>(Function))
    {
        ParameterNames.Add(Parameter->ParameterName);
    }
    TestTrue(TEXT("Рамка окна: начало"), ParameterNames.Contains(FName(TEXT("WorldStateMapOrigin"))));
    TestTrue(TEXT("Рамка окна: размер"), ParameterNames.Contains(FName(TEXT("WorldStateMapSize"))));

    CheckCommonMaterialFunctionWiring(*this, Function, Sources.WorldStateMap);

    // Каналы: выход берётся из выборки карты, из своего выхода (1 R, 2 G, 3 B, 4 A).
    struct FChannel { const TCHAR* Output; int32 SampleOutput; };
    for (const FChannel& Channel : { FChannel{ TEXT("Distortion"), 1 }, FChannel{ TEXT("Corruption"), 2 },
                                     FChannel{ TEXT("HarvestStress"), 3 }, FChannel{ TEXT("ShrineInfluence"), 4 } })
    {
        const UMaterialExpressionFunctionOutput* Output = FindFunctionOutput(Function, Channel.Output);
        TestTrue(*FString::Printf(TEXT("%s -- из выборки карты"), Channel.Output),
            Output && Cast<UMaterialExpressionTextureSample>(Output->A.Expression) && Output->A.OutputIndex == Channel.SampleOutput);
    }

    // Маска окна: четыре шага, два «>= 0» и два «<= 1», перемножены.
    int32 StepsAboveZero = 0;
    int32 StepsBelowOne = 0;
    for (UMaterialExpressionStep* Step : FunctionNodesOfType<UMaterialExpressionStep>(Function))
    {
        StepsAboveZero += (Step->X.IsConnected() && !Step->Y.IsConnected() && Step->ConstY == 0.0f) ? 1 : 0;
        StepsBelowOne += (Step->Y.IsConnected() && !Step->X.IsConnected() && Step->ConstX == 1.0f) ? 1 : 0;
    }
    TestEqual(TEXT("Два шага U,V >= 0"), StepsAboveZero, 2);
    TestEqual(TEXT("Два шага U,V <= 1"), StepsBelowOne, 2);
    const UMaterialExpressionFunctionOutput* Inside = FindFunctionOutput(Function, TEXT("InsideWindow"));
    const UMaterialExpressionMultiply* InsideProduct = Inside ? Cast<UMaterialExpressionMultiply>(Inside->A.Expression) : nullptr;
    TestTrue(TEXT("InsideWindow -- произведение масок по U и по V"), InsideProduct
        && Cast<UMaterialExpressionMultiply>(InsideProduct->A.Expression) && Cast<UMaterialExpressionMultiply>(InsideProduct->B.Expression));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistMaterialFunctions_SampleTrampleFadesToWindowEdge,
    "Herbalist.MaterialFunctions.SampleTrampleFadesToWindowEdge",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistMaterialFunctions_SampleTrampleFadesToWindowEdge::RunTest(const FString& Parameters)
{
    using namespace HerbalistMaterialFunctionsTest;
    const FSources Sources = LoadMaterialFunctionSources();
    if (!TestNotNull(TEXT("MPC_WorldStateFields"), Sources.Collection) || !TestNotNull(TEXT("RT_TrampleMap"), Sources.TrampleMap)) return false;

    UMaterialFunction* Function = NewTransientMaterialFunction();
    if (!TestTrue(TEXT("Граф собран"), BuildSampleTrample(Function, Sources))) return false;

    TestEqual(TEXT("Вход -- позиция"), FunctionInputNames(Function).Array(), TArray<FName>{ FName(TEXT("Position")) });
    const TSet<FName> Outputs = FunctionOutputNames(Function);
    TestTrue(TEXT("Есть выход Trample"), Outputs.Contains(FName(TEXT("Trample"))));
    TestTrue(TEXT("Есть выход RawTrample"), Outputs.Contains(FName(TEXT("RawTrample"))));
    TestTrue(TEXT("Есть выход Fade"), Outputs.Contains(FName(TEXT("Fade"))));

    TSet<FName> ParameterNames;
    for (UMaterialExpressionCollectionParameter* Parameter : FunctionNodesOfType<UMaterialExpressionCollectionParameter>(Function))
    {
        ParameterNames.Add(Parameter->ParameterName);
    }
    TestTrue(TEXT("Рамка троп"), ParameterNames.Contains(FName(TEXT("TrampleMapFrame"))));
    TestTrue(TEXT("Позиция игрока"), ParameterNames.Contains(FName(TEXT("TramplePlayerPosition"))));

    CheckCommonMaterialFunctionWiring(*this, Function, Sources.TrampleMap);

    // Trample = R карты x (1 - SmoothStep(начало, конец, расстояние до игрока)).
    const UMaterialExpressionFunctionOutput* Trample = FindFunctionOutput(Function, TEXT("Trample"));
    const UMaterialExpressionMultiply* Product = Trample ? Cast<UMaterialExpressionMultiply>(Trample->A.Expression) : nullptr;
    const UMaterialExpressionOneMinus* Fade = Product ? Cast<UMaterialExpressionOneMinus>(Product->B.Expression) : nullptr;
    const UMaterialExpressionSmoothStep* Edge = Fade ? Cast<UMaterialExpressionSmoothStep>(Fade->Input.Expression) : nullptr;
    TestTrue(TEXT("Trample -- произведение"), Product != nullptr);
    TestTrue(TEXT("Множитель A -- канал R карты"), Product && Cast<UMaterialExpressionTextureSample>(Product->A.Expression) && Product->A.OutputIndex == 1);
    TestTrue(TEXT("Множитель B -- 1 - SmoothStep"), Edge != nullptr);
    TestTrue(TEXT("SmoothStep: начало и конец из рамки, значение -- расстояние"), Edge
        && Edge->Min.IsConnected() && Edge->Max.IsConnected() && Cast<UMaterialExpressionLength>(Edge->Value.Expression));
    const UMaterialExpressionFunctionOutput* FadeOutput = FindFunctionOutput(Function, TEXT("Fade"));
    TestTrue(TEXT("Fade -- тот же 1 - SmoothStep"), FadeOutput && FadeOutput->A.Expression == Fade);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistMaterialFunctions_TrampleCompressFollowsBacklogScheme,
    "Herbalist.MaterialFunctions.TrampleCompressFollowsBacklogScheme",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistMaterialFunctions_TrampleCompressFollowsBacklogScheme::RunTest(const FString& Parameters)
{
    using namespace HerbalistMaterialFunctionsTest;
    const FSources Sources = LoadMaterialFunctionSources();
    UMaterialFunction* SampleTrample = NewTransientMaterialFunction();
    if (!TestTrue(TEXT("MF_SampleTrample собрана"), BuildSampleTrample(SampleTrample, Sources))) return false;

    UMaterialFunction* Function = NewTransientMaterialFunction();
    if (!TestTrue(TEXT("Граф собран"), BuildTrampleCompressWPO(Function, SampleTrample))) return false;

    TestEqual(TEXT("Вход -- ветер"), FunctionInputNames(Function).Array(), TArray<FName>{ FName(TEXT("WPO")) });
    const TSet<FName> Outputs = FunctionOutputNames(Function);
    TestTrue(TEXT("Есть выход WPO"), Outputs.Contains(FName(TEXT("WPO"))));
    TestTrue(TEXT("Есть выход Trample"), Outputs.Contains(FName(TEXT("Trample"))));
    CheckCommonMaterialFunctionWiring(*this, Function, nullptr);

    const TArray<UMaterialExpressionStaticSwitchParameter*> Switches = FunctionNodesOfType<UMaterialExpressionStaticSwitchParameter>(Function);
    if (TestEqual(TEXT("Один переключатель"), Switches.Num(), 1))
    {
        TestEqual(TEXT("Имя переключателя -- как в инстансах"), Switches[0]->ParameterName, FName(TrampleableSwitchName));
        TestFalse(TEXT("По умолчанию выключен"), static_cast<bool>(Switches[0]->DefaultValue));
        TestTrue(TEXT("True -- сжатие"), Switches[0]->A.IsConnected());
        TestTrue(TEXT("False -- ветер как есть"), Switches[0]->B.IsConnected());
    }

    const TArray<UMaterialExpressionTransformPosition*> Pivots = FunctionNodesOfType<UMaterialExpressionTransformPosition>(Function);
    if (TestEqual(TEXT("Одно основание"), Pivots.Num(), 1))
    {
        TestEqual(TEXT("Основание из Instance & Particle Space (не Local: Nanite)"), Pivots[0]->TransformSourceType.GetValue(), TRANSFORMPOSSOURCE_Instance);
        TestEqual(TEXT("В абсолютный мир"), Pivots[0]->TransformType.GetValue(), TRANSFORMPOSSOURCE_World);
    }
    const TArray<UMaterialExpressionTransform*> Transforms = FunctionNodesOfType<UMaterialExpressionTransform>(Function);
    if (TestEqual(TEXT("Одно преобразование сжатия"), Transforms.Num(), 1))
    {
        TestEqual(TEXT("Сжатие из Instance & Particle Space"), Transforms[0]->TransformSourceType.GetValue(), TRANSFORMSOURCE_Instance);
        TestEqual(TEXT("Сжатие в мир"), Transforms[0]->TransformType.GetValue(), TRANSFORM_World);
    }
    const TArray<UMaterialExpressionLocalPosition*> Locals = FunctionNodesOfType<UMaterialExpressionLocalPosition>(Function);
    if (TestEqual(TEXT("Одна локальная позиция"), Locals.Num(), 1))
    {
        TestTrue(TEXT("Без смещений шейдера -- иначе цикл через WPO"), Locals[0]->IncludedOffsets == EPositionIncludedOffsets::ExcludeOffsets);
        TestTrue(TEXT("От экземпляра"), Locals[0]->LocalOrigin == ELocalPositionOrigin::Instance);
    }
    const TArray<UMaterialExpressionMaterialFunctionCall*> Calls = FunctionNodesOfType<UMaterialExpressionMaterialFunctionCall>(Function);
    if (TestEqual(TEXT("Один вызов MF_SampleTrample"), Calls.Num(), 1))
    {
        TestEqual(TEXT("Зовёт выборку тропы"), Calls[0]->MaterialFunction.Get(), static_cast<UMaterialFunctionInterface*>(SampleTrample));
        bool bPositionConnected = false;
        for (const FFunctionExpressionInput& Input : Calls[0]->FunctionInputs)
        {
            bPositionConnected |= Input.ExpressionInput && Input.ExpressionInput->InputName == FName(TEXT("Position")) && Input.Input.IsConnected();
        }
        TestTrue(TEXT("Тропа читается в основании"), bPositionConnected);
    }

    // WPO: True = ветер x (1 - V) - Transform(LocalPosition x V), False = ветер.
    const UMaterialExpressionFunctionOutput* Wpo = FindFunctionOutput(Function, TEXT("WPO"));
    const UMaterialExpressionStaticSwitchParameter* Switch = Wpo ? Cast<UMaterialExpressionStaticSwitchParameter>(Wpo->A.Expression) : nullptr;
    TestTrue(TEXT("WPO -- из переключателя"), Switch != nullptr);
    TestTrue(TEXT("False -- вход ветра как есть"), Switch && Cast<UMaterialExpressionFunctionInput>(Switch->B.Expression));
    const UMaterialExpressionSubtract* Trampled = Switch ? Cast<UMaterialExpressionSubtract>(Switch->A.Expression) : nullptr;
    const UMaterialExpressionMultiply* WeakWind = Trampled ? Cast<UMaterialExpressionMultiply>(Trampled->A.Expression) : nullptr;
    const UMaterialExpressionTransform* Squash = Trampled ? Cast<UMaterialExpressionTransform>(Trampled->B.Expression) : nullptr;
    const UMaterialExpressionMultiply* LocalTimesTrample = Squash ? Cast<UMaterialExpressionMultiply>(Squash->Input.Expression) : nullptr;
    TestTrue(TEXT("True -- разность"), Trampled != nullptr);
    TestTrue(TEXT("Уменьшаемое -- ветер x (1 - V)"), WeakWind
        && Cast<UMaterialExpressionFunctionInput>(WeakWind->A.Expression) && Cast<UMaterialExpressionOneMinus>(WeakWind->B.Expression));
    TestTrue(TEXT("Вычитаемое -- сжатие в мир"), Squash != nullptr);
    TestTrue(TEXT("Сжатие -- LocalPosition x V из вызова выборки тропы"), LocalTimesTrample
        && Cast<UMaterialExpressionLocalPosition>(LocalTimesTrample->A.Expression)
        && Cast<UMaterialExpressionMaterialFunctionCall>(LocalTimesTrample->B.Expression));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistMaterialFunctions_RebuildReplacesGraphAndKeepsPinIds,
    "Herbalist.MaterialFunctions.RebuildReplacesGraphAndKeepsPinIds",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistMaterialFunctions_RebuildReplacesGraphAndKeepsPinIds::RunTest(const FString& Parameters)
{
    using namespace HerbalistMaterialFunctionsTest;
    // -rebuild: очистка удаляет граф целиком (движковая оставляла каждый второй
    // узел), Id входов и выходов переживают перестройку -- подключения в
    // материалах не рвутся.
    const FSources Sources = LoadMaterialFunctionSources();
    UMaterialFunction* Function = NewTransientMaterialFunction();
    if (!TestTrue(TEXT("Первая сборка"), BuildSampleWorldState(Function, Sources))) return false;
    const int32 NodeCount = Function->GetExpressions().Num();
    const FFunctionPinIds Before = CaptureFunctionPinIds(Function);
    TestEqual(TEXT("Id одного входа"), Before.Inputs.Num(), 1);
    TestEqual(TEXT("Id шести выходов"), Before.Outputs.Num(), 6);

    TestTrue(TEXT("Очистка удаляет все узлы"), ClearMaterialFunction(Function));
    TestEqual(TEXT("Узлов не осталось"), Function->GetExpressions().Num(), 0);

    if (!TestTrue(TEXT("Повторная сборка"), BuildSampleWorldState(Function, Sources))) return false;
    TestEqual(TEXT("Узлов столько же, без дублей"), Function->GetExpressions().Num(), NodeCount);
    const FFunctionPinIds Fresh = CaptureFunctionPinIds(Function);
    bool bAnyIdChanged = false;
    for (const TPair<FName, FGuid>& Output : Before.Outputs)
    {
        bAnyIdChanged |= Fresh.Outputs.FindRef(Output.Key) != Output.Value;
    }
    TestTrue(TEXT("Без восстановления Id новые -- иначе проверка ниже ничего не доказывает"), bAnyIdChanged);

    RestoreFunctionPinIds(Function, Before);
    const FFunctionPinIds After = CaptureFunctionPinIds(Function);
    for (const TPair<FName, FGuid>& Input : Before.Inputs)
    {
        TestEqual(*FString::Printf(TEXT("Id входа %s сохранён"), *Input.Key.ToString()), After.Inputs.FindRef(Input.Key), Input.Value);
    }
    for (const TPair<FName, FGuid>& Output : Before.Outputs)
    {
        TestEqual(*FString::Printf(TEXT("Id выхода %s сохранён"), *Output.Key.ToString()), After.Outputs.FindRef(Output.Key), Output.Value);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistMaterialFunctions_MissingCollectionParameterRefuses,
    "Herbalist.MaterialFunctions.MissingCollectionParameterRefuses",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistMaterialFunctions_MissingCollectionParameterRefuses::RunTest(const FString& Parameters)
{
    using namespace HerbalistMaterialFunctionsTest;
    // MPC без рамок: сборка отказывает с причиной, а не собирает граф с пустыми параметрами.
    FSources Sources = LoadMaterialFunctionSources();
    Sources.Collection = NewObject<UMaterialParameterCollection>(GetTransientPackage(), NAME_None, RF_Transient);
    AddExpectedError(TEXT("нет параметра"), EAutomationExpectedErrorFlags::Contains, 0);

    TestFalse(TEXT("Карта состояния без рамки -- отказ"), BuildSampleWorldState(NewTransientMaterialFunction(), Sources));
    TestFalse(TEXT("Тропы без рамки -- отказ"), BuildSampleTrample(NewTransientMaterialFunction(), Sources));
    TestFalse(TEXT("Веса сезона без параметров времени -- отказ"), BuildSeasonWeights(NewTransientMaterialFunction(), Sources));
    TestFalse(TEXT("Цветы без весов суток -- отказ"), BuildFlowerOpen(NewTransientMaterialFunction(), Sources));
    TestFalse(TEXT("Подкраска без весов сезона -- отказ"), BuildSeasonColor(NewTransientMaterialFunction(), Sources));
    TestFalse(TEXT("Листопад без LeafDrop01 -- отказ"), BuildLeafDrop(NewTransientMaterialFunction(), Sources));

    // Сжатие травы без коллекции погоды -- отказ без узлов, а не граф с пустым снегом.
    FSources NoWeather = LoadMaterialFunctionSources();
    NoWeather.WeatherCollection = nullptr;
    UMaterialFunction* SampleTrample = NewTransientMaterialFunction();
    BuildSampleTrample(SampleTrample, NoWeather);
    TestFalse(TEXT("Трава без коллекции погоды -- отказ"), BuildGrassSquash(NewTransientMaterialFunction(), NoWeather, SampleTrample));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistMaterialFunctions_GeneratedAssetsExist,
    "Herbalist.MaterialFunctions.GeneratedAssetsExist",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistMaterialFunctions_GeneratedAssetsExist::RunTest(const FString& Parameters)
{
    using namespace HerbalistMaterialFunctionsTest;
    // Ассеты собраны -run=MaterialFunctionsSetup и лежат в репозитории.
    struct FExpected { const TCHAR* Name; const TCHAR* Output; };
    for (const FExpected& Expected : { FExpected{ SampleWorldStateName, TEXT("InsideWindow") },
                                       FExpected{ SampleTrampleName, TEXT("Trample") },
                                       FExpected{ TrampleCompressName, TEXT("WPO") },
                                       FExpected{ SeasonWeightsName, TEXT("LeafDrop01") },
                                       FExpected{ SeasonColorName, TEXT("Color") },
                                       FExpected{ LeafDropName, TEXT("OpacityMask") },
                                       FExpected{ GrassSquashName, TEXT("Squash") },
                                       FExpected{ FlowerOpenName, TEXT("Open") } })
    {
        const FString Path = FString::Printf(TEXT("%s/%s"), FunctionsFolder, Expected.Name);
        UMaterialFunction* Function = LoadObject<UMaterialFunction>(nullptr, *Path);
        if (TestNotNull(*FString::Printf(TEXT("%s на диске"), Expected.Name), Function))
        {
            TestTrue(*FString::Printf(TEXT("%s: выход %s"), Expected.Name, Expected.Output), FunctionOutputNames(Function).Contains(FName(Expected.Output)));
        }
    }
    return true;
}

namespace HerbalistMaterialFunctionsTest
{
    bool UsesCollectionParameter(UMaterialFunction* Function, const UMaterialParameterCollection* Collection, const TCHAR* Name)
    {
        for (UMaterialExpressionCollectionParameter* Parameter : FunctionNodesOfType<UMaterialExpressionCollectionParameter>(Function))
        {
            if (Parameter->Collection == Collection && Parameter->ParameterName == FName(Name)) return true;
        }
        return false;
    }

    void CheckNames(FAutomationTestBase& Test, UMaterialFunction* Function, std::initializer_list<const TCHAR*> Inputs, std::initializer_list<const TCHAR*> Outputs)
    {
        const TSet<FName> InputNames = FunctionInputNames(Function);
        const TSet<FName> OutputNames = FunctionOutputNames(Function);
        for (const TCHAR* Name : Inputs)
        {
            Test.TestTrue(*FString::Printf(TEXT("%s: вход %s"), *Function->GetName(), Name), InputNames.Contains(FName(Name)));
        }
        for (const TCHAR* Name : Outputs)
        {
            Test.TestTrue(*FString::Printf(TEXT("%s: выход %s"), *Function->GetName(), Name), OutputNames.Contains(FName(Name)));
        }
        Test.TestEqual(*FString::Printf(TEXT("%s: входов ровно столько"), *Function->GetName()), InputNames.Num(), static_cast<int32>(Inputs.size()));
        Test.TestEqual(*FString::Printf(TEXT("%s: выходов ровно столько"), *Function->GetName()), OutputNames.Num(), static_cast<int32>(Outputs.size()));
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistMaterialFunctions_SeasonLayerReadsTimeCollection,
    "Herbalist.MaterialFunctions.SeasonLayerReadsTimeCollection",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistMaterialFunctions_SeasonLayerReadsTimeCollection::RunTest(const FString& Parameters)
{
    using namespace HerbalistMaterialFunctionsTest;
    // Сезон, подкраска и листопад читают готовые значения MPC_WorldStateFields
    // (этап 1б), а не часы: материал не считает календарь сам.
    const FSources Sources = LoadMaterialFunctionSources();
    if (!TestNotNull(TEXT("MPC_WorldStateFields"), Sources.Collection)) return false;

    UMaterialFunction* Weights = NewTransientMaterialFunction();
    if (TestTrue(TEXT("MF_SeasonWeights собрана"), BuildSeasonWeights(Weights, Sources)))
    {
        CheckNames(*this, Weights, {}, { TEXT("SeasonWeights"), TEXT("Spring"), TEXT("Summer"), TEXT("Autumn"), TEXT("Winter"), TEXT("SeasonUDW"), TEXT("LeafDrop01"), TEXT("LeafFall01"), TEXT("LeafLitter01") });
        CheckCommonMaterialFunctionWiring(*this, Weights, nullptr);
        for (const TCHAR* Name : { TEXT("SeasonWeights"), TEXT("SeasonUDW"), TEXT("LeafDrop01"), TEXT("LeafFall01"), TEXT("LeafLitter01") })
        {
            TestTrue(*FString::Printf(TEXT("Веса сезона читают %s"), Name), UsesCollectionParameter(Weights, Sources.Collection, Name));
        }
    }

    UMaterialFunction* Color = NewTransientMaterialFunction();
    if (TestTrue(TEXT("MF_SeasonColor собрана"), BuildSeasonColor(Color, Sources)))
    {
        CheckNames(*this, Color, { TEXT("Color"), TEXT("SpringTint"), TEXT("SummerTint"), TEXT("AutumnTint"), TEXT("WinterTint"), TEXT("Strength") }, { TEXT("Color"), TEXT("Tint") });
        CheckCommonMaterialFunctionWiring(*this, Color, nullptr);
        TestTrue(TEXT("Подкраска читает SeasonWeights"), UsesCollectionParameter(Color, Sources.Collection, TEXT("SeasonWeights")));
        for (UMaterialExpressionFunctionInput* Input : FunctionNodesOfType<UMaterialExpressionFunctionInput>(Color))
        {
            TestTrue(*FString::Printf(TEXT("Вход %s со значением по умолчанию"), *Input->InputName.ToString()), Input->bUsePreviewValueAsDefault && Input->Preview.IsConnected());
        }
    }

    UMaterialFunction* Leaves = NewTransientMaterialFunction();
    if (TestTrue(TEXT("MF_LeafDrop собрана"), BuildLeafDrop(Leaves, Sources)))
    {
        CheckNames(*this, Leaves, { TEXT("OpacityMask"), TEXT("ClumpSize"), TEXT("Position") }, { TEXT("OpacityMask"), TEXT("Kept"), TEXT("LeafDrop01") });
        CheckCommonMaterialFunctionWiring(*this, Leaves, nullptr);
        // Kept = Step(Y = LeafDrop01, X = шум): листва остаётся, где шум не ниже доли опавшего.
        const UMaterialExpressionFunctionOutput* Kept = FindFunctionOutput(Leaves, TEXT("Kept"));
        const UMaterialExpressionStep* Step = Kept ? Cast<UMaterialExpressionStep>(Kept->A.Expression) : nullptr;
        const UMaterialExpressionCollectionParameter* Threshold = Step ? Cast<UMaterialExpressionCollectionParameter>(Step->Y.Expression) : nullptr;
        TestTrue(TEXT("Порог маски -- LeafDrop01"), Threshold && Threshold->ParameterName == FName(TEXT("LeafDrop01")));
        TestTrue(TEXT("Шум подключён"), Step && Step->X.IsConnected());
        TestEqual(TEXT("Сдвиг по экземпляру"), FunctionNodesOfType<UMaterialExpressionPerInstanceRandom>(Leaves).Num(), 1);

        // Хэш: p = frac(cell x 0.1031); p + dot(p, p.yzx + 33.33); frac((x + y) x z).
        const UMaterialExpressionDotProduct* Dot = nullptr;
        for (UMaterialExpressionDotProduct* Found : FunctionNodesOfType<UMaterialExpressionDotProduct>(Leaves)) Dot = Found;
        const UMaterialExpressionFrac* P = Dot ? Cast<UMaterialExpressionFrac>(Dot->A.Expression) : nullptr;
        const UMaterialExpressionAdd* Shift = Dot ? Cast<UMaterialExpressionAdd>(Dot->B.Expression) : nullptr;
        TestTrue(TEXT("Хэш: dot(p, p.yzx + 33.33)"), P && Shift && !Shift->B.IsConnected() && FMath::IsNearlyEqual(Shift->ConstB, 33.33f));
        TestTrue(TEXT("Хэш: сдвиг берёт перестановку yzx, а не p"), Shift && Cast<UMaterialExpressionAppendVector>(Shift->A.Expression));
        bool bMixUsesUnshiftedP = false;
        for (UMaterialExpressionAdd* Add : FunctionNodesOfType<UMaterialExpressionAdd>(Leaves))
        {
            bMixUsesUnshiftedP |= Add->A.Expression == P && Add->B.Expression == Dot;
        }
        TestTrue(TEXT("Хэш: к dot прибавляется исходный p"), P && bMixUsesUnshiftedP);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistMaterialFunctions_GrassSquashTakesMaxOfTrampleWinterSnow,
    "Herbalist.MaterialFunctions.GrassSquashTakesMaxOfTrampleWinterSnow",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistMaterialFunctions_GrassSquashTakesMaxOfTrampleWinterSnow::RunTest(const FString& Parameters)
{
    using namespace HerbalistMaterialFunctionsTest;
    const FSources Sources = LoadMaterialFunctionSources();
    if (!TestNotNull(TEXT("Коллекция Ultra Dynamic Weather"), Sources.WeatherCollection)) return false;
    UMaterialFunction* SampleTrample = NewTransientMaterialFunction();
    if (!TestTrue(TEXT("MF_SampleTrample собрана"), BuildSampleTrample(SampleTrample, Sources))) return false;

    UMaterialFunction* Function = NewTransientMaterialFunction();
    if (!TestTrue(TEXT("MF_GrassSquash собрана"), BuildGrassSquash(Function, Sources, SampleTrample))) return false;

    CheckNames(*this, Function, { TEXT("WPO"), TEXT("WinterStrength"), TEXT("SnowStrength"), TEXT("Snow") }, { TEXT("WPO"), TEXT("Squash"), TEXT("Trample") });
    CheckCommonMaterialFunctionWiring(*this, Function, nullptr);
    TestTrue(TEXT("Снег по умолчанию -- Snowy из коллекции UDW"), UsesCollectionParameter(Function, Sources.WeatherCollection, WeatherSnowParameterName));
    TestTrue(TEXT("Зима -- SeasonWeights"), UsesCollectionParameter(Function, Sources.Collection, TEXT("SeasonWeights")));

    const TArray<UMaterialExpressionStaticSwitchParameter*> Switches = FunctionNodesOfType<UMaterialExpressionStaticSwitchParameter>(Function);
    if (TestEqual(TEXT("Один переключатель тропы"), Switches.Num(), 1))
    {
        TestEqual(TEXT("Тот же Trampleable, что у инстансов"), Switches[0]->ParameterName, FName(TrampleableSwitchName));
        TestFalse(TEXT("По умолчанию выключен"), static_cast<bool>(Switches[0]->DefaultValue));
    }

    // Squash = Saturate(Max(Max(тропа, зима), снег)); WPO -- из сжатия к основанию.
    const UMaterialExpressionFunctionOutput* Squash = FindFunctionOutput(Function, TEXT("Squash"));
    const UMaterialExpressionSaturate* Clamp = Squash ? Cast<UMaterialExpressionSaturate>(Squash->A.Expression) : nullptr;
    const UMaterialExpressionMax* Outer = Clamp ? Cast<UMaterialExpressionMax>(Clamp->Input.Expression) : nullptr;
    const UMaterialExpressionMax* Inner = Outer ? Cast<UMaterialExpressionMax>(Outer->A.Expression) : nullptr;
    TestTrue(TEXT("Сжатие -- максимум трёх причин"), Inner && Cast<UMaterialExpressionStaticSwitchParameter>(Inner->A.Expression) && Inner->B.IsConnected() && Outer->B.IsConnected());
    const UMaterialExpressionFunctionOutput* Wpo = FindFunctionOutput(Function, TEXT("WPO"));
    const UMaterialExpressionSubtract* Result = Wpo ? Cast<UMaterialExpressionSubtract>(Wpo->A.Expression) : nullptr;
    const UMaterialExpressionTransform* Toward = Result ? Cast<UMaterialExpressionTransform>(Result->B.Expression) : nullptr;
    TestTrue(TEXT("WPO -- ветер минус сжатие в мир из Instance & Particle Space"), Toward && Toward->TransformSourceType == TRANSFORMSOURCE_Instance);
    const TArray<UMaterialExpressionLocalPosition*> Locals = FunctionNodesOfType<UMaterialExpressionLocalPosition>(Function);
    TestTrue(TEXT("Локальная позиция без смещений шейдера"), Locals.Num() == 1 && Locals[0]->IncludedOffsets == EPositionIncludedOffsets::ExcludeOffsets);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistMaterialFunctions_FlowerOpenFollowsDayPhaseWindow,
    "Herbalist.MaterialFunctions.FlowerOpenFollowsDayPhaseWindow",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistMaterialFunctions_FlowerOpenFollowsDayPhaseWindow::RunTest(const FString& Parameters)
{
    using namespace HerbalistMaterialFunctionsTest;
    const FSources Sources = LoadMaterialFunctionSources();
    UMaterialFunction* Function = NewTransientMaterialFunction();
    if (!TestTrue(TEXT("MF_FlowerOpen собрана"), BuildFlowerOpen(Function, Sources))) return false;

    CheckNames(*this, Function, { TEXT("OpenPhase"), TEXT("WPO"), TEXT("PetalMask"), TEXT("CloseAmount") }, { TEXT("WPO"), TEXT("Open") });
    CheckCommonMaterialFunctionWiring(*this, Function, nullptr);

    // Open = SmoothStep(r x 0.25, r x 0.25 + 0.5, OpenPhase . DayPhaseWeights) -- окно вида против весов фаз суток.
    const UMaterialExpressionFunctionOutput* Open = FindFunctionOutput(Function, TEXT("Open"));
    const UMaterialExpressionSmoothStep* Smooth = Open ? Cast<UMaterialExpressionSmoothStep>(Open->A.Expression) : nullptr;
    const UMaterialExpressionDotProduct* Dot = Smooth ? Cast<UMaterialExpressionDotProduct>(Smooth->Value.Expression) : nullptr;
    TestTrue(TEXT("Раскрытость -- SmoothStep от скалярного произведения"), Dot != nullptr);
    const UMaterialExpressionFunctionInput* Phase = Dot ? Cast<UMaterialExpressionFunctionInput>(Dot->A.Expression) : nullptr;
    TestTrue(TEXT("Окно вида -- вход OpenPhase, Vector4"), Phase && Phase->InputName == FName(TEXT("OpenPhase")) && Phase->InputType == FunctionInput_Vector4);
    TestTrue(TEXT("Против DayPhaseWeights"), UsesCollectionParameter(Function, Sources.Collection, TEXT("DayPhaseWeights")));
    TestTrue(TEXT("Поляна раскрывается волной -- порог по экземпляру"), Smooth && Smooth->Min.IsConnected() && Smooth->Max.IsConnected()
        && FunctionNodesOfType<UMaterialExpressionPerInstanceRandom>(Function).Num() == 1);
    return true;
}

#endif // WITH_AUTOMATION_TESTS && WITH_EDITOR
