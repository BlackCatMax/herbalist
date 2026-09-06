// LegendaryEntityActor.cpp
#include "Core/Entities/LegendaryEntityActor.h"
#include "Core/World/GridWorldManager.h"
#include "Core/Entities/ArtifactTypes.h"

void ALegendaryEntityActor::BeginPlay()
{
    Super::BeginPlay();
    OnManifestationBurst();
}

bool ALegendaryEntityActor::WasAcquiredViaDeception(FName ArtifactID, bool& bOutFound) const
{
    bOutFound = false;
    const AGridWorldManager* Manager = WorldManagerRef.Get();
    if (!Manager) return false;

    for (const FAcquiredArtifact& Acquired : Manager->GetAcquiredArtifacts())
    {
        if (Acquired.ArtifactID == ArtifactID)
        {
            bOutFound = true;
            return Acquired.bAcquiredViaDeception;
        }
    }
    return false;
}
