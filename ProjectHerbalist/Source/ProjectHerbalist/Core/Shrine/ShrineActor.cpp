// ShrineActor.cpp
#include "Core/Shrine/ShrineActor.h"
#include "Core/World/GridWorldManager.h"
#include "Components/StaticMeshComponent.h"
#include "HerbalistLogChannels.h"
#include "EngineUtils.h"

AShrineActor::AShrineActor()
{
    PrimaryActorTick.bCanEverTick = false;

    MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
    RootComponent = MeshComponent;
    // Меш назначается контентом (Blueprint-наследник или прямо на
    // размещённом акторе) — тот же принцип, что у остальных акторов
    // проекта: код не хардкодит ассеты. Коллизия не нужна: капище v1 не
    // интерактивно, подношение идёт Apply-командой на его клетку, а не
    // взаимодействием с актором.
    MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
#if WITH_EDITORONLY_DATA
    // Капище регистрируется в симуляции в BeginPlay -- всегда загружено, как
    // котёл и регионы биомов (2026-09-22). Иначе дальнее капище, ни разу не
    // попавшее в радиус стриминга, не существовало бы для мира: ни поля
    // Restoration вокруг, ни учёта в условии Буяна.
    bIsSpatiallyLoaded = false;
#endif
}

void AShrineActor::BeginPlay()
{
    Super::BeginPlay();

    // Тот же поиск менеджера через TActorIterator, что уже у
    // AAlchemyTableActor/AHerbalistResourceActor — актор уровня не знает
    // менеджера заранее, а порядок BeginPlay между акторами UE не гарантирует.
    for (TActorIterator<AGridWorldManager> It(GetWorld()); It; ++It)
    {
        AGridWorldManager* WorldManager = *It;
        int32 X, Y;
        if (WorldManager->WorldPositionToCell(GetActorLocation(), X, Y))
        {
            GridCoords = FIntPoint(X, Y);
            const EShrineType TypeToUse = bResolveTypeFromCell
                ? WorldManager->ResolveShrineTypeForCell(GridCoords)
                : ShrineType;
            WorldManager->RegisterShrine(GridCoords, TypeToUse, InitialRestoration);
        }
        else
        {
            UE_LOG(LogHerbalistWorld, Warning, TEXT("AShrineActor at %s is outside the grid -- no shrine registered"), *GetActorLocation().ToString());
        }
        break;
    }
}
