// Source/ProjectHerbalistTests/Private/Tests/MaterialFunctionsTest.cpp
//
// Функции материалов для карт мира (2026-09-16, HerbalistMaterialFunctionGraphs.h).
// Графы строятся на временных объектах тем же кодом, что у коммандлета, и
// сверяются со схемой бэклога: входы и выходы, цепочки узлов от выходов (каналы,
// шаги маски окна, затухание, сжатие), параметры MPC, явный мип 0 (шейдер
// вершин), Instance & Particle Space у основания, позиция без смещений шейдера,
// переключатель Trampleable, перестройка графа. Компиляцию шейдера проверяет
// -run=MaterialFunctionsSetup -verify (TOOLS_REFERENCE.md) -- автотест в
// редакторском мире без рендера её не видит.

#include "Commandlets/HerbalistMaterialFunctionGraphs.h"

#include "Engine/Texture.h"
#include "Materials/MaterialFunction.h"
#include "Materials/MaterialParameterCollection.h"
#include "Materials/MaterialExpressionCollectionParameter.h"
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
                                       FExpected{ TrampleCompressName, TEXT("WPO") } })
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

#endif // WITH_AUTOMATION_TESTS && WITH_EDITOR
