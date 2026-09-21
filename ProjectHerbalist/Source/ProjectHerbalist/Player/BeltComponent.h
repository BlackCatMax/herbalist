// BeltComponent.h
//
// Пояс (DESIGN_Diegetic_Interface.md, этап 4; решения пользователя
// 2026-09-21). Инструмент сбора, переносной контейнер, оберег и семенной
// мешочек видны на себе -- стоит посмотреть вниз -- и надеваются рукой:
// предмет в руке, взгляд на пояс, взаимодействие. Пустой рукой по предмету
// пояса -- снять его в руку; по мешочку -- развязать (сбор на семена) или
// завязать.
//
// Пояс, как и рука, ничего не хранит сам: инструмент -- это
// CurrentGatheringTool контроллера, контейнер -- ContainerType котомки,
// намерение сбора -- CurrentHarvestIntent. Надетый предмет остаётся в котомке;
// ушёл из неё (продан, убран в хранилище) -- слот пустеет сам.
//
// Оберег: повесить -- значит включить на срок, тем же ActivateWard, что у
// команды; светится, пока действует, потом гаснет. Снять и повесить снова --
// новое окно. Серебряный оберег действует, пока висит.
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Core/Types/HerbalistCoreTypes.h"
#include "Player/BeltItemActor.h"
#include "BeltComponent.generated.h"

UCLASS(ClassGroup = (Herbalist), meta = (BlueprintSpawnableComponent))
class PROJECTHERBALIST_API UBeltComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UBeltComponent();

    // Пояс виден: взгляд опущен ниже BeltPitchDegrees.
    bool IsInView() const;
    static constexpr float BeltPitchDegrees = -45.0f;

    // Надеть предмет из ячейки котомки. false -- это не вещь для пояса (жест
    // идёт дальше); true -- пояс ответил, даже если отказал (причина в логе).
    bool PutOn(int32 InventoryIndex);

    // Пустой рукой по слоту: снять в руку или, у мешочка, переключить.
    void UseSlot(EBeltSlot Slot);

    // Что сейчас на слоте (NAME_None -- пусто). Мешочек -- всегда на месте.
    FName GetSlotItem(EBeltSlot Slot) const;
    bool IsWardLit() const;

    ABeltItemActor* FindItemUnderView(const FVector& Start, const FVector& End) const;
    const TArray<TObjectPtr<ABeltItemActor>>& GetShownItems() const { return Shown; }

    // Какой предмет дал контейнер, надетый не поясом (стартовая корзина).
    void SetContainerItem(FName ItemID);

    // Надетое ушло из котомки -- слот пустеет. Открыт для тестов; зовётся из тика.
    void ReleaseMissing();

    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
    class AHerbalistPlayerController* GetController() const;
    bool OwnsItem(FName ItemID) const;
    void TakeIntoHand(FName ItemID);
    void SyncVisual();
    void ClearVisual();

    // Контейнер: какой предмет его дал. Без реестра (автотесты) -- то, что
    // надели; стартовая корзина -- с BeginPlay.
    FName ContainerItemID;

    // Оберег, повешенный в этом заходе; серебряный после загрузки выводится
    // из мира (GetSlotItem).
    FName WardItemID;
    double WardLitUntil = -1.0;

    UPROPERTY()
    TArray<TObjectPtr<ABeltItemActor>> Shown;
    float VisualRefreshAccumulator = 0.0f;
};
