// LandmarkEntityActor.cpp
#include "Core/Entities/LandmarkEntityActor.h"
#include "Core/World/GridWorldManager.h"
#include "Core/Config/HerbalistSettings.h"
#include "Core/Types/HerbalistCoreMath.h"

ALandmarkEntityActor::ALandmarkEntityActor()
{
    PrimaryActorTick.bCanEverTick = true;
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
