// HerbalistMaterialFunctionGraphs.cpp

#include "HerbalistMaterialFunctionGraphs.h"

#include "MaterialEditingLibrary.h"
#include "Engine/Texture.h"
#include "Engine/EngineTypes.h"
#include "Materials/MaterialFunction.h"
#include "Materials/MaterialParameterCollection.h"
#include "Materials/MaterialExpressionAdd.h"
#include "Materials/MaterialExpressionAppendVector.h"
#include "Materials/MaterialExpressionCollectionParameter.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionConstant4Vector.h"
#include "Materials/MaterialExpressionDotProduct.h"
#include "Materials/MaterialExpressionFloor.h"
#include "Materials/MaterialExpressionLinearInterpolate.h"
#include "Materials/MaterialExpressionMax.h"
#include "Materials/MaterialExpressionPerInstanceRandom.h"
#include "Materials/MaterialExpressionSaturate.h"
#include "Materials/MaterialExpressionVertexColor.h"
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
            UE_LOG(LogTemp, Error, TEXT("В %s нет параметра %s -- сначала -run=WorldStateMapSetup, -run=TrampleMapSetup и -run=TimeDisplaySetup"),
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

    UMaterialExpressionConstant* AddConstant(UMaterialFunction* Function, float Value, int32 Column, int32 Row)
    {
        UMaterialExpressionConstant* Constant = AddNode<UMaterialExpressionConstant>(Function, Column, Row);
        Constant->R = Value;
        return Constant;
    }

    // Вход с числом по умолчанию.
    UMaterialExpressionFunctionInput* AddScalarInput(UMaterialFunction* Function, FName Name, float DefaultValue,
        const FString& Description, int32 SortPriority, int32 Column, int32 Row)
    {
        UMaterialExpressionFunctionInput* Input = AddInput(Function, Name, FunctionInput_Scalar, Description, SortPriority, Column, Row);
        Input->Preview.Connect(0, AddConstant(Function, DefaultValue, Column - 1, Row));
        Input->bUsePreviewValueAsDefault = true;
        return Input;
    }

    UMaterialExpressionFunctionInput* AddVector3Input(UMaterialFunction* Function, FName Name, const FLinearColor& DefaultValue,
        const FString& Description, int32 SortPriority, int32 Column, int32 Row)
    {
        UMaterialExpressionFunctionInput* Input = AddInput(Function, Name, FunctionInput_Vector3, Description, SortPriority, Column, Row);
        UMaterialExpressionConstant3Vector* Default = AddNode<UMaterialExpressionConstant3Vector>(Function, Column - 1, Row);
        Default->Constant = DefaultValue;
        Input->Preview.Connect(0, Default);
        Input->bUsePreviewValueAsDefault = true;
        return Input;
    }

    // Сжатие к основанию экземпляра (как у MF_TrampleCompressWPO): ветер x
    // (1 - Amount) - Transform(Instance -> World, LocalPosition x Amount).
    // LocalPosition -- без смещений шейдера, иначе цикл через WPO.
    UMaterialExpression* AddSquashTowardPivot(UMaterialFunction* Function, UMaterialExpression* Wind, UMaterialExpression* Amount,
        bool bHorizontalOnly, int32 Column, int32 Row)
    {
        UMaterialExpressionLocalPosition* Local = AddNode<UMaterialExpressionLocalPosition>(Function, Column, Row + 2);
        Local->LocalOrigin = ELocalPositionOrigin::Instance;
        Local->IncludedOffsets = EPositionIncludedOffsets::ExcludeOffsets;
        UMaterialExpression* Offset = Local;
        if (bHorizontalOnly)
        {
            UMaterialExpressionComponentMask* XY = AddMask(Function, Local, 0, true, true, false, false, Column + 1, Row + 2);
            UMaterialExpressionAppendVector* Flat = AddNode<UMaterialExpressionAppendVector>(Function, Column + 2, Row + 2);
            Flat->A.Connect(0, XY);
            Flat->B.Connect(0, AddConstant(Function, 0.0f, Column + 1, Row + 3));
            Offset = Flat;
        }
        UMaterialExpressionMultiply* Squash = AddBinary<UMaterialExpressionMultiply>(Function, Offset, 0, Amount, 0, Column + 3, Row + 2);
        UMaterialExpressionTransform* SquashWorld = AddNode<UMaterialExpressionTransform>(Function, Column + 4, Row + 2);
        SquashWorld->TransformSourceType = TRANSFORMSOURCE_Instance;
        SquashWorld->TransformType = TRANSFORM_World;
        SquashWorld->Input.Connect(0, Squash);

        UMaterialExpressionOneMinus* Standing = AddNode<UMaterialExpressionOneMinus>(Function, Column + 3, Row);
        Standing->Input.Connect(0, Amount);
        UMaterialExpressionMultiply* WeakWind = AddBinary<UMaterialExpressionMultiply>(Function, Wind, 0, Standing, 0, Column + 4, Row);
        return AddBinary<UMaterialExpressionSubtract>(Function, WeakWind, 0, SquashWorld, 0, Column + 5, Row + 1);
    }

    // Вход ветра WPO: не подключён -- ноль.
    UMaterialExpressionFunctionInput* AddWindInput(UMaterialFunction* Function, int32 SortPriority, int32 Column, int32 Row)
    {
        return AddVector3Input(Function, TEXT("WPO"), FLinearColor(0.0f, 0.0f, 0.0f),
            TEXT("То, что сейчас подключено к World Position Offset (ветер). Не подключено -- ноль."), SortPriority, Column, Row);
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

bool HerbalistMaterialFunctions::BuildSeasonWeights(UMaterialFunction* Function, const FSources& Sources)
{
    using namespace Detail;
    if (!Function || !Sources.Collection) return false;

    DescribeFunction(Function, TEXT("Сезон из MPC_WorldStateFields (пишет менеджер сетки, Core/Types/HerbalistTimeDisplay.h): веса весна/лето/осень/зима в сумме 1, SeasonUDW -- шкала Ultra Dynamic Sky 0..4 (целое -- середина сезона), LeafDrop01 -- доля опавшей листвы."));

    UMaterialExpressionCollectionParameter* Weights = AddCollectionParameter(Function, Sources.Collection, TEXT("SeasonWeights"), 1, 0);
    UMaterialExpressionCollectionParameter* Udw = AddCollectionParameter(Function, Sources.Collection, TEXT("SeasonUDW"), 1, 5);
    UMaterialExpressionCollectionParameter* LeafDrop = AddCollectionParameter(Function, Sources.Collection, TEXT("LeafDrop01"), 1, 6);
    if (!Weights || !Udw || !LeafDrop) return false;

    UMaterialExpressionComponentMask* All = AddMask(Function, Weights, 0, true, true, true, true, 2, 0);
    AddOutput(Function, TEXT("SeasonWeights"), TEXT("R весна, G лето, B осень, A зима; сумма 1."), 0, All, 0, 3, 0);
    AddOutput(Function, TEXT("Spring"), TEXT("Вес весны."), 1, AddMask(Function, Weights, 0, true, false, false, false, 2, 1), 0, 3, 1);
    AddOutput(Function, TEXT("Summer"), TEXT("Вес лета."), 2, AddMask(Function, Weights, 0, false, true, false, false, 2, 2), 0, 3, 2);
    AddOutput(Function, TEXT("Autumn"), TEXT("Вес осени."), 3, AddMask(Function, Weights, 0, false, false, true, false, 2, 3), 0, 3, 3);
    AddOutput(Function, TEXT("Winter"), TEXT("Вес зимы."), 4, AddMask(Function, Weights, 0, false, false, false, true, 2, 4), 0, 3, 4);
    AddOutput(Function, TEXT("SeasonUDW"), TEXT("Сезон 0..4 в шкале UDS: 0 середина весны, 1 лета, 2 осени, 3 зимы."), 5, Udw, 0, 3, 5);
    AddOutput(Function, TEXT("LeafDrop01"), TEXT("0 с середины весны до конца лета, 1 в середине зимы."), 6, LeafDrop, 0, 3, 6);
    return true;
}

bool HerbalistMaterialFunctions::BuildSeasonColor(UMaterialFunction* Function, const FSources& Sources)
{
    using namespace Detail;
    if (!Function || !Sources.Collection) return false;

    DescribeFunction(Function, TEXT("Цвет, подкрашенный по сезону: Color x (веса сезонов . оттенки), смешано с исходным по Strength. Оттенки -- множители цвета; по умолчанию весна свежее, лето как есть, осень желтеет, зима пожухлая. Подбирать в инстансах."));

    UMaterialExpressionFunctionInput* Color = AddVector3Input(Function, TEXT("Color"), FLinearColor(1.0f, 1.0f, 1.0f),
        TEXT("Базовый цвет (то, что сейчас идёт в Base Color)."), 0, 1, 0);
    UMaterialExpressionFunctionInput* Spring = AddVector3Input(Function, TEXT("SpringTint"), FLinearColor(0.95f, 1.08f, 0.9f),
        TEXT("Множитель цвета весной."), 1, 1, 1);
    UMaterialExpressionFunctionInput* Summer = AddVector3Input(Function, TEXT("SummerTint"), FLinearColor(1.0f, 1.0f, 1.0f),
        TEXT("Множитель цвета летом."), 2, 1, 2);
    UMaterialExpressionFunctionInput* Autumn = AddVector3Input(Function, TEXT("AutumnTint"), FLinearColor(1.25f, 0.95f, 0.45f),
        TEXT("Множитель цвета осенью."), 3, 1, 3);
    UMaterialExpressionFunctionInput* Winter = AddVector3Input(Function, TEXT("WinterTint"), FLinearColor(0.85f, 0.8f, 0.7f),
        TEXT("Множитель цвета зимой."), 4, 1, 4);
    UMaterialExpressionFunctionInput* Strength = AddScalarInput(Function, TEXT("Strength"), 1.0f,
        TEXT("0 -- цвет как есть, 1 -- полностью по сезону."), 5, 1, 5);
    UMaterialExpressionCollectionParameter* Weights = AddCollectionParameter(Function, Sources.Collection, TEXT("SeasonWeights"), 1, 6);
    if (!Weights) return false;

    UMaterialExpression* Tints[4] = { Spring, Summer, Autumn, Winter };
    UMaterialExpression* Sum = nullptr;
    for (int32 Index = 0; Index < 4; ++Index)
    {
        UMaterialExpressionComponentMask* Weight = AddMask(Function, Weights, 0, Index == 0, Index == 1, Index == 2, Index == 3, 2, 6 + Index);
        UMaterialExpressionMultiply* Weighted = AddBinary<UMaterialExpressionMultiply>(Function, Tints[Index], 0, Weight, 0, 3, 1 + Index);
        Sum = Sum ? AddBinary<UMaterialExpressionAdd>(Function, Sum, 0, Weighted, 0, 4, 1 + Index) : static_cast<UMaterialExpression*>(Weighted);
    }
    UMaterialExpressionMultiply* Tinted = AddBinary<UMaterialExpressionMultiply>(Function, Color, 0, Sum, 0, 5, 1);
    UMaterialExpressionLinearInterpolate* Result = AddNode<UMaterialExpressionLinearInterpolate>(Function, 6, 0);
    Result->A.Connect(0, Color);
    Result->B.Connect(0, Tinted);
    Result->Alpha.Connect(0, Strength);

    AddOutput(Function, TEXT("Color"), TEXT("В Base Color."), 0, Result, 0, 7, 0);
    AddOutput(Function, TEXT("Tint"), TEXT("Итоговый множитель сезона."), 1, Sum, 0, 7, 2);
    return true;
}

bool HerbalistMaterialFunctions::BuildLeafDrop(UMaterialFunction* Function, const FSources& Sources)
{
    using namespace Detail;
    if (!Function || !Sources.Collection) return false;

    DescribeFunction(Function, TEXT("Листопад маской: листва остаётся, где шум кучки >= LeafDrop01. Шум -- хэш клетки мировой позиции размером ClumpSize (листья опадают кучками, не пикселями), на 20% сдвинут по экземпляру (деревья облетают не разом). Для маскированной листвы (r.Nanite.Foliage выключен; на Nanite-меше маска -- дорогой программируемый растеризатор). Выход OpacityMask -- в Opacity Mask."));

    UMaterialExpressionFunctionInput* Mask = AddScalarInput(Function, TEXT("OpacityMask"), 1.0f,
        TEXT("То, что сейчас идёт в Opacity Mask. Не подключено -- 1."), 0, 1, 0);
    UMaterialExpressionFunctionInput* ClumpSize = AddScalarInput(Function, TEXT("ClumpSize"), 30.0f,
        TEXT("Размер кучки листьев, опадающей разом, см."), 1, 1, 1);
    UMaterialExpressionFunctionInput* Position = AddPositionInput(Function, TEXT("Position"),
        TEXT("Абсолютная мировая позиция. Не подключена -- позиция пикселя без смещений шейдера (ветер не мерцает листопадом)."), 1, 3);
    Position->SortPriority = 2;
    UMaterialExpressionCollectionParameter* LeafDrop = AddCollectionParameter(Function, Sources.Collection, TEXT("LeafDrop01"), 1, 6);
    if (!LeafDrop) return false;

    // Хэш 3D -> 1D без sin (устойчив к большим координатам): p = frac(cell x
    // 0.1031); p += dot(p, p.yzx + 33.33); h = frac((p.x + p.y) x p.z).
    UMaterialExpressionDivide* Scaled = AddBinary<UMaterialExpressionDivide>(Function, Position, 0, ClumpSize, 0, 2, 2);
    UMaterialExpressionFloor* Cell = AddNode<UMaterialExpressionFloor>(Function, 3, 2);
    Cell->Input.Connect(0, Scaled);
    UMaterialExpressionMultiply* CellScaled = AddNode<UMaterialExpressionMultiply>(Function, 4, 2);
    CellScaled->A.Connect(0, Cell);
    CellScaled->ConstB = 0.1031f;
    UMaterialExpressionFrac* P = AddNode<UMaterialExpressionFrac>(Function, 5, 2);
    P->Input.Connect(0, CellScaled);
    UMaterialExpressionComponentMask* PX = AddMask(Function, P, 0, true, false, false, false, 6, 3);
    UMaterialExpressionComponentMask* PY = AddMask(Function, P, 0, false, true, false, false, 6, 4);
    UMaterialExpressionComponentMask* PZ = AddMask(Function, P, 0, false, false, true, false, 6, 5);
    UMaterialExpressionAppendVector* YZ = AddBinary<UMaterialExpressionAppendVector>(Function, PY, 0, PZ, 0, 7, 4);
    UMaterialExpressionAppendVector* YZX = AddBinary<UMaterialExpressionAppendVector>(Function, YZ, 0, PX, 0, 8, 4);
    UMaterialExpressionAdd* Shifted = AddNode<UMaterialExpressionAdd>(Function, 9, 4);
    Shifted->A.Connect(0, YZX);
    Shifted->ConstB = 33.33f;
    UMaterialExpressionDotProduct* Dot = AddBinary<UMaterialExpressionDotProduct>(Function, P, 0, Shifted, 0, 10, 3);
    UMaterialExpressionAdd* Mixed = AddBinary<UMaterialExpressionAdd>(Function, P, 0, Dot, 0, 11, 2);
    UMaterialExpressionComponentMask* MX = AddMask(Function, Mixed, 0, true, false, false, false, 12, 2);
    UMaterialExpressionComponentMask* MY = AddMask(Function, Mixed, 0, false, true, false, false, 12, 3);
    UMaterialExpressionComponentMask* MZ = AddMask(Function, Mixed, 0, false, false, true, false, 12, 4);
    UMaterialExpressionAdd* XY = AddBinary<UMaterialExpressionAdd>(Function, MX, 0, MY, 0, 13, 2);
    UMaterialExpressionMultiply* XYZ = AddBinary<UMaterialExpressionMultiply>(Function, XY, 0, MZ, 0, 14, 3);
    UMaterialExpressionFrac* Hash = AddNode<UMaterialExpressionFrac>(Function, 15, 3);
    Hash->Input.Connect(0, XYZ);

    UMaterialExpressionPerInstanceRandom* InstanceRandom = AddNode<UMaterialExpressionPerInstanceRandom>(Function, 15, 5);
    UMaterialExpressionLinearInterpolate* Noise = AddNode<UMaterialExpressionLinearInterpolate>(Function, 16, 4);
    Noise->A.Connect(0, Hash);
    Noise->B.Connect(0, InstanceRandom);
    Noise->ConstAlpha = 0.2f;

    // Step(Y, X) = X >= Y: остаётся, где шум не ниже доли опавшего.
    UMaterialExpressionStep* Kept = AddNode<UMaterialExpressionStep>(Function, 17, 5);
    Kept->X.Connect(0, Noise);
    Kept->Y.Connect(0, LeafDrop);
    UMaterialExpressionMultiply* Result = AddBinary<UMaterialExpressionMultiply>(Function, Mask, 0, Kept, 0, 18, 0);

    AddOutput(Function, TEXT("OpacityMask"), TEXT("В Opacity Mask."), 0, Result, 0, 19, 0);
    AddOutput(Function, TEXT("Kept"), TEXT("1 -- листва на месте, 0 -- опала."), 1, Kept, 0, 19, 2);
    AddOutput(Function, TEXT("LeafDrop01"), TEXT("Доля опавшей листвы сезона."), 2, LeafDrop, 0, 19, 4);
    return true;
}

bool HerbalistMaterialFunctions::BuildGrassSquash(UMaterialFunction* Function, const FSources& Sources, UMaterialFunction* SampleTrample)
{
    using namespace Detail;
    if (!Function || !Sources.Collection || !Sources.WeatherCollection || !SampleTrample) return false;

    DescribeFunction(Function, TEXT("Трава ложится к основанию (§3.1 плана): Squash = max(тропа, зима, снег). Зима -- вес зимы x WinterStrength со сдвигом порога по экземпляру (трава жухнет не разом); снег -- покрытие Snowy коллекции Ultra Dynamic Weather x SnowStrength; тропа -- за переключателем Trampleable, как у MF_TrampleCompressWPO. Выход WPO -- в World Position Offset вместо ветра."));

    UMaterialExpressionFunctionInput* Wind = AddWindInput(Function, 0, 1, 0);
    UMaterialExpressionFunctionInput* WinterStrength = AddScalarInput(Function, TEXT("WinterStrength"), 0.8f,
        TEXT("Насколько трава ложится в середине зимы, 0..1."), 1, 1, 1);
    UMaterialExpressionFunctionInput* SnowStrength = AddScalarInput(Function, TEXT("SnowStrength"), 1.0f,
        TEXT("Насколько трава ложится под полным снегом, 0..1."), 2, 1, 2);
    UMaterialExpressionFunctionInput* Snow = AddInput(Function, TEXT("Snow"), FunctionInput_Scalar,
        TEXT("Покрытие снегом 0..1. Не подключено -- Snowy из коллекции Ultra Dynamic Weather."), 3, 1, 3);
    UMaterialExpressionCollectionParameter* Snowy = nullptr;
    if (Sources.WeatherCollection->GetParameterId(WeatherSnowParameterName).IsValid())
    {
        Snowy = AddCollectionParameter(Function, Sources.WeatherCollection, WeatherSnowParameterName, 0, 3);
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("В %s нет параметра %s -- версия Ultra Dynamic Weather переименовала покрытие снегом"),
            *Sources.WeatherCollection->GetPathName(), WeatherSnowParameterName);
    }
    UMaterialExpressionCollectionParameter* Weights = AddCollectionParameter(Function, Sources.Collection, TEXT("SeasonWeights"), 1, 5);
    if (!Snowy || !Weights) return false;
    Snow->Preview.Connect(0, Snowy);
    Snow->bUsePreviewValueAsDefault = true;

    // Тропа у основания -- тот же вызов MF_SampleTrample, что у MF_TrampleCompressWPO.
    UMaterialExpressionConstant3Vector* Origin = AddNode<UMaterialExpressionConstant3Vector>(Function, 0, 7);
    Origin->Constant = FLinearColor(0.0f, 0.0f, 0.0f);
    UMaterialExpressionTransformPosition* Pivot = AddNode<UMaterialExpressionTransformPosition>(Function, 1, 7);
    Pivot->TransformSourceType = TRANSFORMPOSSOURCE_Instance;
    Pivot->TransformType = TRANSFORMPOSSOURCE_World;
    Pivot->Input.Connect(0, Origin);
    UMaterialExpressionMaterialFunctionCall* SampleCall = AddNode<UMaterialExpressionMaterialFunctionCall>(Function, 2, 7);
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

    UMaterialExpressionStaticSwitchParameter* Switch = AddNode<UMaterialExpressionStaticSwitchParameter>(Function, 3, 7);
    Switch->ParameterName = TrampleableSwitchName;
    Switch->DefaultValue = false;
    Switch->A.Connect(TrampleOutput, SampleCall);                // True -- тропа
    Switch->B.Connect(0, AddConstant(Function, 0.0f, 2, 8));      // False -- без тропы
    Switch->UpdateParameterGuid(true, true);

    // Зима: saturate((вес зимы - r x 0.3) / 0.7) x WinterStrength.
    UMaterialExpressionComponentMask* WinterWeight = AddMask(Function, Weights, 0, false, false, false, true, 2, 5);
    UMaterialExpressionPerInstanceRandom* InstanceRandom = AddNode<UMaterialExpressionPerInstanceRandom>(Function, 2, 6);
    UMaterialExpressionMultiply* Offset = AddNode<UMaterialExpressionMultiply>(Function, 3, 6);
    Offset->A.Connect(0, InstanceRandom);
    Offset->ConstB = 0.3f;
    UMaterialExpressionSubtract* Shifted = AddBinary<UMaterialExpressionSubtract>(Function, WinterWeight, 0, Offset, 0, 4, 5);
    UMaterialExpressionDivide* Stretched = AddNode<UMaterialExpressionDivide>(Function, 5, 5);
    Stretched->A.Connect(0, Shifted);
    Stretched->ConstB = 0.7f;
    UMaterialExpressionSaturate* WinterRamp = AddNode<UMaterialExpressionSaturate>(Function, 6, 5);
    WinterRamp->Input.Connect(0, Stretched);
    UMaterialExpressionMultiply* WinterSquash = AddBinary<UMaterialExpressionMultiply>(Function, WinterRamp, 0, WinterStrength, 0, 7, 4);

    UMaterialExpressionMultiply* SnowSquash = AddBinary<UMaterialExpressionMultiply>(Function, Snow, 0, SnowStrength, 0, 7, 2);
    UMaterialExpressionMax* TrampleOrWinter = AddBinary<UMaterialExpressionMax>(Function, Switch, 0, WinterSquash, 0, 8, 5);
    UMaterialExpressionMax* AnySquash = AddBinary<UMaterialExpressionMax>(Function, TrampleOrWinter, 0, SnowSquash, 0, 9, 3);
    UMaterialExpressionSaturate* Squash = AddNode<UMaterialExpressionSaturate>(Function, 10, 3);
    Squash->Input.Connect(0, AnySquash);

    UMaterialExpression* Result = AddSquashTowardPivot(Function, Wind, Squash, false, 11, 0);

    AddOutput(Function, TEXT("WPO"), TEXT("В World Position Offset."), 0, Result, 0, 18, 1);
    AddOutput(Function, TEXT("Squash"), TEXT("Итоговое сжатие 0..1: цвет пожухлости, тень."), 1, Squash, 0, 18, 3);
    AddOutput(Function, TEXT("Trample"), TEXT("Вытоптанность у основания (0 при выключенном Trampleable)."), 2, Switch, 0, 18, 5);
    return true;
}

bool HerbalistMaterialFunctions::BuildFlowerOpen(UMaterialFunction* Function, const FSources& Sources)
{
    using namespace Detail;
    if (!Function || !Sources.Collection) return false;

    DescribeFunction(Function, TEXT("Раскрытость цветка (§3.3 плана): Open = SmoothStep(r x 0.25, r x 0.25 + 0.5, OpenPhase . DayPhaseWeights), r -- случайное число экземпляра (поляна раскрывается волной). OpenPhase -- веса окна раскрытия рассвет/день/закат/ночь, параметр инстанса материала вида. Закрытый цветок: лепестки (PetalMask, по умолчанию красный канал цвета вершин) стягиваются к оси по горизонтали на CloseAmount."));

    UMaterialExpressionFunctionInput* OpenPhase = AddInput(Function, TEXT("OpenPhase"), FunctionInput_Vector4,
        TEXT("Окно раскрытия: R рассвет, G день, B закат, A ночь, 0..1. Не подключено -- днём."), 0, 1, 0);
    UMaterialExpressionConstant4Vector* DayOnly = AddNode<UMaterialExpressionConstant4Vector>(Function, 0, 0);
    DayOnly->Constant = FLinearColor(0.0f, 1.0f, 0.0f, 0.0f);
    OpenPhase->Preview.Connect(0, DayOnly);
    OpenPhase->bUsePreviewValueAsDefault = true;
    UMaterialExpressionFunctionInput* Wind = AddWindInput(Function, 1, 1, 1);
    UMaterialExpressionFunctionInput* PetalMask = AddInput(Function, TEXT("PetalMask"), FunctionInput_Scalar,
        TEXT("Какие вершины -- лепестки, 0..1. Не подключено -- красный канал цвета вершин (у меша без цвета вершин -- весь меш)."), 2, 1, 2);
    UMaterialExpressionVertexColor* VertexColor = AddNode<UMaterialExpressionVertexColor>(Function, 0, 2);
    PetalMask->Preview.Connect(1, VertexColor);   // выход 1 -- R
    PetalMask->bUsePreviewValueAsDefault = true;
    UMaterialExpressionFunctionInput* CloseAmount = AddScalarInput(Function, TEXT("CloseAmount"), 0.7f,
        TEXT("Насколько закрытые лепестки стягиваются к оси, 0..1."), 3, 1, 3);
    UMaterialExpressionCollectionParameter* DayPhase = AddCollectionParameter(Function, Sources.Collection, TEXT("DayPhaseWeights"), 1, 5);
    if (!DayPhase) return false;

    UMaterialExpressionComponentMask* Weights = AddMask(Function, DayPhase, 0, true, true, true, true, 2, 5);
    UMaterialExpressionDotProduct* InWindow = AddBinary<UMaterialExpressionDotProduct>(Function, OpenPhase, 0, Weights, 0, 3, 4);
    UMaterialExpressionPerInstanceRandom* InstanceRandom = AddNode<UMaterialExpressionPerInstanceRandom>(Function, 2, 7);
    UMaterialExpressionMultiply* Low = AddNode<UMaterialExpressionMultiply>(Function, 3, 7);
    Low->A.Connect(0, InstanceRandom);
    Low->ConstB = 0.25f;
    UMaterialExpressionAdd* High = AddNode<UMaterialExpressionAdd>(Function, 4, 7);
    High->A.Connect(0, Low);
    High->ConstB = 0.5f;
    UMaterialExpressionSmoothStep* Open = AddNode<UMaterialExpressionSmoothStep>(Function, 5, 5);
    Open->Min.Connect(0, Low);
    Open->Max.Connect(0, High);
    Open->Value.Connect(0, InWindow);

    UMaterialExpressionOneMinus* Closed = AddNode<UMaterialExpressionOneMinus>(Function, 6, 3);
    Closed->Input.Connect(0, Open);
    UMaterialExpressionMultiply* ClosedPetals = AddBinary<UMaterialExpressionMultiply>(Function, Closed, 0, PetalMask, 0, 7, 3);
    UMaterialExpressionMultiply* Amount = AddBinary<UMaterialExpressionMultiply>(Function, ClosedPetals, 0, CloseAmount, 0, 8, 3);

    UMaterialExpression* Result = AddSquashTowardPivot(Function, Wind, Amount, true, 9, 0);

    AddOutput(Function, TEXT("WPO"), TEXT("В World Position Offset."), 0, Result, 0, 16, 1);
    AddOutput(Function, TEXT("Open"), TEXT("Раскрытость 0..1: цвет, свечение ночных цветов."), 1, Open, 0, 16, 4);
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
