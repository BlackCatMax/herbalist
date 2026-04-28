// Core/Simulation/Public/TraceTypes.h
#pragma once

#include "CoreMinimal.h"
#include "SnapshotTypes.h"
#include "DeltaTypes.h"
#include "CommandTypes.h"
#include "ProjectHerbalist.h"

// ============================================================================
// КАДР ТРАССИРОВКИ
// ============================================================================

struct FTraceFrame
{
    int32 TickID = 0;
    FWorldSnapshot WorldSnapshot;
    TArray<FCommandEntry> Commands;
    FStateDelta GeneratedDelta;    // дельта, полученная при выполнении
};

// ============================================================================
// КОЛЬЦЕВОЙ БУФЕР ТРАССИРОВКИ
// ============================================================================

struct FTraceRingBuffer
{
    static constexpr int32 MaxFrames = 128;

    TArray<FTraceFrame> Frames;
    int32 WriteIndex = 0;

    void Record(int32 TickID, const FWorldSnapshot& WorldSnap, const TArray<FCommandEntry>& Commands, const FStateDelta& Delta)
    {
        if (Frames.Num() < MaxFrames)
        {
            Frames.AddDefaulted();
        }

        FTraceFrame& Frame = Frames[WriteIndex % MaxFrames];
        Frame.TickID         = TickID;
        Frame.WorldSnapshot  = WorldSnap;
        Frame.Commands       = Commands;
        Frame.GeneratedDelta  = Delta;
        WriteIndex++;
    }

    const FTraceFrame* GetLastFrame() const
    {
        if (WriteIndex == 0) return nullptr;
        return &Frames[(WriteIndex - 1) % MaxFrames];
    }

    void DumpToLog() const
    {
        const int32 Count = FMath::Min(WriteIndex, MaxFrames);
        UE_LOG(LogHerbalist, Log, TEXT("=== TRACE DUMP (%d frames) ==="), Count);
        for (int32 i = 0; i < Count; ++i)
        {
            const FTraceFrame& Frame = Frames[i];
            UE_LOG(LogHerbalist, Log, TEXT("Tick %d: %d commands, %d world changes, %d inventory ops"),
                Frame.TickID,
                Frame.Commands.Num(),
                Frame.GeneratedDelta.WorldChanges.Num(),
                Frame.GeneratedDelta.InventoryOps.Num());
        }
    }
};