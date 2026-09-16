// HerbalistMaterialFunctionGraphs.cpp

#include "HerbalistMaterialFunctionGraphs.h"

#include "MaterialEditingLibrary.h"
#include "Engine/Texture.h"
#include "Engine/EngineTypes.h"
#include "Materials/MaterialFunction.h"
#include "Materials/MaterialParameterCollection.h"
#include "Materials/MaterialExpressionCollectionParameter.h"
#include "Materials/MaterialExpressionComponentMask.h"
#include "Materials/MaterialExpressionConstant3Vector.h"
#include "Materials/MaterialExpressionDivide.h"
#include "Materials/MaterialExpressionFrac.h"
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

// Именованное пространство, не анонимное: unity-сборка склеивает файлы модуля.
namespace HerbalistMaterialFunctions::Detail
{
    // Раскладка в редакторе: столбцы слева направо по ходу данных.
    constexpr int32 ColumnStep = 280;
    constexpr int32 RowStep = 150;

    template <typename T>
    T* AddNode(UMaterialFunction* Function, int32 Column, int32 Row)
    {
        return Cast<T>(UMaterialEditingLibrary::CreateMaterialExpressionInFunction(
            Function, T::StaticClass(), Column * ColumnStep, Row * RowStep));
    }

    template <typename T>
    T* AddBinary(UMaterialFunction* Function, UMaterialExpression* A, int32 AOutput, UMaterialExpression* B, int32 BOutput, int32 Column, int32 Row)
    {
        T* Node = AddNode<T>(Function, Column, Row);
        Node->A.Connect(AOutput, A);
        Node->B.Connect(BOutput, B);
        return Node;
    }

    UMaterialExpressionComponentMask* AddMask(UMaterialFunction* Function, UMaterialExpression* From, int32 FromOutput,
        bool bR, bool bG, bool bB, bool bA, int32 Column, int32 Row)
    {
        UMaterialExpressionComponentMask* Mask = AddNode<UMaterialExpressionComponentMask>(Function, Column, Row);
        Mask->Input.Connect(FromOutput, From);
        Mask->R = bR;
        Mask->G = bG;
        Mask->B = bB;
        Mask->A = bA;
        return Mask;
    }

    UMaterialExpressionFunctionInput* AddInput(UMaterialFunction* Function, FName Name, EFunctionInputType Type,
        const FString& Description, int32 SortPriority, int32 Column, int32 Row)
    {
        UMaterialExpressionFunctionInput* Input = AddNode<UMaterialExpressionFunctionInput>(Function, Column, Row);
        Input->InputName = Name;
        Input->InputType = Type;
        Input->Description = Description;
        Input->SortPriority = SortPriority;
        return Input;
    }

    // Вход позиции: не подключён -- абсолютная мировая позиция без смещений
    // шейдера. Без смещений, чтобы функцию можно было звать и из World Position
    // Offset: позиция со смещениями там даёт цикл.
    UMaterialExpressionFunctionInput* AddPositionInput(UMaterialFunction* Function, FName Name, const FString& Description, int32 Column, int32 Row)
    {
        UMaterialExpressionFunctionInput* Input = AddInput(Function, Name, FunctionInput_Vector3, Description, 0, Column, Row);
        UMaterialExpressionWorldPosition* Default = AddNode<UMaterialExpressionWorldPosition>(Function, Column - 1, Row);
        Default->WorldPositionShaderOffset = WPT_ExcludeAllShaderOffsets;
        Input->Preview.Connect(0, Default);
        Input->bUsePreviewValueAsDefault = true;
        return Input;
    }

    void AddOutput(UMaterialFunction* Function, FName Name, const FString& Description, int32 SortPriority,
        UMaterialExpression* From, int32 FromOutput, int32 Column, int32 Row)
    {
        UMaterialExpressionFunctionOutput* Output = AddNode<UMaterialExpressionFunctionOutput>(Function, Column, Row);
        Output->OutputName = Name;
        Output->Description = Description;
        Output->SortPriority = SortPriority;
        Output->A.Connect(FromOutput, From);
    }

    UMaterialExpressionCollectionParameter* AddCollectionParameter(UMaterialFunction* Function, UMaterialParameterCollection* Collection,
        FName ParameterName, int32 Column, int32 Row)
    {
        const FGuid ParameterId = Collection->GetParameterId(ParameterName);
        if (!ParameterId.IsValid())
        {
            UE_LOG(LogTemp, Error, TEXT("В %s нет параметра %s -- сначала -run=WorldStateMapSetup и -run=TrampleMapSetup"),
                *Collection->GetPathName(), *ParameterName.ToString());
            return nullptr;
        }
        UMaterialExpressionCollectionParameter* Parameter = AddNode<UMaterialExpressionCollectionParameter>(Function, Column, Row);
        Parameter->Collection = Collection;
        Parameter->ParameterName = ParameterName;
        Parameter->ParameterId = ParameterId;
        return Parameter;
    }

    // Явный мип 0: в шейдере вершин производных нет, без него выборка не
    // скомпилируется (ловушка из схемы бэклога). Мипов у этих целей нет, в
    // шейдере пикселей это ничего не меняет.
    UMaterialExpressionTextureSample* AddMapSample(UMaterialFunction* Function, UTexture* Texture,
        UMaterialExpression* Coordinates, int32 CoordinatesOutput, int32 Column, int32 Row)
    {
        UMaterialExpressionTextureSample* Sample = AddNode<UMaterialExpressionTextureSample>(Function, Column, Row);
        Sample->Texture = Texture;
        Sample->SamplerType = SAMPLERTYPE_LinearColor;
        Sample->MipValueMode = TMVM_MipLevel;
        Sample->ConstMipValue = 0;
        Sample->Coordinates.Connect(CoordinatesOutput, Coordinates);
        return Sample;
    }

    void DescribeFunction(UMaterialFunction* Function, const FString& Description)
    {
        Function->Description = Description
            + TEXT("\n\nСобрано -run=MaterialFunctionsSetup; повторный запуск с -rebuild перестроит граф.");
        Function->bExposeToLibrary = true;
        Function->LibraryCategoriesText = { FText::FromString(TEXT("Herbalist")) };
    }

    // Выходы TextureSample: 0 RGB, 1 R, 2 G, 3 B, 4 A.
    constexpr int32 SampleR = 1;
    constexpr int32 SampleG = 2;
    constexpr int32 SampleB = 3;
    constexpr int32 SampleA = 4;
}

bool HerbalistMaterialFunctions::BuildSampleWorldState(UMaterialFunction* Function, const FSources& Sources)
{
    using namespace Detail;
    if (!Function || !Sources.Collection || !Sources.WorldStateMap) return false;

    DescribeFunction(Function, TEXT("Состояние клетки из RT_WorldStateMap по мировой позиции. UV = (Pos.XY - WorldStateMapOrigin.XY) / WorldStateMapSize.XY. За окном карты оси -- крайние тексели, InsideWindow = 0."));

    UMaterialExpressionFunctionInput* Position = AddPositionInput(Function, TEXT("WorldPosition"),
        TEXT("Абсолютная мировая позиция. Не подключена -- позиция пикселя или вершины без смещений шейдера."), 1, 0);
    UMaterialExpressionCollectionParameter* Origin = AddCollectionParameter(Function, Sources.Collection, TEXT("WorldStateMapOrigin"), 1, 2);
    UMaterialExpressionCollectionParameter* Size = AddCollectionParameter(Function, Sources.Collection, TEXT("WorldStateMapSize"), 1, 3);
    if (!Origin || !Size) return false;

    UMaterialExpressionComponentMask* PositionXY = AddMask(Function, Position, 0, true, true, false, false, 2, 0);
    UMaterialExpressionComponentMask* OriginXY = AddMask(Function, Origin, 0, true, true, false, false, 2, 2);
    UMaterialExpressionComponentMask* SizeXY = AddMask(Function, Size, 0, true, true, false, false, 2, 3);
    UMaterialExpressionSubtract* Local = AddBinary<UMaterialExpressionSubtract>(Function, PositionXY, 0, OriginXY, 0, 3, 0);
    UMaterialExpressionDivide* UV = AddBinary<UMaterialExpressionDivide>(Function, Local, 0, SizeXY, 0, 4, 0);

    UMaterialExpressionTextureSample* Sample = AddMapSample(Function, Sources.WorldStateMap, UV, 0, 5, 0);

    // Внутри окна: 0 <= U <= 1 и 0 <= V <= 1. Step(Y, X) = X >= Y.
    UMaterialExpressionComponentMask* U = AddMask(Function, UV, 0, true, false, false, false, 5, 3);
    UMaterialExpressionComponentMask* V = AddMask(Function, UV, 0, false, true, false, false, 5, 4);
    auto AddStepAbove = [Function](UMaterialExpression* Value, int32 Row)
    {
        UMaterialExpressionStep* Step = AddNode<UMaterialExpressionStep>(Function, 6, Row);
        Step->ConstY = 0.0f;
        Step->X.Connect(0, Value);
        return Step;
    };
    auto AddStepBelow = [Function](UMaterialExpression* Value, int32 Row)
    {
        UMaterialExpressionStep* Step = AddNode<UMaterialExpressionStep>(Function, 6, Row);
        Step->Y.Connect(0, Value);
        Step->ConstX = 1.0f;
        return Step;
    };
    UMaterialExpressionMultiply* InsideU = AddBinary<UMaterialExpressionMultiply>(Function, AddStepAbove(U, 3), 0, AddStepBelow(U, 4), 0, 7, 3);
    UMaterialExpressionMultiply* InsideV = AddBinary<UMaterialExpressionMultiply>(Function, AddStepAbove(V, 5), 0, AddStepBelow(V, 6), 0, 7, 5);
    UMaterialExpressionMultiply* Inside = AddBinary<UMaterialExpressionMultiply>(Function, InsideU, 0, InsideV, 0, 8, 4);

    AddOutput(Function, TEXT("Distortion"), TEXT("Искажение клетки, канал R."), 0, Sample, SampleR, 9, 0);
    AddOutput(Function, TEXT("Corruption"), TEXT("Порча клетки, канал G."), 1, Sample, SampleG, 9, 1);
    AddOutput(Function, TEXT("HarvestStress"), TEXT("Вытоптанность сборами, канал B."), 2, Sample, SampleB, 9, 2);
    AddOutput(Function, TEXT("ShrineInfluence"), TEXT("Влияние капища, канал A."), 3, Sample, SampleA, 9, 3);
    AddOutput(Function, TEXT("UV"), TEXT("Координаты в окне карты."), 4, UV, 0, 9, 4);
    AddOutput(Function, TEXT("InsideWindow"), TEXT("1 внутри окна карты, 0 за ним -- там оси недостоверны."), 5, Inside, 0, 9, 5);
    return true;
}

bool HerbalistMaterialFunctions::BuildSampleTrample(UMaterialFunction* Function, const FSources& Sources)
{
    using namespace Detail;
    if (!Function || !Sources.Collection || !Sources.TrampleMap) return false;

    DescribeFunction(Function, TEXT("Тропа из RT_TrampleMap: frac(Pos / TrampleMapFrame.R), затухание к краю окна SmoothStep(TrampleMapFrame.G, TrampleMapFrame.B, расстояние до игрока). Для травы -- позиция основания (Instance & Particle Space), см. MF_TrampleCompressWPO."));

    UMaterialExpressionFunctionInput* Position = AddPositionInput(Function, TEXT("Position"),
        TEXT("Абсолютная мировая позиция, где читать тропу. Не подключена -- позиция пикселя или вершины без смещений шейдера."), 1, 0);
    UMaterialExpressionCollectionParameter* Frame = AddCollectionParameter(Function, Sources.Collection, TEXT("TrampleMapFrame"), 1, 2);
    UMaterialExpressionCollectionParameter* Player = AddCollectionParameter(Function, Sources.Collection, TEXT("TramplePlayerPosition"), 1, 4);
    if (!Frame || !Player) return false;

    UMaterialExpressionComponentMask* WindowSize = AddMask(Function, Frame, 0, true, false, false, false, 2, 1);
    UMaterialExpressionComponentMask* FadeStart = AddMask(Function, Frame, 0, false, true, false, false, 2, 2);
    UMaterialExpressionComponentMask* FadeEnd = AddMask(Function, Frame, 0, false, false, true, false, 2, 3);

    UMaterialExpressionDivide* Scaled = AddBinary<UMaterialExpressionDivide>(Function, Position, 0, WindowSize, 0, 3, 0);
    UMaterialExpressionFrac* Wrapped = AddNode<UMaterialExpressionFrac>(Function, 4, 0);
    Wrapped->Input.Connect(0, Scaled);
    UMaterialExpressionComponentMask* UV = AddMask(Function, Wrapped, 0, true, true, false, false, 5, 0);
    UMaterialExpressionTextureSample* Sample = AddMapSample(Function, Sources.TrampleMap, UV, 0, 6, 0);

    UMaterialExpressionComponentMask* PlayerXYZ = AddMask(Function, Player, 0, true, true, true, false, 2, 4);
    UMaterialExpressionSubtract* ToPlayer = AddBinary<UMaterialExpressionSubtract>(Function, Position, 0, PlayerXYZ, 0, 3, 4);
    UMaterialExpressionComponentMask* ToPlayerXY = AddMask(Function, ToPlayer, 0, true, true, false, false, 4, 4);
    UMaterialExpressionLength* Distance = AddNode<UMaterialExpressionLength>(Function, 5, 4);
    Distance->Input.Connect(0, ToPlayerXY);
    UMaterialExpressionSmoothStep* Edge = AddNode<UMaterialExpressionSmoothStep>(Function, 6, 3);
    Edge->Min.Connect(0, FadeStart);
    Edge->Max.Connect(0, FadeEnd);
    Edge->Value.Connect(0, Distance);
    UMaterialExpressionOneMinus* Fade = AddNode<UMaterialExpressionOneMinus>(Function, 7, 3);
    Fade->Input.Connect(0, Edge);

    UMaterialExpressionMultiply* Trample = AddBinary<UMaterialExpressionMultiply>(Function, Sample, SampleR, Fade, 0, 8, 0);

    AddOutput(Function, TEXT("Trample"), TEXT("Вытоптанность 0..1 с затуханием к краю окна."), 0, Trample, 0, 9, 0);
    AddOutput(Function, TEXT("RawTrample"), TEXT("Значение карты без затухания."), 1, Sample, SampleR, 9, 1);
    AddOutput(Function, TEXT("Fade"), TEXT("1 у игрока, 0 у края окна."), 2, Fade, 0, 9, 3);
    return true;
}

bool HerbalistMaterialFunctions::BuildTrampleCompressWPO(UMaterialFunction* Function, UMaterialFunction* SampleTrample)
{
    using namespace Detail;
    if (!Function || !SampleTrample) return false;

    DescribeFunction(Function, TEXT("Трава на тропе прижимается к основанию: вершины сдвигаются к точке основания на долю Trample, ветер слабеет так же. Переключатель Trampleable (по умолчанию выключен) включается в инстансах низкого покрова. Выход WPO -- в World Position Offset вместо ветра."));

    UMaterialExpressionFunctionInput* Wind = AddInput(Function, TEXT("WPO"), FunctionInput_Vector3,
        TEXT("То, что сейчас подключено к World Position Offset (ветер). Не подключено -- ноль."), 0, 1, 0);
    UMaterialExpressionConstant3Vector* NoWind = AddNode<UMaterialExpressionConstant3Vector>(Function, 0, 0);
    NoWind->Constant = FLinearColor(0.0f, 0.0f, 0.0f);
    Wind->Preview.Connect(0, NoWind);
    Wind->bUsePreviewValueAsDefault = true;

    // Основание кустика. Только Instance & Particle Space: PCG-трава -- Nanite,
    // и Local Space там означает весь PCG-компонент (трава сжималась бы к его центру).
    UMaterialExpressionConstant3Vector* Origin = AddNode<UMaterialExpressionConstant3Vector>(Function, 0, 3);
    Origin->Constant = FLinearColor(0.0f, 0.0f, 0.0f);
    UMaterialExpressionTransformPosition* Pivot = AddNode<UMaterialExpressionTransformPosition>(Function, 1, 3);
    Pivot->TransformSourceType = TRANSFORMPOSSOURCE_Instance;
    Pivot->TransformType = TRANSFORMPOSSOURCE_World;
    Pivot->Input.Connect(0, Origin);

    UMaterialExpressionMaterialFunctionCall* SampleCall = AddNode<UMaterialExpressionMaterialFunctionCall>(Function, 2, 3);
    SampleCall->SetMaterialFunction(SampleTrample);
    FExpressionInput* PositionPin = nullptr;
    for (FFunctionExpressionInput& CallInput : SampleCall->FunctionInputs)
    {
        if (CallInput.ExpressionInput && CallInput.ExpressionInput->InputName == FName(TEXT("Position")))
        {
            PositionPin = &CallInput.Input;
        }
    }
    int32 TrampleOutput = INDEX_NONE;
    for (int32 i = 0; i < SampleCall->FunctionOutputs.Num(); ++i)
    {
        const UMaterialExpressionFunctionOutput* CallOutput = SampleCall->FunctionOutputs[i].ExpressionOutput;
        if (CallOutput && CallOutput->OutputName == FName(TEXT("Trample")))
        {
            TrampleOutput = i;
        }
    }
    if (!PositionPin || TrampleOutput == INDEX_NONE)
    {
        UE_LOG(LogTemp, Error, TEXT("%s: у %s нет входа Position или выхода Trample"), *Function->GetName(), *SampleTrample->GetName());
        return false;
    }
    PositionPin->Connect(0, Pivot);

    // Сжатие: локальная позиция вершины без смещений шейдера (иначе позиция
    // зависит от самого WPO -- цикл), доля Trample, обратно в мир.
    UMaterialExpressionLocalPosition* Local = AddNode<UMaterialExpressionLocalPosition>(Function, 2, 5);
    Local->LocalOrigin = ELocalPositionOrigin::Instance;
    Local->IncludedOffsets = EPositionIncludedOffsets::ExcludeOffsets;
    UMaterialExpressionMultiply* Squash = AddBinary<UMaterialExpressionMultiply>(Function, Local, 0, SampleCall, TrampleOutput, 3, 5);
    UMaterialExpressionTransform* SquashWorld = AddNode<UMaterialExpressionTransform>(Function, 4, 5);
    SquashWorld->TransformSourceType = TRANSFORMSOURCE_Instance;
    SquashWorld->TransformType = TRANSFORM_World;
    SquashWorld->Input.Connect(0, Squash);

    UMaterialExpressionOneMinus* Standing = AddNode<UMaterialExpressionOneMinus>(Function, 3, 1);
    Standing->Input.Connect(TrampleOutput, SampleCall);
    UMaterialExpressionMultiply* WeakWind = AddBinary<UMaterialExpressionMultiply>(Function, Wind, 0, Standing, 0, 4, 0);
    UMaterialExpressionSubtract* Trampled = AddBinary<UMaterialExpressionSubtract>(Function, WeakWind, 0, SquashWorld, 0, 5, 2);

    UMaterialExpressionStaticSwitchParameter* Switch = AddNode<UMaterialExpressionStaticSwitchParameter>(Function, 6, 1);
    Switch->ParameterName = TrampleableSwitchName;
    Switch->DefaultValue = false;
    Switch->A.Connect(0, Trampled);   // True
    Switch->B.Connect(0, Wind);       // False
    Switch->UpdateParameterGuid(true, true);

    AddOutput(Function, TEXT("WPO"), TEXT("В World Position Offset."), 0, Switch, 0, 7, 1);
    AddOutput(Function, TEXT("Trample"), TEXT("Вытоптанность у основания кустика, 0..1."), 1, SampleCall, TrampleOutput, 7, 3);
    return true;
}

bool HerbalistMaterialFunctions::ClearMaterialFunction(UMaterialFunction* Function)
{
    if (!Function) return false;

    // Копия списка: удаление меняет сам массив узлов функции.
    TArray<UMaterialExpression*> Existing;
    for (UMaterialExpression* Expression : Function->GetExpressions())
    {
        Existing.Add(Expression);
    }
    for (UMaterialExpression* Expression : Existing)
    {
        UMaterialEditingLibrary::DeleteMaterialExpressionInFunction(Function, Expression);
    }
    return Function->GetExpressions().Num() == 0;
}

HerbalistMaterialFunctions::FFunctionPinIds HerbalistMaterialFunctions::CaptureFunctionPinIds(const UMaterialFunction* Function)
{
    FFunctionPinIds PinIds;
    if (!Function) return PinIds;
    for (const UMaterialExpression* Expression : Function->GetExpressions())
    {
        if (const UMaterialExpressionFunctionInput* Input = Cast<UMaterialExpressionFunctionInput>(Expression))
        {
            PinIds.Inputs.Add(Input->InputName, Input->Id);
        }
        else if (const UMaterialExpressionFunctionOutput* Output = Cast<UMaterialExpressionFunctionOutput>(Expression))
        {
            PinIds.Outputs.Add(Output->OutputName, Output->Id);
        }
    }
    return PinIds;
}

void HerbalistMaterialFunctions::RestoreFunctionPinIds(UMaterialFunction* Function, const FFunctionPinIds& PinIds)
{
    if (!Function) return;
    for (UMaterialExpression* Expression : Function->GetExpressions())
    {
        if (UMaterialExpressionFunctionInput* Input = Cast<UMaterialExpressionFunctionInput>(Expression))
        {
            if (const FGuid* Id = PinIds.Inputs.Find(Input->InputName)) Input->Id = *Id;
        }
        else if (UMaterialExpressionFunctionOutput* Output = Cast<UMaterialExpressionFunctionOutput>(Expression))
        {
            if (const FGuid* Id = PinIds.Outputs.Find(Output->OutputName)) Output->Id = *Id;
        }
    }
}
