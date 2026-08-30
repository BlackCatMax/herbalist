// LandmarkEntityActor.cpp
#include "Core/Entities/LandmarkEntityActor.h"
#include "Core/World/GridWorldManager.h"

const FEntityLandmark* ALandmarkEntityActor::GetLandmark() const
{
    AGridWorldManager* Manager = WorldManagerRef.Get();
    return Manager ? Manager->FindLandmarkAt(GridCell) : nullptr;
}
