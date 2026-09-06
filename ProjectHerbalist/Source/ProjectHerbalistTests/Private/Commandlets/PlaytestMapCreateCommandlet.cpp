// PlaytestMapCreateCommandlet.cpp
#include "Commandlets/PlaytestMapCreateCommandlet.h"
#include "Core/World/GridWorldManager.h"
#include "Core/World/BiomeRegionVolume.h"
#include "Core/Storage/AlchemyTableActor.h"
#include "GameFramework/PlayerStart.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "Misc/PackageName.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/World.h"

namespace
{
    // Один регион-полоса на биом, во весь X, полосой по Y -- тот же
    // порядок север-юг, что уже задаёт сам список EBiomeType
    // (HerbalistCoreTypes.h). Числа в единицах CellSize=100 (см. довод у
    // AGridWorldManager::GridSizeX/GridSizeY/CellSize) -- грид 20x20 даёт
    // клетки в [0,1900], края региона -100/+2100 намеренно с запасом,
    // чтобы захватить клетки на самой границе сетки без зазора.
    struct FBiomeBand
    {
        EBiomeType Biome;
        float MinY;
        float MaxY;
    };

    void SpawnBiomeBand(UWorld* World, const FBiomeBand& Band)
    {
        ABiomeRegionVolume* Region = World->SpawnActor<ABiomeRegionVolume>();
        if (!Region) return;

        Region->Biome = Band.Biome;
        const TArray<FVector> Corners = {
            FVector(-100.0f, Band.MinY, 0.0f),
            FVector(2100.0f, Band.MinY, 0.0f),
            FVector(2100.0f, Band.MaxY, 0.0f),
            FVector(-100.0f, Band.MaxY, 0.0f),
        };
        Region->SetSplinePointsWorld(Corners);
    }
}

int32 UPlaytestMapCreateCommandlet::Main(const FString& Params)
{
    const TCHAR* AssetPath = TEXT("/Game/Maps/L_Playtest");

    if (FPackageName::DoesPackageExist(AssetPath))
    {
        UE_LOG(LogTemp, Display, TEXT("PlaytestMapCreate: %s уже существует, ничего не делаю"), AssetPath);
        return 0;
    }

    UPackage* Package = CreatePackage(AssetPath);
    if (!Package)
    {
        UE_LOG(LogTemp, Error, TEXT("PlaytestMapCreate: не удалось создать пакет %s"), AssetPath);
        return 1;
    }
    Package->FullyLoad();

    UWorld* NewWorld = UWorld::CreateWorld(EWorldType::Editor, /*bInformEngineOfWorld=*/false, FName(TEXT("L_Playtest")), Package);
    if (!NewWorld)
    {
        UE_LOG(LogTemp, Error, TEXT("PlaytestMapCreate: UWorld::CreateWorld вернул nullptr"));
        return 1;
    }
    NewWorld->SetFlags(RF_Public | RF_Standalone);
    FAssetRegistryModule::AssetCreated(NewWorld);

    // Менеджер мира в начале координат -- клетка (X,Y) отображается ровно
    // в (X*100, Y*100, 0) (см. AGridWorldManager::GetCellWorldPositionFlat),
    // без этого пришлось бы сдвигать все восемь полос биомов на
    // произвольное смещение актора.
    AGridWorldManager* Manager = NewWorld->SpawnActor<AGridWorldManager>(AGridWorldManager::StaticClass(), FTransform::Identity);
    if (!Manager)
    {
        UE_LOG(LogTemp, Error, TEXT("PlaytestMapCreate: не удалось заспавнить AGridWorldManager"));
        return 1;
    }

    // Восемь биомов полосами по Y, тот же порядок, что EBiomeType -- см.
    // довод у FBiomeBand выше. 2200 единиц (-100..2100) / 8 = 275 на полосу.
    const TArray<FBiomeBand> Bands = {
        { EBiomeType::Tundra,          -100.0f,  175.0f },
        { EBiomeType::Taiga,            175.0f,  450.0f },
        { EBiomeType::MixedForest,      450.0f,  725.0f },
        { EBiomeType::BroadleafForest,  725.0f, 1000.0f },
        { EBiomeType::ForestSteppe,    1000.0f, 1275.0f },
        { EBiomeType::Steppe,          1275.0f, 1550.0f },
        { EBiomeType::Floodplain,      1550.0f, 1825.0f },
        { EBiomeType::Bog,             1825.0f, 2100.0f },
    };
    for (const FBiomeBand& Band : Bands)
    {
        SpawnBiomeBand(NewWorld, Band);
    }

    // Алхимический стол (домашний якорь) -- клетка (10,10), примерно в
    // центре сетки, на границе MixedForest/BroadleafForest полос. Z=50 --
    // без ландшафта высота клетки везде 0 (см. довод у файла), приподнят
    // над нулём для видимости, не для физики (у стола нет гравитации).
    NewWorld->SpawnActor<AAlchemyTableActor>(AAlchemyTableActor::StaticClass(),
        FTransform(FRotator::ZeroRotator, FVector(1000.0f, 1000.0f, 50.0f)));

    // Игрок стартует рядом со столом, не поверх него.
    NewWorld->SpawnActor<APlayerStart>(APlayerStart::StaticClass(),
        FTransform(FRotator::ZeroRotator, FVector(900.0f, 900.0f, 100.0f)));

    Package->MarkPackageDirty();

    const FString PackageFileName = FPackageName::LongPackageNameToFilename(
        Package->GetName(), FPackageName::GetMapPackageExtension());

    FSavePackageArgs SaveArgs;
    SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
    SaveArgs.SaveFlags = SAVE_NoError;

    const bool bSuccess = UPackage::SavePackage(Package, NewWorld, *PackageFileName, SaveArgs);
    if (!bSuccess)
    {
        UE_LOG(LogTemp, Error, TEXT("PlaytestMapCreate: не удалось сохранить пакет %s"), *PackageFileName);
        return 1;
    }

    UE_LOG(LogTemp, Display, TEXT("PlaytestMapCreate: создана %s (%d биом-регионов, стол, PlayerStart, GridWorldManager)"),
        AssetPath, Bands.Num());

    // CreateWorld(bInformEngineOfWorld=false) не регистрирует мир в списке
    // движка -- без явного CleanupWorld() здесь движок при выходе из
    // процесса пытается уничтожить ещё живой, не подготовленный к
    // разрушению мир (подсистемы, в т.ч. Water, падают в своём собственном
    // teardown). Найдено эмпирически (2026-09-06) -- SavePackage к этому
    // моменту уже успешно отработал, файл на диске корректен независимо
    // от этого падения, но сам краш недопустим для чистого прогона.
    NewWorld->CleanupWorld();
    return 0;
}
