// Source/ProjectHerbalistTests/Private/Tests/ResourceDistanceFadeTest.cpp
//
// Затухание ресурсов у границы материализации (2026-09-12). После правки
// материализации ресурсы стали исчезать раньше ландшафта: они стоят только в
// радиусе симуляции, землю World Partition держит дальше. Поднимать радиус
// ресурсов до дальности земли дорого (при клетке 1 м -- сотни тысяч
// акторов), поэтому решено сделать границу незаметной: материалы ресурсов
// сжимают их к основанию на подходе к ней.
//
// Проверяется то, что делает C++: откуда берётся полоса, помечен ли ресурс и
// доходит ли полоса до материала. Сам шейдер -- в редакторе, глазами.

#include "Core/World/GridWorldManager.h"
#include "Core/Config/HerbalistSettings.h"
#include "Core/Resources/AHerbalistResourceActor.h"
#include "Components/StaticMeshComponent.h"
#include "Kismet/KismetMaterialLibrary.h"
#include "Materials/MaterialParameterCollection.h"
#include "Misc/AutomationTest.h"
#include "Editor.h"
#include "Engine/World.h"

#if WITH_AUTOMATION_TESTS && WITH_EDITOR

#include "TestWorldHelpers.h"

namespace
{
    // Тот же приём, что FScopedChunkSettings в GridStreamingTest.cpp: настройки --
    // синглтон на процесс, после теста обязаны вернуться.
    struct FScopedResourceFadeSettings
    {
        UHerbalistSettings* Settings;
        float SavedRadiusMeters;
        int32 SavedChunkSize;

        FScopedResourceFadeSettings(float RadiusMeters, int32 ChunkSize)
            : Settings(GetMutableDefault<UHerbalistSettings>())
        {
            SavedRadiusMeters = Settings->ActiveSimulationRadiusMeters;
            SavedChunkSize = Settings->ChunkSizeInCells;
            Settings->ActiveSimulationRadiusMeters = RadiusMeters;
            Settings->ChunkSizeInCells = ChunkSize;
        }

        ~FScopedResourceFadeSettings()
        {
            Settings->ActiveSimulationRadiusMeters = SavedRadiusMeters;
            Settings->ChunkSizeInCells = SavedChunkSize;
        }
    };
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistResourceFade_FadeEndsWhereResourcesMayVanish,
    "Herbalist.ResourceFade.FadeEndsWhereResourcesMayVanish",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistResourceFade_FadeEndsWhereResourcesMayVanish::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;
    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;

    // Размер клетки -- из самой сетки, а не константой теста.
    const float CellSpan = static_cast<float>(Manager->GetCellWorldPositionFlat(1, 0).X - Manager->GetCellWorldPositionFlat(0, 0).X);

    {
        FScopedResourceFadeSettings Scoped(/*RadiusMeters=*/100.0f, /*ChunkSize=*/8);
        const float ChunkSpan = CellSpan * 8.0f;
        float Start = 0.0f;
        float End = 0.0f;
        Manager->GetResourceFadeFrame(Start, End);

        TestEqual(TEXT("Конец полосы -- радиус в чанках x размер чанка: ближе ресурс не исчезнет"),
            End, Manager->GetActiveRadiusInChunks() * ChunkSpan, 0.01f);
        TestEqual(TEXT("Ширина полосы -- один чанк: шаг, которым двигается граница"), End - Start, ChunkSpan, 0.01f);
        if (FMath::IsNearlyEqual(CellSpan, 100.0f))
        {
            TestEqual(TEXT("При клетке 1 м -- 88..96 м"), Start, 8800.0f, 0.01f);
            TestEqual(TEXT("При клетке 1 м -- конец на 96 м"), End, 9600.0f, 0.01f);
        }
    }

    {
        FScopedResourceFadeSettings Scoped(/*RadiusMeters=*/-1.0f, /*ChunkSize=*/8);
        float Start = 0.0f;
        float End = 0.0f;
        Manager->GetResourceFadeFrame(Start, End);
        TestTrue(TEXT("Стриминг выключен -- полоса дальше любого мира"), Start >= 1.0e8f);
        TestTrue(TEXT("...и начало не совпадает с концом (SmoothStep не делит на ноль)"), End > Start);
    }

    {
        FScopedResourceFadeSettings Scoped(/*RadiusMeters=*/0.0f, /*ChunkSize=*/8);
        float Start = 0.0f;
        float End = 0.0f;
        Manager->GetResourceFadeFrame(Start, End);
        TestEqual(TEXT("Радиус 0 -- граница может быть прямо у игрока"), End, 0.0f, 0.01f);
        TestTrue(TEXT("...и полоса не вырождается"), Start < End);
    }

    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistResourceFade_ResourceActorIsMarkedForFade,
    "Herbalist.ResourceFade.ResourceActorIsMarkedForFade",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistResourceFade_ResourceActorIsMarkedForFade::RunTest(const FString& Parameters)
{
    // Мастер-материалы ресурсов общие с декором паков; сжимается только то,
    // у чего стоит метка. Проверяется рантайм-значение -- его и берёт рендер.
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AHerbalistResourceActor* Resource = World->SpawnActor<AHerbalistResourceActor>();
    if (!TestNotNull(TEXT("Resource actor spawned"), Resource)) return false;

    const UStaticMeshComponent* Mesh = Resource->FindComponentByClass<UStaticMeshComponent>();
    if (TestNotNull(TEXT("Resource has a mesh component"), Mesh))
    {
        const TArray<float>& Data = Mesh->GetCustomPrimitiveData().Data;
        const int32 Index = AHerbalistResourceActor::DistanceFadeDataIndex;
        TestTrue(TEXT("Метка затухания стоит у заспавненного ресурса"), Data.IsValidIndex(Index) && Data[Index] == 1.0f);
    }

    Resource->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistResourceFade_FrameReachesMaterialCollection,
    "Herbalist.ResourceFade.FrameReachesMaterialCollection",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistResourceFade_FrameReachesMaterialCollection::RunTest(const FString& Parameters)
{
    // Без параметра в коллекции запись молча теряется -- тест ловит и это
    // (параметр заводит -run=WorldStateMapSetup).
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;
    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;

    UMaterialParameterCollection* Collection = LoadObject<UMaterialParameterCollection>(nullptr,
        TEXT("/Game/Materials/MPC_WorldStateFields.MPC_WorldStateFields"));
    if (!TestNotNull(TEXT("MPC_WorldStateFields exists"), Collection)) { Manager->Destroy(); return false; }

    {
        FScopedResourceFadeSettings Scoped(/*RadiusMeters=*/100.0f, /*ChunkSize=*/8);
        Manager->WorldStateFrameCollection = Collection;
        Manager->UpdateWorldStateMap();

        float Start = 0.0f;
        float End = 0.0f;
        Manager->GetResourceFadeFrame(Start, End);
        const FLinearColor Frame = UKismetMaterialLibrary::GetVectorParameterValue(World, Collection, TEXT("ResourceFadeFrame"));
        TestEqual(TEXT("R -- начало полосы"), Frame.R, Start, 0.01f);
        TestEqual(TEXT("G -- конец полосы"), Frame.G, End, 0.01f);
    }

    Manager->Destroy();
    return true;
}

#endif // WITH_AUTOMATION_TESTS && WITH_EDITOR
