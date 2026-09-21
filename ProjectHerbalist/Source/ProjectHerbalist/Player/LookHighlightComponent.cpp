// LookHighlightComponent.cpp
#include "Player/LookHighlightComponent.h"
#include "Core/Interaction/Interactable.h"
#include "Core/Resources/AHerbalistResourceActor.h"
#include "Components/PrimitiveComponent.h"
#include "GameFramework/PlayerController.h"
#include "Player/HerbalistPlayerController.h"
#include "Player/PesterComponent.h"
#include "Player/PesterItemActor.h"
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

    APlayerController* PC = Cast<APlayerController>(GetOwner());
    if (!PC || !PC->IsLocalController() || !GetWorld()) return;

    FVector ViewLocation;
    FRotator ViewRotation;
    PC->GetPlayerViewPoint(ViewLocation, ViewRotation);
    FCollisionQueryParams Params(SCENE_QUERY_STAT(HerbalistLookHighlight), false);
    Params.AddIgnoredActor(PC->GetPawn());

    // Те же два канала и в том же порядке, что у Interact и сбора
    // (GetHitResultFromCamera): сначала видимость, канал ресурсов --
    // запасной. Иначе подсветка и то, что сработает по клавише, могли бы
    // разойтись.
    FHitResult Hit;
    const FVector End = ViewLocation + ViewRotation.Vector() * ReachCm;
    AActor* Target = nullptr;

    // Открытый пестерь -- его предметы мир не видит, спрашиваем напрямую.
    if (const AHerbalistPlayerController* HPC = Cast<AHerbalistPlayerController>(PC))
    {
        if (HPC->PesterComponent)
        {
            if (APesterItemActor* Item = HPC->PesterComponent->FindItemUnderView(ViewLocation, ViewLocation + ViewRotation.Vector() * 200.0f))
            {
                SetFocusedActor(Item);
                return;
            }
        }
    }
    if (GetWorld()->LineTraceSingleByChannel(Hit, ViewLocation, End, ECC_Visibility, Params))
    {
        Target = Hit.GetActor();
    }
    if (!IsHighlightable(Target) && GetWorld()->LineTraceSingleByChannel(Hit, ViewLocation, End, ECC_GameTraceChannel1, Params))
    {
        Target = Hit.GetActor();
    }
    SetFocusedActor(Target);
}

void ULookHighlightComponent::EndPlay(const EEndPlayReason::Type Reason)
{
    SetFocusedActor(nullptr);
    Super::EndPlay(Reason);
}
