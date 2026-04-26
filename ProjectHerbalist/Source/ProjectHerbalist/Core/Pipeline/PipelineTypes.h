// PipelineTypes.h
#pragma once

#include "CoreMinimal.h"
#include "Core/Types/HerbalistCoreTypes.h"

struct FAggregatedState
{
    FL2Direction Dir;
    float Magnitude = 0.0f;
    FMeta Meta;

    FAggregatedState() : Magnitude(0.0f) {}
};