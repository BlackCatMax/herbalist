// MemoryFragmentActor.cpp
#include "Core/Zaryana/MemoryFragmentActor.h"
#include "Core/World/GridWorldManager.h"
#include "Player/HerbalistPlayerController.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Core/Config/HerbalistSettings.h"
#include "Core/Types/HerbalistActorLabel.h"
#include "Engine/StaticMesh.h"
#include "ProjectHerbalist.h"
#include "HerbalistLogChannels.h"

AMemoryFragmentActor::AMemoryFragmentActor()
{
    PrimaryActorTick.bCanEverTick = true;

    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

    MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
    MeshComponent->SetupAttachment(RootComponent);
    MeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    MeshComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
    MeshComponent->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);

    InteractionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("InteractionSphere"));
    InteractionSphere->SetupAttachment(RootComponent);
    InteractionSphere->SetSphereRadius(60.0f);
    InteractionSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    InteractionSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
    InteractionSphere->SetCollisionResponseToChannel(ECC_GameTraceChannel1, ECR_Block);
    InteractionSphere->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
}

void AMemoryFragmentActor::Init(FName InDefinitionID, bool bInIsFalse, float InLifetimeSeconds,
    AGridWorldManager* InWorldManager, int32 InGridX, int32 InGridY)
{
    DefinitionID = InDefinitionID;
    bIsFalse = bInIsFalse;
    RemainingLifetime = InLifetimeSeconds;
    WorldManager = InWorldManager;
    GridX = InGridX;
    GridY = InGridY;

    // Подпись для аутлайнера/выделения в PIE -- см. HerbalistActorLabel.h.
    // У фрагментов та же беда, что у бестиария: заглушка-куб одна на все
    // 12 определений. Ложный фрагмент помечается отдельно -- в игре он
    // неотличим от настоящего намеренно (§19.2), но в аутлайнере автору
    // различать их необходимо.
    SetHerbalistDebugLabel(this, FString::Printf(TEXT("Фрагмент %s%s (%d,%d)"),
        *DefinitionID.ToString(), bIsFalse ? TEXT(" [ложный]") : TEXT(""), GridX, GridY));

    // Меш-заглушка (2026-09-02) — та же логика и та же оговорка, что у
    // AHerbalistEntityActor::Init: применяется, только если у класса,
    // которым нас заспавнили, меша нет (Blueprint карточки всегда главнее).
    if (MeshComponent && !MeshComponent->GetStaticMesh())
    {
        if (const UHerbalistSettings* Settings = GetHerbalistSettings())
        {
            if (UStaticMesh* Placeholder = Settings->PlaceholderMemoryFragmentMesh.LoadSynchronous())
            {
                MeshComponent->SetStaticMesh(Placeholder);
            }
        }
    }
}

void AMemoryFragmentActor::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (bCollected) return;

    RemainingLifetime -= DeltaTime;
    if (RemainingLifetime <= 0.0f)
    {
        UE_LOG(LogHerbalistZaryana, Log, TEXT("[MemoryFragment] %s at (%d,%d) faded uncollected"),
            *DefinitionID.ToString(), GridX, GridY);
        Destroy();
    }
}

void AMemoryFragmentActor::OnInteract_Implementation(AHerbalistPlayerController* PC)
{
    if (bCollected || !PC || !WorldManager) return;
    bCollected = true;

    WorldManager->CollectMemoryFragment(DefinitionID, bIsFalse, PC, GetGridCell());
    Destroy();
}
