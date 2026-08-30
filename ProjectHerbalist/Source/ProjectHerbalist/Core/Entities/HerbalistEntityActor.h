// HerbalistEntityActor.h
//
// Родительский класс проявленных сущностей бестиария (2026-08-30, "заводим
// родительские классы для дальнейшей реализации и связки"). До этой правки
// весь бестиарий (§16.2/§16.3/§16.4, 58 карточек) существовал только как
// данные — таблицы определений (AmbientEntityTypes.h/LandmarkTypes.h/
// LegendaryEntityTypes.h) и один FName-тег на клетке
// (FGridCell::ManifestedEntityID, см. GridWorldManagerEntities.cpp). Ничего
// не спавнилось, ничего не было видно и не с чем было провзаимодействовать —
// §16.2 в 16_Entity_Manifestation.md прямо называлось "амбиентная зона, без
// актора" (осознанное решение той сессии). Разворачивается по прямому
// запросу пользователя: у всех трёх рангов теперь есть физическое
// представление, готовое принять меш/партиклы/поведение по конкретному
// существу позже (контент, не C++ — тот же принцип, что уже даёт
// BP_FirstPersonCharacter поверх контроллера).
//
// Голый C++-класс сознательно без меша по умолчанию (невидимый маркер) —
// три под-класса по рангу (AmbientEntityActor/LandmarkEntityActor/
// LegendaryEntityActor) существуют, чтобы у каждого ранга был свой
// C++-тип для дальнейшего расхождения поведения, а не для визуала самого
// по себе; визуал — Blueprint-наследники поверх них, по одному на
// конкретное существо, когда дойдёт очередь до контента.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/Interaction/Interactable.h"
#include "HerbalistEntityActor.generated.h"

class AGridWorldManager;
class UStaticMeshComponent;

UCLASS()
class PROJECTHERBALIST_API AHerbalistEntityActor : public AActor, public IInteractable
{
    GENERATED_BODY()

public:
    AHerbalistEntityActor();

    // Вызывается сразу после спавна (AGridWorldManager::SyncManifestedEntityActor)
    // — тот же принцип двухфазной инициализации, что уже у
    // AHerbalistResourceActor::Init/AMemoryFragmentActor::Init: конструктор
    // создаёт компоненты, Init наполняет данными о конкретном проявлении.
    virtual void Init(FName InEntityID, const FIntPoint& InCell, AGridWorldManager* InWorldManager);

    FName GetEntityID() const { return EntityID; }
    FIntPoint GetGridCell() const { return GridCell; }

    // Пустая реализация по умолчанию — большинство существ (особенно Низший
    // ранг, декоративный по спецификации) не нуждаются во взаимодействии
    // вовсе. Наследник переопределяет, если у конкретного ранга/существа
    // оно появится (см. GetLandmark() у ALandmarkEntityActor — естественная
    // точка для будущего "посмотреть на хозяина").
    virtual void OnInteract_Implementation(AHerbalistPlayerController* PC) override {}

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UStaticMeshComponent* MeshComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Entity")
    FName EntityID;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Entity")
    FIntPoint GridCell = FIntPoint(-1, -1);

    UPROPERTY()
    TWeakObjectPtr<AGridWorldManager> WorldManagerRef;
};
