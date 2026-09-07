// PlaytestMapResizeCommandlet.cpp
#include "Commandlets/PlaytestMapResizeCommandlet.h"

#include "Core/World/GridWorldManager.h"
#include "Core/World/BiomeRegionVolume.h"
#include "Core/Types/BiomeTypes.h"

#include "Engine/World.h"
#include "EngineUtils.h"
#include "UObject/SavePackage.h"
#include "Misc/PackageName.h"
#include "Misc/Parse.h"

namespace
{
    // Порядок полос — тот же, что задаёт сам EBiomeType (север-юг), ровно как
    // в PlaytestMapCreateCommandlet. Дублировать список нельзя: расхождение
    // порядка означало бы, что пересозданная карта и изменённая в размере
    // раскладывают биомы по-разному. Берём его из общего источника.
    TArray<EBiomeType> BandOrder()
    {
        return {
            EBiomeType::Tundra,
            EBiomeType::Taiga,
            EBiomeType::MixedForest,
            EBiomeType::BroadleafForest,
            EBiomeType::ForestSteppe,
            EBiomeType::Steppe,
            EBiomeType::Floodplain,
            EBiomeType::Bog,
        };
    }
}

int32 UPlaytestMapResizeCommandlet::Main(const FString& Params)
{
    int32 NewSize = 128;
    FParse::Value(*Params, TEXT("size="), NewSize);
    if (NewSize < 8)
    {
        UE_LOG(LogTemp, Error, TEXT("PlaytestMapResize: -size=%d слишком мал (минимум 8)"), NewSize);
        return 1;
    }

    const TCHAR* AssetPath = TEXT("/Game/Maps/L_Playtest");
    UPackage* Package = LoadPackage(nullptr, AssetPath, LOAD_None);
    if (!Package)
    {
        UE_LOG(LogTemp, Error, TEXT("PlaytestMapResize: не удалось загрузить %s"), AssetPath);
        return 1;
    }

    UWorld* World = UWorld::FindWorldInPackage(Package);
    if (!World)
    {
        UE_LOG(LogTemp, Error, TEXT("PlaytestMapResize: в пакете %s нет мира"), AssetPath);
        return 1;
    }
    // Обходим PersistentLevel->Actors НАПРЯМУЮ, а не через TActorIterator:
    // мир загружен из пакета, но не инициализирован (AddToWorld не
    // выполнялся), поэтому World->Levels пуст и итератор молча не нашёл бы
    // ни одного актора -- коммандлет отчитался бы об успехе, ничего не
    // сделав. Поймано разбором ДО первого запуска, на чужой карте с ручной
    // работой такой тихий промах обошёлся бы дорого.
    ULevel* Level = World->PersistentLevel;
    if (!Level)
    {
        UE_LOG(LogTemp, Error, TEXT("PlaytestMapResize: у мира нет PersistentLevel"));
        return 1;
    }

    AGridWorldManager* Manager = nullptr;
    for (AActor* Actor : Level->Actors)
    {
        if (AGridWorldManager* Found = Cast<AGridWorldManager>(Actor))
        {
            Manager = Found;
            break;
        }
    }
    if (!Manager)
    {
        UE_LOG(LogTemp, Error, TEXT("PlaytestMapResize: на карте нет AGridWorldManager"));
        return 1;
    }

    const int32 OldX = Manager->GridSizeX;
    const int32 OldY = Manager->GridSizeY;
    const float CellSize = Manager->CellSize;

    Manager->GridSizeX = NewSize;
    Manager->GridSizeY = NewSize;
    Manager->Modify();

    // Полный размах мира в юнитах. Клетки занимают [0, (N-1)*CellSize],
    // края регионов берём с запасом в половину клетки с каждой стороны,
    // чтобы крайние клетки попадали внутрь без зазора (тот же довод, что у
    // -100/+2100 в PlaytestMapCreateCommandlet).
    const float Margin = CellSize * 0.5f;
    const float MinEdge = -Margin;
    const float MaxEdge = (NewSize - 1) * CellSize + Margin;

    const TArray<EBiomeType> Order = BandOrder();
    const float BandHeight = (MaxEdge - MinEdge) / static_cast<float>(Order.Num());

    // Старые регионы сносим целиком и раскладываем заново: они полностью
    // генерируемые, ручного авторства в них нет, а подгонять существующие
    // сплайны по одному значило бы зависеть от того, что их ровно восемь и
    // они в том же порядке.
    int32 Removed = 0;
    TArray<ABiomeRegionVolume*> Stale;
    for (AActor* Actor : Level->Actors)
    {
        if (ABiomeRegionVolume* Region = Cast<ABiomeRegionVolume>(Actor))
        {
            Stale.Add(Region);
        }
    }
    for (ABiomeRegionVolume* Region : Stale)
    {
        if (Region)
        {
            World->DestroyActor(Region);
            ++Removed;
        }
    }

    int32 Created = 0;
    for (int32 i = 0; i < Order.Num(); ++i)
    {
        ABiomeRegionVolume* Region = World->SpawnActor<ABiomeRegionVolume>();
        if (!Region) continue;

        Region->Biome = Order[i];
        const float MinY = MinEdge + BandHeight * i;
        const float MaxY = MinEdge + BandHeight * (i + 1);
        const TArray<FVector> Corners = {
            FVector(MinEdge, MinY, 0.0f),
            FVector(MaxEdge, MinY, 0.0f),
            FVector(MaxEdge, MaxY, 0.0f),
            FVector(MinEdge, MaxY, 0.0f),
        };
        Region->SetSplinePointsWorld(Corners);
        ++Created;

        UE_LOG(LogTemp, Display, TEXT("  полоса %-16s Y [%.0f .. %.0f]"),
            *FBiomeDefaults::BiomeTypeToName(Order[i]).ToString(), MinY, MaxY);
    }

    Package->MarkPackageDirty();
    const FString PackageFileName = FPackageName::LongPackageNameToFilename(
        Package->GetName(), FPackageName::GetMapPackageExtension());

    FSavePackageArgs SaveArgs;
    SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
    SaveArgs.SaveFlags = SAVE_NoError;
    if (!UPackage::SavePackage(Package, World, *PackageFileName, SaveArgs))
    {
        UE_LOG(LogTemp, Error, TEXT("PlaytestMapResize: не удалось сохранить %s"), *PackageFileName);
        return 1;
    }

    const float WorldMetres = NewSize * CellSize / 100.0f;
    UE_LOG(LogTemp, Display, TEXT("PlaytestMapResize: сетка %dx%d -> %dx%d (мир %.0f м при CellSize=%.0f), полос снесено %d, создано %d"),
        OldX, OldY, NewSize, NewSize, WorldMetres, CellSize, Removed, Created);
    UE_LOG(LogTemp, Display, TEXT("ЛАНДШАФТ НЕ ТРОНУТ -- клетки за его краем получат высоту 0. Довести ландшафт до нового размера нужно в редакторе."));
    return 0;
}
