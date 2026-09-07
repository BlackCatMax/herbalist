// WorldStateMapSetupCommandlet.cpp

#include "WorldStateMapSetupCommandlet.h"

#include "Core/World/GridWorldManager.h"

#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "Engine/Level.h"
#include "Materials/MaterialParameterCollection.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "Misc/PackageName.h"

namespace
{
    // Имя намеренно отличается от SaveTable/SaveBiomeDefaultsTable у соседних
    // коммандлетов: unity-сборка склеивает все Commandlets/*.cpp в одну единицу
    // трансляции, и одинаковые тела в анонимных namespace дают MSVC C2084
    // (наступали на это 2026-09-07, см. AmbientGatesPatchCommandlet.cpp).
    bool SaveWorldStateMapPackage(UPackage* Package, UObject* Asset)
    {
        Package->MarkPackageDirty();
        const FString FileName = FPackageName::LongPackageNameToFilename(
            Package->GetName(), FPackageName::GetAssetPackageExtension());

        FSavePackageArgs Args;
        Args.TopLevelFlags = RF_Public | RF_Standalone;
        Args.SaveFlags = SAVE_NoError;
        return UPackage::SavePackage(Package, Asset, *FileName, Args);
    }

    const TCHAR* RenderTargetPath = TEXT("/Game/Materials/RT_WorldStateMap");
    const TCHAR* CollectionPath   = TEXT("/Game/Materials/MPC_WorldStateFields");
    const TCHAR* PlaytestMapPath  = TEXT("/Game/Maps/L_Playtest");

    // Ищет менеджер сетки на карте, чтобы узнать размер сетки. Обход
    // PersistentLevel->Actors НАПРЯМУЮ, а не через TActorIterator: мир,
    // поднятый через LoadPackage, не инициализирован, World->Levels у него
    // пуст, и итератор молча не найдёт ничего (урок PlaytestMapResize).
    AGridWorldManager* FindManagerInLevel(UWorld* World)
    {
        if (!World || !World->PersistentLevel) return nullptr;
        for (AActor* Actor : World->PersistentLevel->Actors)
        {
            if (AGridWorldManager* Manager = Cast<AGridWorldManager>(Actor))
            {
                return Manager;
            }
        }
        return nullptr;
    }

    bool AddVectorParameterIfMissing(UMaterialParameterCollection* Collection, FName ParameterName)
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

int32 UWorldStateMapSetupCommandlet::Main(const FString& Params)
{
    UE_LOG(LogTemp, Display, TEXT("=== WorldStateMapSetup ==="));

    // ---- 1. Размер сетки с карты (нужен до создания цели) ----
    int32 SizeX = 256;
    int32 SizeY = 256;

    UPackage* MapPackage = LoadPackage(nullptr, PlaytestMapPath, LOAD_None);
    UWorld* MapWorld = MapPackage ? UWorld::FindWorldInPackage(MapPackage) : nullptr;
    AGridWorldManager* Manager = FindManagerInLevel(MapWorld);

    if (Manager)
    {
        SizeX = FMath::Max(1, Manager->GridSizeX);
        SizeY = FMath::Max(1, Manager->GridSizeY);
        UE_LOG(LogTemp, Display, TEXT("Сетка на L_Playtest: %d x %d клеток, клетка %.0f см"),
            SizeX, SizeY, Manager->CellSize);
    }
    else
    {
        UE_LOG(LogTemp, Warning,
            TEXT("Менеджер сетки на L_Playtest не найден -- размер цели взят по умолчанию (%d x %d). "
                 "Рантайм всё равно подгонит его под сетку через ResizeTarget."), SizeX, SizeY);
    }

    // ---- 2. Render target ----
    UTextureRenderTarget2D* RenderTarget =
        LoadObject<UTextureRenderTarget2D>(nullptr, RenderTargetPath);

    if (RenderTarget)
    {
        UE_LOG(LogTemp, Display, TEXT("RT_WorldStateMap уже есть (%d x %d) -- не трогаю."),
            RenderTarget->SizeX, RenderTarget->SizeY);
    }
    else
    {
        UPackage* Package = CreatePackage(RenderTargetPath);
        if (!Package)
        {
            UE_LOG(LogTemp, Error, TEXT("Не удалось создать пакет %s"), RenderTargetPath);
            return 1;
        }

        RenderTarget = NewObject<UTextureRenderTarget2D>(
            Package, TEXT("RT_WorldStateMap"), RF_Public | RF_Standalone);

        // Доводы по каждой из этих настроек -- в шапке заголовка; все три
        // влияют на правильность, а не на вкус.
        RenderTarget->RenderTargetFormat = RTF_RGBA8;
        RenderTarget->bForceLinearGamma = true;
        RenderTarget->Filter = TF_Bilinear;
        RenderTarget->AddressX = TA_Clamp;
        RenderTarget->AddressY = TA_Clamp;
        RenderTarget->ClearColor = FLinearColor::Black;
        RenderTarget->InitAutoFormat(SizeX, SizeY);
        RenderTarget->UpdateResourceImmediate(true);

        FAssetRegistryModule::AssetCreated(RenderTarget);

        if (!SaveWorldStateMapPackage(Package, RenderTarget))
        {
            UE_LOG(LogTemp, Error, TEXT("Не удалось сохранить %s"), RenderTargetPath);
            return 1;
        }
        UE_LOG(LogTemp, Display, TEXT("Создан RT_WorldStateMap: %d x %d, RGBA8, линейная гамма, билинейный, clamp"),
            SizeX, SizeY);
    }

    // ---- 3. Параметры рамки в MPC ----
    // Материалу нужно уметь построить UV из абсолютной мировой позиции:
    // UV = (WorldPos.XY - Origin.XY) / Size.XY. Origin и Size кладём в тот
    // же MPC, где уже живёт GlobalMorok -- заводить второй канал ради двух
    // векторов незачем. Значения туда пишет сам менеджер в рантайме
    // (UpdateWorldStateMap), здесь только заводятся сами параметры: MPC
    // без объявленного параметра игнорирует запись в него молча.
    if (UMaterialParameterCollection* Collection =
            LoadObject<UMaterialParameterCollection>(nullptr, CollectionPath))
    {
        const bool bAddedOrigin = AddVectorParameterIfMissing(Collection, FName(TEXT("WorldStateMapOrigin")));
        const bool bAddedSize   = AddVectorParameterIfMissing(Collection, FName(TEXT("WorldStateMapSize")));

        if (bAddedOrigin || bAddedSize)
        {
            if (!SaveWorldStateMapPackage(Collection->GetOutermost(), Collection))
            {
                UE_LOG(LogTemp, Error, TEXT("Не удалось сохранить %s"), CollectionPath);
                return 1;
            }
        }
        else
        {
            UE_LOG(LogTemp, Display, TEXT("Параметры рамки в MPC уже есть -- не трогаю."));
        }
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("Не найден %s"), CollectionPath);
        return 1;
    }

    // ---- 4. Назначение цели менеджеру на карте ----
    if (!Manager)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("Назначить render target некому -- менеджер на карте не найден. "
                 "Ассеты созданы, назначение придётся сделать вручную."));
        return 0;
    }

    bool bMapChanged = false;

    if (Manager->WorldStateMap.IsNull())
    {
        Manager->WorldStateMap = RenderTarget;
        bMapChanged = true;
        UE_LOG(LogTemp, Display, TEXT("  + менеджеру назначен WorldStateMap"));
    }
    else
    {
        UE_LOG(LogTemp, Display, TEXT("У менеджера уже назначен WorldStateMap (%s) -- не трогаю."),
            *Manager->WorldStateMap.ToString());
    }

    if (Manager->WorldStateFrameCollection.IsNull())
    {
        Manager->WorldStateFrameCollection =
            LoadObject<UMaterialParameterCollection>(nullptr, CollectionPath);
        bMapChanged = true;
        UE_LOG(LogTemp, Display, TEXT("  + менеджеру назначен WorldStateFrameCollection"));
    }
    else
    {
        UE_LOG(LogTemp, Display, TEXT("У менеджера уже назначен WorldStateFrameCollection -- не трогаю."));
    }

    if (!bMapChanged)
    {
        UE_LOG(LogTemp, Display, TEXT("=== WorldStateMapSetup: менять на карте нечего ==="));
        return 0;
    }

    Manager->MarkPackageDirty();

    const FString MapFileName = FPackageName::LongPackageNameToFilename(
        MapPackage->GetName(), FPackageName::GetMapPackageExtension());

    FSavePackageArgs MapSaveArgs;
    MapSaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
    MapSaveArgs.SaveFlags = SAVE_NoError;
    if (!UPackage::SavePackage(MapPackage, MapWorld, *MapFileName, MapSaveArgs))
    {
        UE_LOG(LogTemp, Error, TEXT("Не удалось сохранить карту %s"), PlaytestMapPath);
        return 1;
    }

    UE_LOG(LogTemp, Display, TEXT("RT_WorldStateMap назначен менеджеру на L_Playtest, карта сохранена."));
    UE_LOG(LogTemp, Display, TEXT("=== WorldStateMapSetup: готово ==="));
    return 0;
}
