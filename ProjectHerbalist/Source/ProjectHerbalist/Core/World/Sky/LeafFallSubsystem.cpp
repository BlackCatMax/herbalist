// Core/World/Sky/LeafFallSubsystem.cpp

#include "Core/World/Sky/LeafFallSubsystem.h"
#include "Core/World/GridWorldManager.h"
#include "Core/Config/HerbalistSettings.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "ProjectHerbalist.h"
#include "HerbalistLogChannels.h"

const FName ULeafFallSubsystem::LeafFallRateParameter(TEXT("LeafFallRate"));
const FName ULeafFallSubsystem::WindIntensityParameter(TEXT("WindIntensity"));

bool ULeafFallSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
    return WorldType == EWorldType::Game || WorldType == EWorldType::PIE || WorldType == EWorldType::Editor;
}

TStatId ULeafFallSubsystem::GetStatId() const
{
    RETURN_QUICK_DECLARE_CYCLE_STAT(ULeafFallSubsystem, STATGROUP_Tickables);
}

void ULeafFallSubsystem::Deinitialize()
{
    if (UNiagaraComponent* Component = LeafFallComponent.Get())
    {
        Component->DestroyComponent();
    }
    LeafFallComponent.Reset();
    Super::Deinitialize();
}

float ULeafFallSubsystem::LeafFallRate(float LeafFall01, float Wind01, bool bDeciduous, float WindShare)
{
    if (!bDeciduous)
    {
        return 0.0f;
    }
    const float Share = FMath::Clamp(WindShare, 0.0f, 1.0f);
    return FMath::Clamp(LeafFall01, 0.0f, 1.0f) * ((1.0f - Share) + Share * FMath::Clamp(Wind01, 0.0f, 1.0f));
}

float ULeafFallSubsystem::ComputeLeafFallRate(const AGridWorldManager& Manager, const FVector& Location) const
{
    const UHerbalistSettings* Settings = GetHerbalistSettings();
    int32 X = 0, Y = 0;
    const FGridCell* Cell = Manager.WorldPositionToCell(Location, X, Y) ? Manager.GetCellConst(X, Y) : nullptr;
    const bool bDeciduous = Cell && !Cell->bIsWater && Settings && Settings->DeciduousBiomes.Contains(Cell->Biome);
    return LeafFallRate(Manager.GetLeafFall01(), Manager.GetWindIntensity(), bDeciduous,
        Settings ? Settings->LeafFallWindShare : 0.6f);
}

void ULeafFallSubsystem::ApplyRate(AActor* Owner, UNiagaraSystem* System, float Rate, float Wind01, float DeltaSeconds)
{
    UNiagaraComponent* Component = LeafFallComponent.Get();
    if (Rate <= KINDA_SMALL_NUMBER)
    {
        if (!Component || !bRequestedActive)
        {
            return;
        }
        Component->SetVariableFloat(LeafFallRateParameter, 0.0f);
        ZeroRateSeconds += FMath::Max(DeltaSeconds, 0.0f);
        if (ZeroRateSeconds >= IdleDeactivateSeconds)
        {
            // Надолго (вне сезона, ушёл из леса): деактивация, уже падающие
            // листья долетают.
            Component->Deactivate();
            bRequestedActive = false;
        }
        return;
    }
    ZeroRateSeconds = 0.0f;
    if (!System || !Owner || !Owner->GetRootComponent())
    {
        return;
    }
    if (!Component || Component->GetAttachParent() != Owner->GetRootComponent() || Component->GetAsset() != System)
    {
        if (Component)
        {
            Component->DestroyComponent();
        }
        Component = UNiagaraFunctionLibrary::SpawnSystemAttached(System, Owner->GetRootComponent(), NAME_None,
            FVector::ZeroVector, FRotator::ZeroRotator, EAttachLocation::KeepRelativeOffset,
            /*bAutoDestroy=*/false, /*bAutoActivate=*/true);
        LeafFallComponent = Component;
        bRequestedActive = Component != nullptr;
    }
    if (!Component)
    {
        return;
    }
    Component->SetVariableFloat(LeafFallRateParameter, Rate);
    Component->SetVariableFloat(WindIntensityParameter, FMath::Clamp(Wind01, 0.0f, 1.0f));
    if (!bRequestedActive)
    {
        Component->Activate();
        bRequestedActive = true;
    }
}

void ULeafFallSubsystem::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    UWorld* World = GetWorld();
    if (!World || !World->IsGameWorld())
    {
        return;
    }

    // Система -- один раз: битый путь иначе грузился бы каждый кадр.
    if (!bSystemResolved)
    {
        bSystemResolved = true;
        const UHerbalistSettings* Settings = GetHerbalistSettings();
        CachedSystem = Settings ? Settings->LeafFallSystem.LoadSynchronous() : nullptr;
    }
    UNiagaraSystem* System = CachedSystem.Get();
    if (!System)
    {
        return;
    }

    AGridWorldManager* Manager = CachedManager.Get();
    if (!Manager && World->GetTimeSeconds() >= NextManagerSearchSeconds)
    {
        // Карта без менеджера (меню) -- искать раз в секунду, не каждый кадр.
        NextManagerSearchSeconds = World->GetTimeSeconds() + 1.0;
        for (TActorIterator<AGridWorldManager> It(World); It; ++It)
        {
            Manager = *It;
            CachedManager = Manager;
            break;
        }
    }
    APawn* Pawn = UGameplayStatics::GetPlayerPawn(World, 0);
    if (!Manager || !Pawn)
    {
        ApplyRate(nullptr, System, 0.0f, 0.0f, DeltaTime);
        return;
    }

    ApplyRate(Pawn, System, ComputeLeafFallRate(*Manager, Pawn->GetActorLocation()), Manager->GetWindIntensity(), DeltaTime);
}
