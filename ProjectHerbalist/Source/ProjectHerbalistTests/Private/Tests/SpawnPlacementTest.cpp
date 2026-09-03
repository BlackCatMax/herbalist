// Source/ProjectHerbalistTests/Private/Tests/SpawnPlacementTest.cpp
//
// Размещение ресурсов в мире (2026-09-03, запрос пользователя: "трава для
// сбора может оказаться в камне или дереве"). Точка спавна теперь не просто
// "центр клетки + сдвиг": она садится на поверхность трейсом и проверяется
// на занятость. Если свободного места в клетке нет -- ресурс не ставится
// вовсе, и это правильный исход.
//
// Ландшафт из проверки занятости исключён намеренно: он под каждой точкой
// мира, и без этого занятой была бы вся сетка целиком. Тест держит и это.

#include "Core/World/GridWorldManager.h"
#include "Core/Config/HerbalistSettings.h"
#include "Misc/AutomationTest.h"
#include "Editor.h"
#include "Engine/World.h"
#include "Components/BoxComponent.h"

#if WITH_AUTOMATION_TESTS && WITH_EDITOR

#include "TestWorldHelpers.h"

namespace
{
    struct FScopedPlacementSettings
    {
        UHerbalistSettings* Settings;
        bool bSavedReject;
        float SavedClearance;
        bool bSavedTrace;

        FScopedPlacementSettings(bool bReject, float Clearance, bool bTrace)
            : Settings(GetMutableDefault<UHerbalistSettings>())
        {
            bSavedReject = Settings->bRejectOccupiedSpawnPoints;
            SavedClearance = Settings->SpawnClearanceRadius;
            bSavedTrace = Settings->bTraceSpawnToGround;
            Settings->bRejectOccupiedSpawnPoints = bReject;
            Settings->SpawnClearanceRadius = Clearance;
            Settings->bTraceSpawnToGround = bTrace;
        }
        ~FScopedPlacementSettings()
        {
            Settings->bRejectOccupiedSpawnPoints = bSavedReject;
            Settings->SpawnClearanceRadius = SavedClearance;
            Settings->bTraceSpawnToGround = bSavedTrace;
        }
    };

    // Преграда с настоящей коллизионной геометрией -- «камень», в который
    // трава лезть не должна. Именно примитив-бокс, а не AStaticMeshActor:
    // у меш-компонента без назначенного меша геометрии нет вовсе, и
    // оверлап-тесту нечего цеплять (поймано первым прогоном этого теста).
    AActor* SpawnBlocker(UWorld* World, const FVector& Location)
    {
        AActor* Blocker = World->SpawnActor<AActor>(AActor::StaticClass(), Location, FRotator::ZeroRotator);
        if (!Blocker) return nullptr;

        UBoxComponent* Box = NewObject<UBoxComponent>(Blocker);
        Box->SetBoxExtent(FVector(100.0f));
        Box->SetMobility(EComponentMobility::Movable);
        Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
        Box->SetCollisionObjectType(ECC_WorldStatic);
        Box->SetCollisionResponseToAllChannels(ECR_Block);
        Blocker->SetRootComponent(Box);
        Box->RegisterComponent();
        Blocker->SetActorLocation(Location);
        return Blocker;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistSpawnPlacement_EmptyGroundIsNotConsideredBlocked,
    "Herbalist.SpawnPlacement.EmptyGroundIsNotConsideredBlocked",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistSpawnPlacement_EmptyGroundIsNotConsideredBlocked::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;

    {
        FScopedPlacementSettings Scoped(/*bReject=*/true, /*Clearance=*/40.0f, /*bTrace=*/true);

        // Главное свойство: сам по себе ландшафт занятостью НЕ считается,
        // иначе ни одна точка мира не прошла бы проверку.
        const FVector Somewhere = Manager->GetCellWorldPosition(5, 5);
        TestFalse(TEXT("Bare ground is free"), Manager->IsSpawnPointBlocked(Somewhere));

        FRandomStream Rng(1234);
        FVector Found;
        TestTrue(TEXT("A free position is found on empty ground"),
            Manager->FindFreeSpawnPositionInCell(5, 5, 30.0f, Rng, Found));
    }

    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistSpawnPlacement_BlockerMakesThePointOccupied,
    "Herbalist.SpawnPlacement.BlockerMakesThePointOccupied",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistSpawnPlacement_BlockerMakesThePointOccupied::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;

    {
        FScopedPlacementSettings Scoped(/*bReject=*/true, /*Clearance=*/100.0f, /*bTrace=*/false);

        const FVector Point = Manager->GetCellWorldPosition(7, 7);
        TestFalse(TEXT("Point is free before the blocker appears"), Manager->IsSpawnPointBlocked(Point));

        // «Камень» ровно там, куда собирались ставить траву.
        AActor* Blocker = SpawnBlocker(World, Point + FVector(0, 0, 100.0f));
        if (!TestNotNull(TEXT("Blocker spawned"), Blocker)) { Manager->Destroy(); return false; }

        TestTrue(TEXT("The very same point is occupied once something stands there"),
            Manager->IsSpawnPointBlocked(Point));

        // И выключатель работает: с bRejectOccupiedSpawnPoints=false проверка
        // не выполняется вовсе, поведение как до этой правки.
        {
            FScopedPlacementSettings Off(/*bReject=*/false, /*Clearance=*/100.0f, /*bTrace=*/false);
            TestFalse(TEXT("With rejection disabled nothing is ever considered blocked"),
                Manager->IsSpawnPointBlocked(Point));
        }

        Blocker->Destroy();
    }

    Manager->Destroy();
    return true;
}

#endif // WITH_AUTOMATION_TESTS && WITH_EDITOR
