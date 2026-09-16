// MaterialFunctionsSetupCommandlet.cpp

#include "MaterialFunctionsSetupCommandlet.h"

#include "HerbalistMaterialFunctionGraphs.h"

#include "MaterialEditingLibrary.h"
#include "MaterialShared.h"
#include "RHIGlobals.h"
#include "Engine/Texture.h"
#include "Materials/Material.h"
#include "Materials/MaterialFunction.h"
#include "Materials/MaterialParameterCollection.h"
#include "Materials/MaterialExpressionMaterialFunctionCall.h"
#include "Materials/MaterialExpressionStaticSwitchParameter.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "Misc/PackageName.h"

namespace
{
    bool SaveMaterialFunctionPackage(UMaterialFunction* Function)
    {
        UPackage* Package = Function->GetOutermost();
        Package->MarkPackageDirty();
        const FString FileName = FPackageName::LongPackageNameToFilename(
            Package->GetName(), FPackageName::GetAssetPackageExtension());

        FSavePackageArgs Args;
        Args.TopLevelFlags = RF_Public | RF_Standalone;
        Args.SaveFlags = SAVE_NoError;
        return UPackage::SavePackage(Package, Function, *FileName, Args);
    }

    enum class EMaterialFunctionPrepareResult
    {
        Skip,      // есть и не перестраивается
        Build,     // пустая функция, строить
        Failed,
    };

    // Существующую функцию при перестройке очищает, запомнив Id входов и выходов.
    EMaterialFunctionPrepareResult PrepareMaterialFunction(const TCHAR* Name, bool bRebuild,
        UMaterialFunction*& OutFunction, HerbalistMaterialFunctions::FFunctionPinIds& OutPinIds)
    {
        const FString PackagePath = FString::Printf(TEXT("%s/%s"), HerbalistMaterialFunctions::FunctionsFolder, Name);
        if (UMaterialFunction* Existing = LoadObject<UMaterialFunction>(nullptr, *PackagePath))
        {
            OutFunction = Existing;
            if (!bRebuild)
            {
                return EMaterialFunctionPrepareResult::Skip;
            }
            OutPinIds = HerbalistMaterialFunctions::CaptureFunctionPinIds(Existing);
            if (!HerbalistMaterialFunctions::ClearMaterialFunction(Existing))
            {
                UE_LOG(LogTemp, Error, TEXT("%s: старый граф не удалился целиком"), Name);
                return EMaterialFunctionPrepareResult::Failed;
            }
            return EMaterialFunctionPrepareResult::Build;
        }

        UPackage* Package = CreatePackage(*PackagePath);
        if (!Package)
        {
            UE_LOG(LogTemp, Error, TEXT("Не удалось создать пакет %s"), *PackagePath);
            return EMaterialFunctionPrepareResult::Failed;
        }
        OutFunction = NewObject<UMaterialFunction>(Package, Name, RF_Public | RF_Standalone | RF_Transactional);
        FAssetRegistryModule::AssetCreated(OutFunction);
        return EMaterialFunctionPrepareResult::Build;
    }

    // -verify: временный материал с вызовом функции, выход -- в свойство
    // материала, синхронная компиляция под текущую платформу шейдеров. Автотест
    // графа компиляцию не видит, это её единственная проверка без редактора.
    // Попадание текстуры в скомпилированный материал доказывает, что ветка с
    // выборкой карты действительно собрана, а не отброшена.
    bool VerifyMaterialFunctionCompiles(UMaterialFunction* Function, const TCHAR* OutputName, EMaterialProperty Property,
        const TCHAR* Case, UTexture* Texture, bool bExpectTexture)
    {
        UMaterial* Material = NewObject<UMaterial>(GetTransientPackage(), NAME_None, RF_Transient);
        if (Property == MP_OpacityMask)
        {
            // Маска компилируется только у маскированного материала.
            Material->BlendMode = BLEND_Masked;
        }
        UMaterialExpressionMaterialFunctionCall* Call = Cast<UMaterialExpressionMaterialFunctionCall>(
            UMaterialEditingLibrary::CreateMaterialExpression(Material, UMaterialExpressionMaterialFunctionCall::StaticClass()));
        if (!Call || !Call->SetMaterialFunction(Function) || !UMaterialEditingLibrary::ConnectMaterialProperty(Call, OutputName, Property))
        {
            UE_LOG(LogTemp, Error, TEXT("[verify] %s (%s): не удалось подключить выход %s"), *Function->GetName(), Case, OutputName);
            return false;
        }

        Material->ForceRecompileForRendering(EMaterialShaderPrecompileMode::Synchronous);
        FMaterialResource* Resource = Material->GetMaterialResource(GMaxRHIShaderPlatform);
        if (!Resource)
        {
            UE_LOG(LogTemp, Error, TEXT("[verify] %s (%s): нет ресурса материала для платформы шейдеров (запуск без -nullrhi, с -AllowCommandletRendering)"),
                *Function->GetName(), Case);
            return false;
        }
        Resource->FinishCompilation();
        for (const FString& CompileError : Resource->GetCompileErrors())
        {
            UE_LOG(LogTemp, Error, TEXT("[verify] %s (%s): %s"), *Function->GetName(), Case, *CompileError);
        }
        if (Resource->GetCompileErrors().Num() > 0)
        {
            return false;
        }

        // Только текстуры, вошедшие в скомпилированный код: GetReferencedTextures
        // собирает их со всех узлов, включая отброшенную ветку переключателя.
        bool bTextureReferenced = false;
        for (const FMaterialTextureParameterInfo& Used : Resource->GetUniform2DTextureExpressions())
        {
            UTexture* UsedTexture = nullptr;
            Used.GetGameThreadTextureValue(Material, *Resource, UsedTexture);
            bTextureReferenced |= UsedTexture == Texture;
        }
        if (bTextureReferenced != bExpectTexture)
        {
            UE_LOG(LogTemp, Error, TEXT("[verify] %s (%s): карта %s %s, ожидалось обратное"), *Function->GetName(), Case,
                *GetNameSafe(Texture), bTextureReferenced ? TEXT("читается") : TEXT("не читается"));
            return false;
        }
        UE_LOG(LogTemp, Display, TEXT("[verify] %s (%s): компилируется, карта %s"), *Function->GetName(), Case,
            bTextureReferenced ? TEXT("читается") : TEXT("не читается"));
        return true;
    }

    // Переключатель по умолчанию выключен -- ветка сжатия без этого не
    // компилировалась бы вовсе. Значение меняется только в памяти, не сохраняется.
    // Годится для любой функции с переключателем Trampleable (MF_TrampleCompressWPO, MF_GrassSquash).
    bool VerifyTrampleCompressBothBranches(UMaterialFunction* Function, UTexture* TrampleMap)
    {
        UMaterialExpressionStaticSwitchParameter* Switch = nullptr;
        for (UMaterialExpression* Expression : Function->GetExpressions())
        {
            if (UMaterialExpressionStaticSwitchParameter* Found = Cast<UMaterialExpressionStaticSwitchParameter>(Expression))
            {
                Switch = Found;
            }
        }
        if (!Switch)
        {
            UE_LOG(LogTemp, Error, TEXT("[verify] %s: нет переключателя"), *Function->GetName());
            return false;
        }

        const bool bDefault = Switch->DefaultValue;
        Switch->DefaultValue = false;
        const bool bWindOnly = VerifyMaterialFunctionCompiles(Function, TEXT("WPO"), MP_WorldPositionOffset,
            TEXT("Trampleable выключен"), TrampleMap, /*bExpectTexture=*/false);
        Switch->DefaultValue = true;
        const bool bTrampled = VerifyMaterialFunctionCompiles(Function, TEXT("WPO"), MP_WorldPositionOffset,
            TEXT("Trampleable включён"), TrampleMap, /*bExpectTexture=*/true);
        Switch->DefaultValue = bDefault;
        return bWindOnly && bTrampled;
    }
}

int32 UMaterialFunctionsSetupCommandlet::Main(const FString& Params)
{
    using namespace HerbalistMaterialFunctions;
    UE_LOG(LogTemp, Display, TEXT("=== MaterialFunctionsSetup ==="));
    const bool bRebuild = FParse::Param(*Params, TEXT("rebuild"));
    // -only=MF_A,MF_B -- -rebuild только перечисленных: остальные функции могли
    // поправить в редакторе, их незачем пересобирать ради новой.
    FString OnlyList;
    FParse::Value(*Params, TEXT("only="), OnlyList);
    TArray<FString> OnlyNames;
    OnlyList.ParseIntoArray(OnlyNames, TEXT(","), true);
    if (OnlyNames.Num() > 0 && !bRebuild)
    {
        UE_LOG(LogTemp, Warning, TEXT("-only= действует только вместе с -rebuild -- ничего не перестраиваю"));
    }

    FSources Sources;
    Sources.Collection = LoadObject<UMaterialParameterCollection>(nullptr, CollectionPath);
    Sources.WorldStateMap = LoadObject<UTexture>(nullptr, WorldStateMapPath);
    Sources.TrampleMap = LoadObject<UTexture>(nullptr, TrampleMapPath);
    Sources.WeatherCollection = LoadObject<UMaterialParameterCollection>(nullptr, WeatherCollectionPath);
    if (!Sources.Collection || !Sources.WorldStateMap || !Sources.TrampleMap)
    {
        UE_LOG(LogTemp, Error, TEXT("Нет %s, %s или %s -- сначала -run=WorldStateMapSetup и -run=TrampleMapSetup"),
            CollectionPath, WorldStateMapPath, TrampleMapPath);
        return 1;
    }
    if (!Sources.WeatherCollection)
    {
        UE_LOG(LogTemp, Error, TEXT("Нет %s -- Ultra Dynamic Weather не в проекте (MF_GrassSquash берёт снег оттуда)"), WeatherCollectionPath);
        return 1;
    }

    UMaterialFunction* WorldState = nullptr;
    UMaterialFunction* Trample = nullptr;
    UMaterialFunction* Compress = nullptr;
    UMaterialFunction* SeasonWeights = nullptr;
    UMaterialFunction* SeasonColor = nullptr;
    UMaterialFunction* LeafDrop = nullptr;
    UMaterialFunction* GrassSquash = nullptr;
    UMaterialFunction* FlowerOpen = nullptr;
    bool bTrampleBuilt = false;

    struct FStep
    {
        const TCHAR* Name;
        UMaterialFunction** Slot;
        TFunction<bool(UMaterialFunction*)> Build;
    };
    const TArray<FStep> Steps = {
        { SampleWorldStateName, &WorldState, [&Sources](UMaterialFunction* F) { return BuildSampleWorldState(F, Sources); } },
        { SampleTrampleName, &Trample, [&Sources](UMaterialFunction* F) { return BuildSampleTrample(F, Sources); } },
        // Зовёт MF_SampleTrample -- строится после неё.
        { TrampleCompressName, &Compress, [&Trample](UMaterialFunction* F) { return BuildTrampleCompressWPO(F, Trample); } },
        // Слой сезона и суток (этап 3 DESIGN_Living_Vegetation_Research.md).
        { SeasonWeightsName, &SeasonWeights, [&Sources](UMaterialFunction* F) { return BuildSeasonWeights(F, Sources); } },
        { SeasonColorName, &SeasonColor, [&Sources](UMaterialFunction* F) { return BuildSeasonColor(F, Sources); } },
        { LeafDropName, &LeafDrop, [&Sources](UMaterialFunction* F) { return BuildLeafDrop(F, Sources); } },
        // Тоже зовёт MF_SampleTrample.
        { GrassSquashName, &GrassSquash, [&Sources, &Trample](UMaterialFunction* F) { return BuildGrassSquash(F, Sources, Trample); } },
        { FlowerOpenName, &FlowerOpen, [&Sources](UMaterialFunction* F) { return BuildFlowerOpen(F, Sources); } },
    };

    for (const FString& Only : OnlyNames)
    {
        if (!Steps.ContainsByPredicate([&Only](const FStep& Step) { return Only == Step.Name; }))
        {
            UE_LOG(LogTemp, Warning, TEXT("-only=: функции %s нет (имена с MF_)"), *Only);
        }
    }

    for (const FStep& Step : Steps)
    {
        // Вызов MF_SampleTrample внутри сжатия запоминает её входы и выходы --
        // перестроенная выборка тропы тянет за собой перестройку сжатия.
        const bool bDependsOnRebuiltTrample = (Step.Slot == &Compress || Step.Slot == &GrassSquash) && bTrampleBuilt;

        FFunctionPinIds PinIds;
        const bool bSelected = OnlyNames.Num() == 0 || OnlyNames.Contains(Step.Name);
        const EMaterialFunctionPrepareResult Prepared = PrepareMaterialFunction(Step.Name, (bRebuild && bSelected) || bDependsOnRebuiltTrample, *Step.Slot, PinIds);
        if (Prepared == EMaterialFunctionPrepareResult::Failed)
        {
            return 1;
        }
        if (Prepared == EMaterialFunctionPrepareResult::Skip)
        {
            UE_LOG(LogTemp, Display, TEXT("%s уже есть -- не трогаю (-rebuild перестроит)."), Step.Name);
            continue;
        }

        UMaterialFunction* Function = *Step.Slot;
        if (!Step.Build(Function))
        {
            UE_LOG(LogTemp, Error, TEXT("%s: граф не собран, не сохраняю."), Step.Name);
            return 1;
        }
        RestoreFunctionPinIds(Function, PinIds);
        UMaterialEditingLibrary::LayoutMaterialFunctionExpressions(Function);
        UMaterialEditingLibrary::UpdateMaterialFunction(Function, nullptr);
        if (!SaveMaterialFunctionPackage(Function))
        {
            UE_LOG(LogTemp, Error, TEXT("Не удалось сохранить %s"), *Function->GetPathName());
            return 1;
        }
        bTrampleBuilt |= Step.Slot == &Trample;
        UE_LOG(LogTemp, Display, TEXT("Собрана %s"), *Function->GetPathName());
    }

    if (FParse::Param(*Params, TEXT("verify")))
    {
        bool bAllCompile = true;
        bAllCompile &= VerifyMaterialFunctionCompiles(WorldState, TEXT("Distortion"), MP_BaseColor, TEXT("шейдер пикселей"), Sources.WorldStateMap, true);
        bAllCompile &= VerifyMaterialFunctionCompiles(WorldState, TEXT("Distortion"), MP_WorldPositionOffset, TEXT("шейдер вершин"), Sources.WorldStateMap, true);
        bAllCompile &= VerifyMaterialFunctionCompiles(Trample, TEXT("Trample"), MP_BaseColor, TEXT("шейдер пикселей"), Sources.TrampleMap, true);
        bAllCompile &= VerifyMaterialFunctionCompiles(Trample, TEXT("Trample"), MP_WorldPositionOffset, TEXT("шейдер вершин"), Sources.TrampleMap, true);
        bAllCompile &= VerifyTrampleCompressBothBranches(Compress, Sources.TrampleMap);
        // Слой сезона: текстур не читает, кроме тропы у MF_GrassSquash.
        bAllCompile &= VerifyMaterialFunctionCompiles(SeasonWeights, TEXT("Winter"), MP_BaseColor, TEXT("шейдер пикселей"), Sources.TrampleMap, false);
        bAllCompile &= VerifyMaterialFunctionCompiles(SeasonWeights, TEXT("LeafDrop01"), MP_WorldPositionOffset, TEXT("шейдер вершин"), Sources.TrampleMap, false);
        bAllCompile &= VerifyMaterialFunctionCompiles(SeasonWeights, TEXT("LeafLitter01"), MP_BaseColor, TEXT("подстилка, шейдер пикселей"), Sources.TrampleMap, false);
        bAllCompile &= VerifyMaterialFunctionCompiles(SeasonColor, TEXT("Color"), MP_BaseColor, TEXT("шейдер пикселей"), Sources.TrampleMap, false);
        bAllCompile &= VerifyMaterialFunctionCompiles(LeafDrop, TEXT("OpacityMask"), MP_BaseColor, TEXT("шейдер пикселей"), Sources.TrampleMap, false);
        bAllCompile &= VerifyMaterialFunctionCompiles(LeafDrop, TEXT("OpacityMask"), MP_OpacityMask, TEXT("маска, маскированный материал"), Sources.TrampleMap, false);
        bAllCompile &= VerifyTrampleCompressBothBranches(GrassSquash, Sources.TrampleMap);
        bAllCompile &= VerifyMaterialFunctionCompiles(FlowerOpen, TEXT("WPO"), MP_WorldPositionOffset, TEXT("шейдер вершин"), Sources.TrampleMap, false);
        bAllCompile &= VerifyMaterialFunctionCompiles(FlowerOpen, TEXT("Open"), MP_BaseColor, TEXT("шейдер пикселей"), Sources.TrampleMap, false);
        if (!bAllCompile)
        {
            UE_LOG(LogTemp, Error, TEXT("[verify] есть ошибки"));
            return 1;
        }
    }

    UE_LOG(LogTemp, Display, TEXT("=== MaterialFunctionsSetup: готово ==="));
    return 0;
}
