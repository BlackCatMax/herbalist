// Source/ProjectHerbalistTests/Private/Tests/TrampleMaterialWiringTest.cpp
//
// Тропа в мастер-материалах (2026-09-27, -run=MaterialFunctionsSetup -wire):
// трава -- прежний WPO через MF_TrampleCompressWPO, ландшафт -- выборка
// MF_SampleTrample по пикселю вместо сжатия. Те же функции, что у
// коммандлета, на временных материалах; компиляцию настоящих мастеров
// проверяет -wire -verify.

#include "Commandlets/HerbalistMaterialFunctionGraphs.h"

#include "MaterialEditingLibrary.h"
#include "Engine/Texture.h"
#include "Materials/Material.h"
#include "Materials/MaterialFunction.h"
#include "Materials/MaterialParameterCollection.h"
#include "Materials/MaterialExpressionConstant3Vector.h"
#include "Materials/MaterialExpressionFunctionInput.h"
#include "Materials/MaterialExpressionFunctionOutput.h"
#include "Materials/MaterialExpressionLinearInterpolate.h"
#include "Materials/MaterialExpressionMaterialFunctionCall.h"
#include "Misc/AutomationTest.h"

#if WITH_AUTOMATION_TESTS && WITH_EDITOR

namespace HerbalistTrampleWiringTest
{
    using namespace HerbalistMaterialFunctions;

    struct FTrampleFunctions
    {
        UMaterialFunction* Sample = nullptr;
        UMaterialFunction* Compress = nullptr;
    };

    bool BuildTrampleFunctions(FTrampleFunctions& Out)
    {
        FSources Sources;
        Sources.Collection = LoadObject<UMaterialParameterCollection>(nullptr, CollectionPath);
        Sources.TrampleMap = LoadObject<UTexture>(nullptr, TrampleMapPath);
        Out.Sample = NewObject<UMaterialFunction>(GetTransientPackage(), NAME_None, RF_Transient);
        Out.Compress = NewObject<UMaterialFunction>(GetTransientPackage(), NAME_None, RF_Transient);
        return BuildSampleTrample(Out.Sample, Sources) && BuildTrampleCompressWPO(Out.Compress, Out.Sample);
    }

    UMaterial* NewTransientMaterial()
    {
        return NewObject<UMaterial>(GetTransientPackage(), NAME_None, RF_Transient);
    }

    template <typename T>
    T* AddExpression(UMaterial* Material)
    {
        return Cast<T>(UMaterialEditingLibrary::CreateMaterialExpression(Material, T::StaticClass()));
    }

    UMaterialExpressionMaterialFunctionCall* AddCall(UMaterial* Material, UMaterialFunction* Function)
    {
        UMaterialExpressionMaterialFunctionCall* Call = AddExpression<UMaterialExpressionMaterialFunctionCall>(Material);
        return Call && Call->SetMaterialFunction(Function) ? Call : nullptr;
    }

    FExpressionInput* CallInput(UMaterialExpressionMaterialFunctionCall* Call, FName Name)
    {
        for (FFunctionExpressionInput& Input : Call->FunctionInputs)
        {
            if (Input.ExpressionInput && Input.ExpressionInput->InputName == Name) return &Input.Input;
        }
        return nullptr;
    }

    int32 CallOutput(const UMaterialExpressionMaterialFunctionCall* Call, FName Name)
    {
        for (int32 i = 0; i < Call->FunctionOutputs.Num(); ++i)
        {
            if (Call->FunctionOutputs[i].ExpressionOutput && Call->FunctionOutputs[i].ExpressionOutput->OutputName == Name) return i;
        }
        return INDEX_NONE;
    }

    int32 CountCallsTo(const UMaterial* Material, const UMaterialFunction* Function)
    {
        int32 Count = 0;
        for (UMaterialExpression* Expression : Material->GetExpressions())
        {
            const UMaterialExpressionMaterialFunctionCall* Call = Cast<UMaterialExpressionMaterialFunctionCall>(Expression);
            Count += Call && Call->MaterialFunction == Function ? 1 : 0;
        }
        return Count;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistTrampleWiring_GrassWindGoesThroughCompress,
    "Herbalist.MaterialFunctions.Wiring.GrassWindGoesThroughCompress",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistTrampleWiring_GrassWindGoesThroughCompress::RunTest(const FString& Parameters)
{
    using namespace HerbalistTrampleWiringTest;
    FTrampleFunctions Functions;
    if (!TestTrue(TEXT("Функции тропы собраны"), BuildTrampleFunctions(Functions))) return false;

    UMaterial* Material = NewTransientMaterial();
    UMaterialExpressionConstant3Vector* Wind = AddExpression<UMaterialExpressionConstant3Vector>(Material);
    if (!TestNotNull(TEXT("Ветер"), Wind)) return false;
    UMaterialEditingLibrary::ConnectMaterialProperty(Wind, FString(), MP_WorldPositionOffset);

    TestTrue(TEXT("Подключено"), WireTrampleCompressIntoWPO(Material, Functions.Compress) == EWireResult::Wired);
    FExpressionInput* Wpo = Material->GetExpressionInputForProperty(MP_WorldPositionOffset);
    UMaterialExpressionMaterialFunctionCall* Call = Cast<UMaterialExpressionMaterialFunctionCall>(Wpo->Expression);
    if (!TestTrue(TEXT("В WPO -- вызов сжатия"), Call && Call->MaterialFunction == Functions.Compress)) return false;
    TestEqual(TEXT("Из выхода WPO"), Wpo->OutputIndex, CallOutput(Call, TEXT("WPO")));
    const FExpressionInput* WindPin = CallInput(Call, TEXT("WPO"));
    TestTrue(TEXT("Ветер -- на входе WPO сжатия, прежний узел цел"), WindPin && WindPin->Expression == Wind);

    const int32 NodesBefore = Material->GetExpressions().Num();
    TestTrue(TEXT("Повторно -- уже подключено"), WireTrampleCompressIntoWPO(Material, Functions.Compress) == EWireResult::AlreadyWired);
    TestEqual(TEXT("Второго вызова нет"), Material->GetExpressions().Num(), NodesBefore);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistTrampleWiring_ManualCompressElsewhereIsLeftAlone,
    "Herbalist.MaterialFunctions.Wiring.ManualCompressElsewhereIsLeftAlone",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistTrampleWiring_ManualCompressElsewhereIsLeftAlone::RunTest(const FString& Parameters)
{
    // Сжатие уже стоит, но не в WPO -- второй вызов считал бы тропу дважды.
    using namespace HerbalistTrampleWiringTest;
    FTrampleFunctions Functions;
    if (!TestTrue(TEXT("Функции тропы собраны"), BuildTrampleFunctions(Functions))) return false;

    UMaterial* Material = NewTransientMaterial();
    if (!TestNotNull(TEXT("Ручной вызов"), AddCall(Material, Functions.Compress))) return false;
    AddExpectedError(TEXT("не в World Position Offset"), EAutomationExpectedErrorFlags::Contains, 1);
    TestTrue(TEXT("Отказ"), WireTrampleCompressIntoWPO(Material, Functions.Compress) == EWireResult::Failed);
    TestEqual(TEXT("Вызов один"), CountCallsTo(Material, Functions.Compress), 1);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistTrampleWiring_LandscapeSamplesPerPixel,
    "Herbalist.MaterialFunctions.Wiring.LandscapeSamplesPerPixel",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistTrampleWiring_LandscapeSamplesPerPixel::RunTest(const FString& Parameters)
{
    // Как было в M_landscape: Lerp к земле тропы по Trample сжатия, его WPO --
    // в World Position Offset. Сжатие читает тропу в основании экземпляра --
    // у ландшафта это начало компонента.
    using namespace HerbalistTrampleWiringTest;
    FTrampleFunctions Functions;
    if (!TestTrue(TEXT("Функции тропы собраны"), BuildTrampleFunctions(Functions))) return false;

    UMaterial* Material = NewTransientMaterial();
    UMaterialExpressionLinearInterpolate* Lerp = AddExpression<UMaterialExpressionLinearInterpolate>(Material);
    UMaterialExpressionMaterialFunctionCall* Wrong = AddCall(Material, Functions.Compress);
    if (!TestTrue(TEXT("Граф собран"), Lerp && Wrong)) return false;
    Lerp->Alpha.Connect(CallOutput(Wrong, TEXT("Trample")), Wrong);
    UMaterialEditingLibrary::ConnectMaterialProperty(Lerp, FString(), MP_BaseColor);
    UMaterialEditingLibrary::ConnectMaterialProperty(Wrong, TEXT("WPO"), MP_WorldPositionOffset);

    TestTrue(TEXT("Заменено"), ReplaceCompressWithSampleTrample(Material, Functions.Compress, Functions.Sample) == EWireResult::Wired);
    TestEqual(TEXT("Сжатия не осталось"), CountCallsTo(Material, Functions.Compress), 0);
    UMaterialExpressionMaterialFunctionCall* Sample = Cast<UMaterialExpressionMaterialFunctionCall>(Lerp->Alpha.Expression);
    if (!TestTrue(TEXT("Alpha -- из выборки тропы"), Sample && Sample->MaterialFunction == Functions.Sample)) return false;
    TestEqual(TEXT("Выход Trample"), Lerp->Alpha.OutputIndex, CallOutput(Sample, TEXT("Trample")));
    const FExpressionInput* Position = CallInput(Sample, TEXT("Position"));
    TestTrue(TEXT("Position не подключена -- позиция пикселя"), Position && !Position->Expression);
    TestNull(TEXT("WPO ландшафта пуст: на входе сжатия ничего не было"),
        Material->GetExpressionInputForProperty(MP_WorldPositionOffset)->Expression);
    TestEqual(TEXT("Base Color не тронут"), Material->GetExpressionInputForProperty(MP_BaseColor)->Expression,
        static_cast<UMaterialExpression*>(Lerp));

    TestTrue(TEXT("Повторно -- уже так"), ReplaceCompressWithSampleTrample(Material, Functions.Compress, Functions.Sample) == EWireResult::AlreadyWired);
    TestEqual(TEXT("Выборка одна"), CountCallsTo(Material, Functions.Sample), 1);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistTrampleWiring_LandscapeKeepsWhatFedCompressWpo,
    "Herbalist.MaterialFunctions.Wiring.LandscapeKeepsWhatFedCompressWpo",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistTrampleWiring_LandscapeKeepsWhatFedCompressWpo::RunTest(const FString& Parameters)
{
    // Выключенный переключатель отдаёт вход WPO как есть -- то же после замены.
    using namespace HerbalistTrampleWiringTest;
    FTrampleFunctions Functions;
    if (!TestTrue(TEXT("Функции тропы собраны"), BuildTrampleFunctions(Functions))) return false;

    UMaterial* Material = NewTransientMaterial();
    UMaterialExpressionConstant3Vector* Offset = AddExpression<UMaterialExpressionConstant3Vector>(Material);
    UMaterialExpressionMaterialFunctionCall* Wrong = AddCall(Material, Functions.Compress);
    if (!TestTrue(TEXT("Граф собран"), Offset && Wrong)) return false;
    CallInput(Wrong, TEXT("WPO"))->Connect(0, Offset);
    UMaterialEditingLibrary::ConnectMaterialProperty(Wrong, TEXT("WPO"), MP_WorldPositionOffset);

    TestTrue(TEXT("Заменено"), ReplaceCompressWithSampleTrample(Material, Functions.Compress, Functions.Sample) == EWireResult::Wired);
    TestEqual(TEXT("В WPO -- то, что было на входе сжатия"),
        Material->GetExpressionInputForProperty(MP_WorldPositionOffset)->Expression, static_cast<UMaterialExpression*>(Offset));
    return true;
}

#endif
