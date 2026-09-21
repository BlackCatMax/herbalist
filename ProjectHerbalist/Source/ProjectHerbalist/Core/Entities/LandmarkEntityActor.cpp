// LandmarkEntityActor.cpp
#include "Core/Entities/LandmarkEntityActor.h"
#include "Core/World/GridWorldManager.h"
#include "Player/HerbalistPlayerController.h"
#include "Components/StaticMeshComponent.h"
#include "Core/Config/HerbalistSettings.h"
#include "Core/Types/HerbalistCoreMath.h"

ALandmarkEntityActor::ALandmarkEntityActor()
{
    PrimaryActorTick.bCanEverTick = true;
    // Силуэт ловит взгляд -- иначе с хозяином не заговорить (этап 5).
    // Только видимость: он не толкает и не держит.
    MeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    MeshComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
    MeshComponent->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
}

void ALandmarkEntityActor::OnInteract_Implementation(AHerbalistPlayerController* PC)
{
    // Виден -- и актор не скрыт, и сам меш видим: мигание силуэта (арт,
    // OnRespectThresholdCrossed) может гасить и то, и другое (ревью
    // 2026-09-21).
    if (PC && !IsHidden() && MeshComponent && MeshComponent->IsVisible())
    {
        PC->TalkTo(GridCell.X, GridCell.Y);
    }
}

const FEntityLandmark* ALandmarkEntityActor::GetLandmark() const
{
    AGridWorldManager* Manager = WorldManagerRef.Get();
    return Manager ? Manager->FindLandmarkAt(GridCell) : nullptr;
}

void ALandmarkEntityActor::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    const FEntityLandmark* Landmark = GetLandmark();
    if (!Landmark) return;

    const UHerbalistSettings* Settings = GetHerbalistSettings();
    const float HysteresisMargin = Settings ? Settings->EntityManifestationHysteresis : 0.05f;

    // Те же 0.5/-0.3, что уже гейтят bless/curse в UpdateEntityManifestations
    // (см. довод у класса, LandmarkEntityActor.h) -- не новые константы.
    const bool bBlessEligible = HerbalistCore::Math::PassesHysteresisThreshold(bIsCurrentlyBlessed, Landmark->Respect, 0.5f, HysteresisMargin);
    const bool bCurseEligible = HerbalistCore::Math::PassesHysteresisThreshold(bIsCurrentlyCursed, -Landmark->Respect, 0.3f, HysteresisMargin);

    if (bBlessEligible != bIsCurrentlyBlessed || bCurseEligible != bIsCurrentlyCursed)
    {
        bIsCurrentlyBlessed = bBlessEligible;
        bIsCurrentlyCursed = bCurseEligible;
        OnRespectThresholdCrossed(bBlessEligible, bCurseEligible);
    }
}
