// GridWorldManagerTick.cpp
#include "Core/World/GridWorldManager.h"
#include "ProjectHerbalist.h"
#include "DrawDebugHelpers.h"
#include "Core/Simulation/Public/CommandTypes.h"
#include "Core/Simulation/Public/SnapshotService.h"
#include "Core/BiomeGraph/BiomeGraphSubsystem.h"

void AGridWorldManager::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (UBiomeGraphSubsystem* Graph = GetWorld()->GetSubsystem<UBiomeGraphSubsystem>())
    {
        Graph->StepSimulation(DeltaTime);
    }

    // Восстановление стресса
    if (bEnableRecovery && StressCells.Num() > 0)
    {
        TArray<int32> StressIndices = StressCells.Array();
        for (int32 Idx : StressIndices)
        {
            int32 X = Idx % GridSizeX;
            int32 Y = Idx / GridSizeX;
            FGridCell* Cell = GetCell(X, Y);
            if (!Cell) continue;

            float OldStress = Cell->HarvestStress;
            Cell->HarvestStress = FMath::Max(0.0f, Cell->HarvestStress - HarvestStressDecayRate * DeltaTime);
            if (!FMath::IsNearlyEqual(OldStress, Cell->HarvestStress, 1e-4f))
            {
                MarkDirty(X, Y);
                RecalculateDistortionFromHarvestStress(*Cell);
            }
            if (Cell->HarvestStress <= 0.0f)
            {
                StressCells.Remove(Idx);
            }
        }
    }

    // Интерполяция грязных клеток
    if (DirtyCells.Num() > 0)
    {
        TArray<int32> DirtyIndices = DirtyCells.Array();
        for (int32 Idx : DirtyIndices)
        {
            int32 X = Idx % GridSizeX;
            int32 Y = Idx / GridSizeX;
            FGridCell* Cell = GetCell(X, Y);
            if (!Cell) continue;

            bool bStateNear =
                FMath::IsNearlyEqual(Cell->State.Magnitude, Cell->TargetState.Magnitude, 0.001f) &&
                FMath::IsNearlyEqual(Cell->State.Meta.Distortion, Cell->TargetState.Meta.Distortion, 0.001f) &&
                FMath::IsNearlyEqual(Cell->State.Meta.Stability, Cell->TargetState.Meta.Stability, 0.001f) &&
                FMath::IsNearlyEqual(Cell->State.Meta.Purity, Cell->TargetState.Meta.Purity, 0.001f) &&
                FMath::IsNearlyEqual(Cell->State.Meta.Potency, Cell->TargetState.Meta.Potency, 0.001f) &&
                FMath::IsNearlyEqual(Cell->State.Meta.Resonance, Cell->TargetState.Meta.Resonance, 0.001f) &&
                FMath::IsNearlyEqual(Cell->State.Meta.Corruption, Cell->TargetState.Meta.Corruption, 0.001f) &&
                FMath::IsNearlyEqual(Cell->State.Direction.Body, Cell->TargetState.Direction.Body, 0.001f) &&
                FMath::IsNearlyEqual(Cell->State.Direction.Mind, Cell->TargetState.Direction.Mind, 0.001f) &&
                FMath::IsNearlyEqual(Cell->State.Direction.Spirit, Cell->TargetState.Direction.Spirit, 0.001f) &&
                FMath::IsNearlyEqual(Cell->State.Direction.Nature, Cell->TargetState.Direction.Nature, 0.001f);

            if (!bStateNear)
            {
                InterpolateCell(*Cell, DeltaTime);
                UpdateMemory(Cell->Memory, Cell->State, 0.05f * DeltaTime);
            }
            else
            {
                Cell->State = Cell->TargetState;
                UpdateMemory(Cell->Memory, Cell->State, 0.1f);
                DirtyCells.Remove(Idx);
            }
        }
    }

    // Управление активностью тика
    bool bShouldTick = (DirtyCells.Num() > 0) || (StressCells.Num() > 0);
    UBiomeGraphSubsystem* Graph = GetWorld()->GetSubsystem<UBiomeGraphSubsystem>();
    if (Graph && Graph->IsInitialized()) bShouldTick = true;

    if (bInterpolationActive != bShouldTick)
    {
        bInterpolationActive = bShouldTick;
        SetActorTickEnabled(bShouldTick);
    }
	
		if (bUseNewPipeline)
	{
		FCommandGraph CmdGraph = Simulation::FSnapshotService::BuildCommandGraph(PendingCommands);
		PendingCommands.Empty();
		Simulation::FSnapshotService::ExecuteTick(CmdGraph);
	}

#if WITH_EDITOR
    if (bEnableDebugDraw)
    {
        DrawGridDebug();
        DrawBiomeGraphDebug();
    }
#endif
}

void AGridWorldManager::InterpolateCell(FGridCell& Cell, float DeltaTime)
{
    // без изменений
    FRealState& Cur = Cell.State;
    const FRealState& Target = Cell.TargetState;

    Cur.Magnitude = FMath::FInterpTo(Cur.Magnitude, Target.Magnitude, DeltaTime, StateInterpolationSpeed);
    Cur.Direction.Body = FMath::FInterpTo(Cur.Direction.Body, Target.Direction.Body, DeltaTime, StateInterpolationSpeed);
    Cur.Direction.Mind = FMath::FInterpTo(Cur.Direction.Mind, Target.Direction.Mind, DeltaTime, StateInterpolationSpeed);
    Cur.Direction.Spirit = FMath::FInterpTo(Cur.Direction.Spirit, Target.Direction.Spirit, DeltaTime, StateInterpolationSpeed);
    Cur.Direction.Nature = FMath::FInterpTo(Cur.Direction.Nature, Target.Direction.Nature, DeltaTime, StateInterpolationSpeed);
    Cur.Meta.Distortion = FMath::FInterpTo(Cur.Meta.Distortion, Target.Meta.Distortion, DeltaTime, StateInterpolationSpeed);
    Cur.Meta.Stability = FMath::FInterpTo(Cur.Meta.Stability, Target.Meta.Stability, DeltaTime, StateInterpolationSpeed);
    Cur.Meta.Purity = FMath::FInterpTo(Cur.Meta.Purity, Target.Meta.Purity, DeltaTime, StateInterpolationSpeed);
    Cur.Meta.Potency = FMath::FInterpTo(Cur.Meta.Potency, Target.Meta.Potency, DeltaTime, StateInterpolationSpeed);
    Cur.Meta.Resonance = FMath::FInterpTo(Cur.Meta.Resonance, Target.Meta.Resonance, DeltaTime, StateInterpolationSpeed);
    Cur.Meta.Corruption = FMath::FInterpTo(Cur.Meta.Corruption, Target.Meta.Corruption, DeltaTime, StateInterpolationSpeed);

    Cur.Direction.NormalizeSum();
}