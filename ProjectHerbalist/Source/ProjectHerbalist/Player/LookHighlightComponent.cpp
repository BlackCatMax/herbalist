// LookHighlightComponent.cpp
#include "Player/LookHighlightComponent.h"
#include "Core/Interaction/Interactable.h"
#include "Core/Resources/AHerbalistResourceActor.h"
#include "Components/PrimitiveComponent.h"
#include "GameFramework/PlayerController.h"
#include "Player/HerbalistPlayerController.h"
#include "Player/PesterComponent.h"
#include "Player/PesterItemActor.h"
#include "Player/BeltComponent.h"
#include "Player/BeltItemActor.h"
#include "Engine/World.h"

ULookHighlightComponent::ULookHighlightComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = true;
    // Подсветка -- не симуляция: десять раз в секунду глазу хватает, а
    // трасса каждый кадр не нужна.
    PrimaryComponentTick.TickInterval = 0.1f;
}

bool ULookHighlightComponent::IsHighlightable(const AActor* Actor)
{
    return Actor && (Actor->Implements<UInteractable>() || Actor->IsA<AHerbalistResourceActor>());
}

bool ULookHighlightComponent::IsFocusCandidate(const AActor* Actor) const
{
    if (!IsHighlightable(Actor))
    {
        return false;
    }
    // Растение -- только в пределах сбора (2026-09-28, по PIE-логу: подсвечено
    // за 10 м, «Взаимодействие» отказывало «далеко -- 225 см»). Что подсвечено,
    // с тем клавиша и сработает. Без пешки мерить не от чего -- не режем.
    const AHerbalistPlayerController* HPC = Cast<AHerbalistPlayerController>(GetOwner());
    if (HPC && HPC->GetPawn() && Actor->IsA<AHerbalistResourceActor>())
    {
        return HPC->HarvestDistanceTo(Actor->GetActorLocation()) <= HPC->MaxHarvestDistance;
    }
    return true;
}

void ULookHighlightComponent::Highlight(AActor* Actor)
{
    if (!Actor) return;
    TArray<UPrimitiveComponent*> Primitives;
    Actor->GetComponents<UPrimitiveComponent>(Primitives);
    for (UPrimitiveComponent* Primitive : Primitives)
    {
        SavedDepth.Add({ Primitive, Primitive->bRenderCustomDepth != 0, Primitive->CustomDepthStencilValue });
        Primitive->SetRenderCustomDepth(true);
        Primitive->SetCustomDepthStencilValue(HighlightStencilValue);
    }
}

void ULookHighlightComponent::Unhighlight()
{
    for (const FSavedDepth& Saved : SavedDepth)
    {
        if (UPrimitiveComponent* Primitive = Saved.Primitive.Get())
        {
            Primitive->SetRenderCustomDepth(Saved.bRenderCustomDepth);
            Primitive->SetCustomDepthStencilValue(Saved.StencilValue);
        }
    }
    SavedDepth.Reset();
}

void ULookHighlightComponent::SetFocusedActor(AActor* NewTarget)
{
    AActor* Current = FocusedActor.Get();
    AActor* Wanted = IsHighlightable(NewTarget) ? NewTarget : nullptr;
    if (Current == Wanted) return;
    Unhighlight();
    FocusedActor = Wanted;
    Highlight(Wanted);
}

void ULookHighlightComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    // Подсветка -- только у своего игрока; явный RefreshFocus (нажатие
    // клавиши, тесты) идёт без этой проверки.
    const APlayerController* PC = Cast<APlayerController>(GetOwner());
    if (PC && PC->IsLocalController())
    {
        RefreshFocus();
    }
}

AActor* ULookHighlightComponent::RefreshFocus()
{
    APlayerController* PC = Cast<APlayerController>(GetOwner());
    if (!PC || !GetWorld())
    {
        return FocusedActor.Get();
    }

    FVector ViewLocation;
    FRotator ViewRotation;
    PC->GetPlayerViewPoint(ViewLocation, ViewRotation);
    FCollisionQueryParams Params(SCENE_QUERY_STAT(HerbalistLookHighlight), false);
    Params.AddIgnoredActor(PC->GetPawn());

    // Открытый пестерь и пояс -- их предметы мир не видит, спрашиваем напрямую.
    if (const AHerbalistPlayerController* HPC = Cast<AHerbalistPlayerController>(PC))
    {
        if (HPC->PesterComponent)
        {
            if (APesterItemActor* Item = HPC->PesterComponent->FindItemUnderView(ViewLocation, ViewLocation + ViewRotation.Vector() * 200.0f))
            {
                SetFocusedActor(Item);
                return Item;
            }
        }
        if (HPC->BeltComponent)
        {
            if (ABeltItemActor* Item = HPC->BeltComponent->FindItemUnderView(ViewLocation, ViewLocation + ViewRotation.Vector() * 200.0f))
            {
                SetFocusedActor(Item);
                return Item;
            }
        }
    }

    // Мир: прямой луч по видимости, затем по каналу взаимодействия; мимо --
    // сфера по тем же каналам, ближайшая к глазу цель.
    const FVector End = ViewLocation + ViewRotation.Vector() * ReachCm;
    AActor* Target = nullptr;
    FHitResult Hit;
    // Что глаз видит первым -- дальше него сфера не ищет: канал
    // взаимодействия стены не держат, и без этой границы подсветилось бы
    // то, что за стеной.
    float SightLimit = ReachCm;
    if (GetWorld()->LineTraceSingleByChannel(Hit, ViewLocation, End, ECC_Visibility, Params))
    {
        // Край того, что закрывает вид, -- с запасом на толщину сферы.
        SightLimit = FVector::Dist(ViewLocation, Hit.ImpactPoint) + AssistRadiusCm * 2.0f;
        if (IsFocusCandidate(Hit.GetActor()))
        {
            Target = Hit.GetActor();
        }
    }
    if (!Target && GetWorld()->LineTraceSingleByChannel(Hit, ViewLocation, End, ECC_GameTraceChannel1, Params)
        && IsFocusCandidate(Hit.GetActor()) && FVector::Dist(ViewLocation, Hit.ImpactPoint) <= SightLimit)
    {
        Target = Hit.GetActor();
    }
    if (!Target && AssistRadiusCm > 0.0f)
    {
        const FCollisionShape Sphere = FCollisionShape::MakeSphere(AssistRadiusCm);
        float BestDistSq = TNumericLimits<float>::Max();
        for (const ECollisionChannel Channel : { ECC_Visibility, ECC_GameTraceChannel1 })
        {
            TArray<FHitResult> Hits;
            GetWorld()->SweepMultiByChannel(Hits, ViewLocation, End, FQuat::Identity, Channel, Sphere, Params);
            for (const FHitResult& Candidate : Hits)
            {
                const float DistSq = FVector::DistSquared(ViewLocation, Candidate.ImpactPoint);
                if (IsFocusCandidate(Candidate.GetActor()) && DistSq < BestDistSq && DistSq <= FMath::Square(SightLimit))
                {
                    BestDistSq = DistSq;
                    Target = Candidate.GetActor();
                }
            }
        }
    }
    SetFocusedActor(Target);
    return FocusedActor.Get();
}

void ULookHighlightComponent::EndPlay(const EEndPlayReason::Type Reason)
{
    SetFocusedActor(nullptr);
    Super::EndPlay(Reason);
}
