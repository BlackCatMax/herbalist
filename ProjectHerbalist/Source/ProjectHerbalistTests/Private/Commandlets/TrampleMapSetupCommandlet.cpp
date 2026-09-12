// TrampleMapSetupCommandlet.cpp

#include "TrampleMapSetupCommandlet.h"

#include "Core/World/Trample/TrampleWindow.h"

#include "Engine/TextureRenderTarget2D.h"
#include "Materials/MaterialParameterCollection.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "Misc/PackageName.h"

namespace
{
    // Имена уникальны: unity-сборка склеивает все Commandlets/*.cpp в одну
    // единицу трансляции (см. WorldStateMapSetupCommandlet.cpp).
    const TCHAR* TrampleMapAssetPath = TEXT("/Game/Materials/RT_TrampleMap");
    const TCHAR* TrampleCollectionAssetPath = TEXT("/Game/Materials/MPC_WorldStateFields");

    bool SaveTrampleSetupPackage(UPackage* Package, UObject* Asset)
    {
        Package->MarkPackageDirty();
        const FString FileName = FPackageName::LongPackageNameToFilename(
            Package->GetName(), FPackageName::GetAssetPackageExtension());

        FSavePackageArgs Args;
        Args.TopLevelFlags = RF_Public | RF_Standalone;
        Args.SaveFlags = SAVE_NoError;
        return UPackage::SavePackage(Package, Asset, *FileName, Args);
    }

    // Приводит цель к нужным настройкам. true -- что-то поменялось.
    bool ConfigureTrampleMapTarget(UTextureRenderTarget2D* Target)
    {
        bool bChanged = false;
        if (Target->RenderTargetFormat != RTF_RGBA8) { Target->RenderTargetFormat = RTF_RGBA8; bChanged = true; }
        if (!Target->bForceLinearGamma) { Target->bForceLinearGamma = true; bChanged = true; }
        if (Target->Filter != TF_Bilinear) { Target->Filter = TF_Bilinear; bChanged = true; }
        if (Target->AddressX != TA_Wrap) { Target->AddressX = TA_Wrap; bChanged = true; }
        if (Target->AddressY != TA_Wrap) { Target->AddressY = TA_Wrap; bChanged = true; }
        if (Target->ClearColor != FLinearColor::Black) { Target->ClearColor = FLinearColor::Black; bChanged = true; }
        if (Target->SizeX != FTrampleWindow::Size || Target->SizeY != FTrampleWindow::Size)
        {
            Target->InitAutoFormat(FTrampleWindow::Size, FTrampleWindow::Size);
            bChanged = true;
        }
        if (bChanged)
        {
            Target->UpdateResourceImmediate(true);
        }

        // Та же ловушка, что у RT_WorldStateMap: поле UTexture::SRGB у
        // созданного в коммандлете ассета не синхронизируется с IsSRGB(), и
        // материал отказывается компилировать сэмплер Linear Color.
        const bool bShouldBeSRGB = Target->IsSRGB();
        if (Target->SRGB != bShouldBeSRGB)
        {
            Target->SRGB = bShouldBeSRGB;
            bChanged = true;
        }
        return bChanged;
    }

    bool AddTrampleCollectionVector(UMaterialParameterCollection* Collection, FName ParameterName)
    {
        for (const FCollectionVectorParameter& Existing : Collection->VectorParameters)
        {
            if (Existing.ParameterName == ParameterName)
            {
                return false;
            }
        }

        FCollectionVectorParameter NewParameter;
        NewParameter.ParameterName = ParameterName;
        NewParameter.DefaultValue = FLinearColor(0.0f, 0.0f, 0.0f, 0.0f);
        NewParameter.Id = FGuid::NewGuid();
        Collection->VectorParameters.Add(NewParameter);

        UE_LOG(LogTemp, Display, TEXT("  + параметр MPC: %s"), *ParameterName.ToString());
        return true;
    }
}

int32 UTrampleMapSetupCommandlet::Main(const FString& Params)
{
    UE_LOG(LogTemp, Display, TEXT("=== TrampleMapSetup ==="));

    // ---- 1. Текстура ----
    if (UTextureRenderTarget2D* Existing = LoadObject<UTextureRenderTarget2D>(nullptr, TrampleMapAssetPath))
    {
        if (ConfigureTrampleMapTarget(Existing))
        {
            if (!SaveTrampleSetupPackage(Existing->GetOutermost(), Existing))
            {
                UE_LOG(LogTemp, Error, TEXT("Не удалось сохранить %s"), TrampleMapAssetPath);
                return 1;
            }
            UE_LOG(LogTemp, Display, TEXT("RT_TrampleMap приведён к нужным настройкам."));
        }
        else
        {
            UE_LOG(LogTemp, Display, TEXT("RT_TrampleMap уже настроен -- не трогаю."));
        }
    }
    else
    {
        UPackage* Package = CreatePackage(TrampleMapAssetPath);
        if (!Package)
        {
            UE_LOG(LogTemp, Error, TEXT("Не удалось создать пакет %s"), TrampleMapAssetPath);
            return 1;
        }

        UTextureRenderTarget2D* Target = NewObject<UTextureRenderTarget2D>(Package, TEXT("RT_TrampleMap"), RF_Public | RF_Standalone);
        Target->InitAutoFormat(FTrampleWindow::Size, FTrampleWindow::Size);
        ConfigureTrampleMapTarget(Target);
        Target->UpdateResourceImmediate(true);
        FAssetRegistryModule::AssetCreated(Target);

        if (!SaveTrampleSetupPackage(Package, Target))
        {
            UE_LOG(LogTemp, Error, TEXT("Не удалось сохранить %s"), TrampleMapAssetPath);
            return 1;
        }
        UE_LOG(LogTemp, Display, TEXT("Создан RT_TrampleMap: %d x %d, RGBA8, линейная гамма, билинейный, wrap"),
            FTrampleWindow::Size, FTrampleWindow::Size);
    }

    // ---- 2. Рамка в MPC ----
    UMaterialParameterCollection* Collection = LoadObject<UMaterialParameterCollection>(nullptr, TrampleCollectionAssetPath);
    if (!Collection)
    {
        UE_LOG(LogTemp, Error, TEXT("Не найден %s"), TrampleCollectionAssetPath);
        return 1;
    }

    const bool bAddedFrame = AddTrampleCollectionVector(Collection, FName(TEXT("TrampleMapFrame")));
    const bool bAddedPlayer = AddTrampleCollectionVector(Collection, FName(TEXT("TramplePlayerPosition")));
    if (bAddedFrame || bAddedPlayer)
    {
        if (!SaveTrampleSetupPackage(Collection->GetOutermost(), Collection))
        {
            UE_LOG(LogTemp, Error, TEXT("Не удалось сохранить %s"), TrampleCollectionAssetPath);
            return 1;
        }
    }
    else
    {
        UE_LOG(LogTemp, Display, TEXT("Параметры троп в MPC уже есть -- не трогаю."));
    }

    UE_LOG(LogTemp, Display, TEXT("=== TrampleMapSetup: готово ==="));
    return 0;
}
