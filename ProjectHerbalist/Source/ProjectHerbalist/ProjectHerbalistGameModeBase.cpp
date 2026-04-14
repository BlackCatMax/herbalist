#include "ProjectHerbalistGameModeBase.h"
#include "Core/Pipeline/HerbalistPipeline.h"

AProjectHerbalistGameModeBase::AProjectHerbalistGameModeBase()
{
}

void AProjectHerbalistGameModeBase::BeginPlay()
{
    Super::BeginPlay();

    using namespace HerbalistCore;

    // === DEFAULT VALUES ===
    FRealState LocalA;
    LocalA.Magnitude = 0.5f;
    LocalA.Direction.Body = 0.8f;
    LocalA.Direction.Mind = 0.2f;
    LocalA.Direction.Spirit = 0.1f;
    LocalA.Direction.Nature = 0.4f;

    FRealState LocalB;
    LocalB.Magnitude = 0.3f;
    LocalB.Direction.Body = 0.3f;
    LocalB.Direction.Mind = 0.7f;
    LocalB.Direction.Spirit = 0.2f;
    LocalB.Direction.Nature = 0.6f;

    FEnvironment LocalEnv;
    LocalEnv.Toxicity = 0.9f;
    LocalEnv.Fertility = 0.2f;
    LocalEnv.Moisture = 0.5f;

    FMemoryState LocalMemory;
    LocalMemory.AccumulatedDistortion = 0.9f;
    LocalMemory.StabilityMemory = 0.3f;
    LocalMemory.HistoryPurity = 0.1f;

    FIntent LocalIntent;
    LocalIntent.Coherence = 0.1f;

    FRngState LocalRng;

    // === SWITCH ===
    const FRealState& FinalA = bUseEditorInputs ? A : LocalA;
    const FRealState& FinalB = bUseEditorInputs ? B : LocalB;
    const FEnvironment& FinalEnv = bUseEditorInputs ? Env : LocalEnv;
    const FMemoryState& FinalMemory = bUseEditorInputs ? Memory : LocalMemory;
    const FIntent& FinalIntent = bUseEditorInputs ? Intent : LocalIntent;
    FRngState& FinalRng = bUseEditorInputs ? Rng : LocalRng;

    // === RUN ===
    FRealState Result = Pipeline::ApplyMorok(
        FinalA,
        FinalB,
        FinalEnv,
        FinalMemory,
        FinalIntent,
        FinalRng);

    // === LOG ===
    UE_LOG(LogTemp, Warning, TEXT("Result Magnitude: %f"), Result.Magnitude);
    UE_LOG(LogTemp, Warning, TEXT("Distortion: %f"), Result.Meta.Distortion);
    UE_LOG(LogTemp, Warning, TEXT("Purity: %f"), Result.Meta.Purity);

    UE_LOG(LogTemp, Warning, TEXT("Dir Body: %f"), Result.Direction.Body);
    UE_LOG(LogTemp, Warning, TEXT("Dir Mind: %f"), Result.Direction.Mind);
    UE_LOG(LogTemp, Warning, TEXT("Dir Spirit: %f"), Result.Direction.Spirit);
    UE_LOG(LogTemp, Warning, TEXT("Dir Nature: %f"), Result.Direction.Nature);
}