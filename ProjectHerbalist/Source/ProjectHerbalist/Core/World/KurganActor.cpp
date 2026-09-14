// KurganActor.cpp
#include "Core/World/KurganActor.h"
#include "Core/World/GridWorldManager.h"
#include "Core/Subsystems/IngredientRegistrySubsystem.h"
#include "Player/HerbalistPlayerController.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SphereComponent.h"
#include "HerbalistLogChannels.h"

AKurganActor::AKurganActor()
{
    PrimaryActorTick.bCanEverTick = false;

    MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
    RootComponent = MeshComponent;
    MeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    MeshComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
    MeshComponent->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);

    // Та же радиус-сфера взаимодействия, что уже AMemoryFragmentActor --
    // Interact() бьёт трассой из камеры, сфера нужна только затем, чтобы
    // курган давал себя выделить чуть раньше самого меша.
    InteractionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("InteractionSphere"));
    InteractionSphere->SetupAttachment(RootComponent);
    InteractionSphere->SetSphereRadius(60.0f);
    InteractionSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    InteractionSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
    InteractionSphere->SetCollisionResponseToChannel(ECC_GameTraceChannel1, ECR_Block);
    InteractionSphere->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
}

void AKurganActor::Init(AGridWorldManager* InWorldManager, int32 InGridX, int32 InGridY, FName InGrantedIngredientID)
{
    WorldManager = InWorldManager;
    GridX = InGridX;
    GridY = InGridY;
    GrantedIngredientID = InGrantedIngredientID;
}

void AKurganActor::OnInteract_Implementation(AHerbalistPlayerController* PC)
{
    if (bLooted || !PC || !WorldManager) return;

    const FIntPoint Cell(GridX, GridY);
    const FName* PendingReward = WorldManager->GetKurganSites().Find(Cell);
    if (!PendingReward)
    {
        // Уже разграблен где-то ещё (например, вторым актором на той же
        // клетке в теории) -- тот же честный no-op, что и остальные
        // идемпотентные Register*-методы проекта.
        return;
    }
    const FName Granted = *PendingReward;   // LootKurgan удалит запись, указатель повиснет

    // Резолв State через IngredientRegistrySubsystem -- тот же приём, что
    // раньше жил в AHerbalistPlayerController::LootKurgan (перенесён сюда
    // целиком вместе с самим подбором, 2026-09-06). Недоступен в Editor-мире
    // автотестов -- без реестра предмет всё равно попадает в инвентарь
    // (голый FRealState()), тот же класс пробела, что у остальных резолвов
    // по имени в этом проекте (см. ROADMAP.md).
    FInventoryItem Reward;
    Reward.IngredientID = Granted;
    Reward.Count = 1;
    Reward.bSubjectToDecay = false;
    if (UGameInstance* GameInstance = PC->GetGameInstance())
    {
        if (UIngredientRegistrySubsystem* IngredientSubsystem = GameInstance->GetSubsystem<UIngredientRegistrySubsystem>())
        {
            if (const FIngredientTableRow* Row = IngredientSubsystem->GetRow(Granted))
            {
                Reward.State = Row->BaseState;
            }
        }
    }

    // Место в сумке -- ДО разграбления (2026-09-14): LootKurgan снимает курган
    // навсегда, а AddItem при полной сумке отказывал молча -- Костяной нож или
    // Серебряный оберег пропадали вместе с курганом.
    if (!PC->InventoryComponent || PC->InventoryComponent->GetAvailableCapacityFor(Reward) < Reward.Count)
    {
        UE_LOG(LogHerbalistWorld, Warning, TEXT("[Kurgan] Сумка полна: '%s' некуда положить, курган (%d,%d) не тронут"),
            *Granted.ToString(), GridX, GridY);
        return;
    }

    FName Looted;
    if (!WorldManager->LootKurgan(Cell, Looted))
    {
        return;
    }
    bLooted = true;

    PC->InventoryComponent->AddItem(Reward, Reward.Count);

    UE_LOG(LogHerbalistWorld, Log, TEXT("[Kurgan] Picked up '%s' at (%d,%d)"), *Granted.ToString(), GridX, GridY);
    Destroy();
}
