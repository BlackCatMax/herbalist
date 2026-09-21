// SleepBenchActor.h
//
// Лавка у дома (DESIGN_Diegetic_Interface.md, этап 6; решение пользователя
// 2026-09-21: «лавка у стола, сон до утра»). Системного меню нет: лечь на
// лавку пустой рукой -- проспать до рассвета и сохраниться. Ночью это ещё и
// способ переждать нечисть.
//
// Автор уровня ставит лавку сам, у дома; меш -- Blueprint-наследник.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/Interaction/Interactable.h"
#include "SleepBenchActor.generated.h"

class UStaticMeshComponent;
class USphereComponent;

UCLASS(Blueprintable, BlueprintType)
class PROJECTHERBALIST_API ASleepBenchActor : public AActor, public IInteractable
{
    GENERATED_BODY()

public:
    ASleepBenchActor();

    virtual void OnInteract_Implementation(AHerbalistPlayerController* PC) override;

protected:
    // TODO: финальный арт -- лавка; меш задаёт Blueprint-наследник.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UStaticMeshComponent> MeshComponent;

    // Та же сфера взгляда, что у тайника и жертвенника: лавку видно и без меша.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<USphereComponent> InteractionSphere;
};
