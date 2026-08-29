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
#include "Engine/World.h"

namespace
{
    // Прямой вызов виртуального BeginPlay() пропускает состояние-машину движка
    // (AActor::ActorHasBegunPlay выставляется в DispatchBeginPlay ДО вызова
    // BeginPlay) и валит ensure "ActorHasBegunPlay == BeginningPlay". Правильный
    // публичный API именно для этого случая — динамически заспавненный актор вне
    // обычного старта уровня/PIE — DispatchBeginPlay(), не голый BeginPlay().
    AGridWorldManager* SpawnAndBeginPlay(UWorld* World)
    {
        if (!World) return nullptr;
        AGridWorldManager* Manager = World->SpawnActor<AGridWorldManager>();
        if (Manager)
        {
            Manager->DispatchBeginPlay();
        }
        return Manager;
    }
}
