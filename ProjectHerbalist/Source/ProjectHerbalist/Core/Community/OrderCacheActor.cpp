// OrderCacheActor.cpp
#include "Core/Community/OrderCacheActor.h"
#include "Core/World/GridWorldManager.h"
#include "Core/Community/OrderTypes.h"
#include "Core/Config/HerbalistSettings.h"
#include "Core/Inventory/HerbalistInventoryComponent.h"
#include "HerbalistLogChannels.h"
#include "Player/HerbalistPlayerController.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SphereComponent.h"
#include "EngineUtils.h"

AOrderCacheActor::AOrderCacheActor()
{
    PrimaryActorTick.bCanEverTick = false;

    MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
    RootComponent = MeshComponent;
    MeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    MeshComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
    MeshComponent->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);

    // Та же сфера взаимодействия, что у кургана: тайник даёт себя выделить
    // чуть раньше самого меша -- пока меша нет вовсе, только по ней.
    InteractionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("InteractionSphere"));
    InteractionSphere->SetupAttachment(RootComponent);
    InteractionSphere->SetSphereRadius(60.0f);
    InteractionSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    InteractionSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
    InteractionSphere->SetCollisionResponseToChannel(ECC_GameTraceChannel1, ECR_Block);
    InteractionSphere->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
}

void AOrderCacheActor::BeginPlay()
{
    Super::BeginPlay();
    for (TActorIterator<AGridWorldManager> It(GetWorld()); It; ++It)
    {
        RegisteredManager = *It;
        It->RegisterOrderCache(this);
        break;
    }
}

void AOrderCacheActor::EndPlay(const EEndPlayReason::Type Reason)
{
    if (AGridWorldManager* Manager = RegisteredManager.Get())
    {
        Manager->UnregisterOrderCache(this);
    }
    Super::EndPlay(Reason);
}

void AOrderCacheActor::OnInteract_Implementation(AHerbalistPlayerController* PC)
{
    UE_LOG(LogHerbalistWorld, Log, TEXT("[Orders] Тайник: зелье кладут рукой"));
}

bool AOrderCacheActor::ReceiveHeldItem(AHerbalistPlayerController* PC, int32 InventoryIndex)
{
    if (!PC || !PC->InventoryComponent || !PC->InventoryComponent->GetItems().IsValidIndex(InventoryIndex)) return false;
    const FInventoryItem Potion = PC->InventoryComponent->GetItems()[InventoryIndex];
    if (!HerbalistOrders::IsDeliverable(Potion)) return false;

    AGridWorldManager* Manager = PC->FindWorldManager();
    if (!Manager) return true;

    // Положить можно, только дотянувшись: та же досягаемость, что у правила
    // «отдают у тайника». Без пешки (автотесты) взгляд -- уже присутствие.
    const UHerbalistSettings* Settings = GetHerbalistSettings();
    const float ReachCm = (Settings ? Settings->OrderCacheReachMeters : 3.0f) * 100.0f;
    if (const APawn* Pawn = PC->GetPawn())
    {
        if (FVector::DistSquared(Pawn->GetActorLocation(), GetActorLocation()) > FMath::Square(ReachCm))
        {
            UE_LOG(LogHerbalistWorld, Log, TEXT("[Orders] До тайника не дотянуться -- подойдите ближе"));
            return true;
        }
    }

    const int32 Number = Manager->FindMostUrgentOpenOrder();
    // Отдаётся то, что лежит, -- настоящее состояние, не то, что видит травник.
    if (Number == 0 || !Manager->DeliverOrder(Number, Potion))
    {
        UE_LOG(LogHerbalistWorld, Log, TEXT("[Orders] Открытых заказов нет -- зелье осталось в котомке"));
        return true;
    }
    PC->InventoryComponent->RemoveItem(InventoryIndex, 1);
    return true;
}
