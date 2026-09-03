// TestWorldHelpers.h
//
// SpawnAndBeginPlay(UWorld*) — общий хелпер тестов бестиария/циклов/капищ.
// Раньше был продублирован одинаковым телом в собственном anonymous namespace
// каждого из 15 файлов Tests/*.cpp. Пока ProjectHerbalistTests собирался
// файл-за-файлом, дубликаты в разных TU были безобидны. Как только UBT
// перешёл на unity-сборку модуля (один Module.ProjectHerbalistTests.cpp через
// #include всех файлов — сработало добавлением новых файлов в этой сессии,
// 2026-08-29), одинаковые тела внутри ОДНОЙ единицы трансляции стали настоящим
// ODR-нарушением (MSVC C2084 "already has a body"). Один общий inline-хелпер,
// не 15 копий — тот же принцип, что и везде в проекте против дублирования.
// Anonymous namespace (не именованный) — даже если когда-нибудь снова
// появится TU, куда этот заголовок не попадёт через unity-слияние (например,
// точечная пересборка одного файла Live Coding), у каждой единицы трансляции
// будет своя приватная копия с internal linkage, без риска коллизии на этапе
// линковки.
#pragma once

#include "CoreMinimal.h"
#include "Core/World/GridWorldManager.h"
#include "Core/World/BiomeRegionVolume.h"
#include "Core/BiomeGraph/BiomeGraphSubsystem.h"
#include "Core/BiomeGraph/BiomeGraphAsset.h"
#include "Components/SplineComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"

namespace
{
    // Прямой вызов виртуального BeginPlay() пропускает состояние-машину движка
    // (AActor::ActorHasBegunPlay выставляется в DispatchBeginPlay ДО вызова
    // BeginPlay) и валит ensure "ActorHasBegunPlay == BeginningPlay". Правильный
    // публичный API именно для этого случая — динамически заспавненный актор вне
    // обычного старта уровня/PIE — DispatchBeginPlay(), не голый BeginPlay().
    //
    // KeepRegions (2026-09-02, найдено при добавлении PCG-спавна) — тесты
    // выполняются в персистентном editor-мире, который теперь грузит
    // EditorStartupMap (L_TestDev) с настоящими, авторски расставленными
    // ABiomeRegionVolume. Без явной изоляции InitializeCells каждого теста
    // видел бы ИХ через TActorIterator наравне со своими собственными —
    // 29 тестов, полагавшихся на "чистый" блочный фолбэк со всеми 8
    // биомами гарантированно доступными (SeedLegendaryAnchors/
    // SeedTestLandmarks ищут клетку КАЖДОГО биома), падали молча, как
    // только уровень получил реальный контент. Уничтожаем в мире ВСЕ
    // ABiomeRegionVolume, кроме явно переданных вызывающим тестом (его
    // собственные, только что заспавненные) — тот же принцип, что и у
    // остальных хелперов этого файла: тест получает предсказуемый мир,
    // не то, что случайно лежит на уровне. Region->Destroy() — только в
    // этом одноразовом headless-процессе, .umap на диске не трогается.
    AGridWorldManager* SpawnAndBeginPlay(UWorld* World, const TArray<AActor*>& KeepRegions = {})
    {
        if (!World) return nullptr;

        for (TActorIterator<ABiomeRegionVolume> It(World); It; ++It)
        {
            ABiomeRegionVolume* Region = *It;
            if (Region && !KeepRegions.Contains(Region))
            {
                Region->Destroy();
            }
        }

        // Ровно тот же довод, что и у KeepRegions выше, но про сам менеджер
        // (2026-09-03, найдено на ShrineActorTest): в персистентном
        // editor-мире живёт настоящий, расставленный на L_TestDev
        // BP_GridWorldManager, плюс менеджеры предыдущих тестов, чей
        // Destroy() отложен движком. Пока тест дёргал только свой указатель,
        // это было безобидно. Как только в тестах появились АКТОРЫ, которые
        // сами ищут менеджер через TActorIterator (AShrineActor,
        // AAlchemyTableActor, AHerbalistResourceActor), они стали находить
        // чужой менеджер вместо только что заспавненного — и регистрировать
        // капище/Домового в нём, из-за чего тест не видел у себя ничего.
        // Чистим мир от прежних менеджеров ПЕРЕД спавном своего.
        for (TActorIterator<AGridWorldManager> It(World); It; ++It)
        {
            if (AGridWorldManager* Stale = *It)
            {
                Stale->Destroy();
            }
        }

        AGridWorldManager* Manager = World->SpawnActor<AGridWorldManager>();
        if (Manager)
        {
            Manager->DispatchBeginPlay();
        }
        return Manager;
    }

    // Инициализирует UBiomeGraphSubsystem боевым DA_BiomeGraph -- тот же путь,
    // что уже LegendaryEntityTest.cpp/BiomeGraphIntegrationTest.cpp использовали
    // каждый в своей копии до этого рефакторинга (2026-09-02, тот же ODR-урок,
    // что уже вынес SpawnAndBeginPlay сюда -- unity-сборка модуля склеивает все
    // Tests/*.cpp в одну единицу трансляции, два одноимённых InitGraph в двух
    // анонимных namespace разных файлов там же стали бы редефиницией). Вызывающая
    // сторона отвечает за Graph->Deinitialize() в конце теста.
    UBiomeGraphSubsystem* InitGraph(UWorld* World)
    {
        if (!World) return nullptr;
        UBiomeGraphSubsystem* Graph = World->GetSubsystem<UBiomeGraphSubsystem>();
        if (!Graph) return nullptr;
        UBiomeGraphAsset* Asset = LoadObject<UBiomeGraphAsset>(nullptr, TEXT("/Game/Data/DA_BiomeGraph"));
        if (!Asset) return nullptr;
        Graph->InitializeFromAsset(Asset);
        return Graph;
    }

    // Прямоугольный ABiomeRegionVolume, покрывающий заданный прямоугольник
    // мировых координат -- вынесено сюда 2026-09-02 (тот же ODR-довод, что
    // и у InitGraph выше): второй потребитель (GridWorldManagerSpawnPositionTest.cpp)
    // появился следом за первым (GridWorldManagerBiomeRegionTest.cpp).
    // Точки — Linear, не сглаженный Catmull-Rom по умолчанию: тестам нужна
    // предсказуемая прямоугольная форма, не то, что выглядело бы органично
    // при настоящем авторстве в редакторе. ВАЖНО: вызывающая сторона должна
    // заспавнить регион(ы) ДО AGridWorldManager (InitializeCells читает
    // TActorIterator<ABiomeRegionVolume> внутри BeginPlay, порядок важен).
    ABiomeRegionVolume* SpawnRegionCoveringWorldRect(UWorld* World, EBiomeType Biome,
        float MinX, float MinY, float MaxX, float MaxY)
    {
        if (!World) return nullptr;
        ABiomeRegionVolume* Region = World->SpawnActor<ABiomeRegionVolume>();
        if (!Region) return nullptr;
        Region->Biome = Biome;

        if (USplineComponent* Spline = Region->FindComponentByClass<USplineComponent>())
        {
            const TArray<FVector> Corners = {
                FVector(MinX, MinY, 0.0f), FVector(MaxX, MinY, 0.0f),
                FVector(MaxX, MaxY, 0.0f), FVector(MinX, MaxY, 0.0f),
            };
            Spline->SetSplinePoints(Corners, ESplineCoordinateSpace::World, false);
            for (int32 i = 0; i < Corners.Num(); ++i)
            {
                Spline->SetSplinePointType(i, ESplinePointType::Linear, false);
            }
            Spline->SetClosedLoop(true, false);
            Spline->UpdateSpline();
        }
        Region->UpdateCachedPoints();
        return Region;
    }
}
