// PesterComponent.h
//
// Пестерь -- котомка в руках (DESIGN_Diegetic_Interface.md, этап 2, решения
// пользователя 2026-09-21). Вместо окна котомки: пестерь раскрывается перед
// камерой, внутри -- мешочки по типу сырья, в них предметы; посмотрел на
// предмет, нажал взаимодействие -- он в руке.
//
// Место -- те же 20 мест на всё (MaxSlots), просто разложенные по мешочкам:
// вид меняется, правило вместимости нет. Время идёт, пока пестерь открыт.
//
// Данные не меняются: пестерь -- раскладка UHerbalistInventoryComponent,
// перестраивается на каждое изменение состава (OnInventoryChanged).
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Core/Types/HerbalistCoreTypes.h"
#include "Core/Data/IngredientTableRow.h"
#include "PesterComponent.generated.h"

class APesterItemActor;

// Мешочки пестеря -- по типу сырья, как в ресёрче (§3.1): коренья отдельно
// от листьев, камни в своём лоскуте, склянки в гнёздах.
UENUM(BlueprintType)
enum class EPesterPouch : uint8
{
    Herbs,      // травы, листья, коренья, цветы
    Fungi,      // грибы
    Stones,     // камни, минералы, кристаллы-обереги
    Vials,      // вода и зелья -- склянки в гнёздах
    Other,      // прочее: инструменты, находки, катализаторы
    Count UMETA(Hidden)
};

UCLASS(ClassGroup = (Herbalist), meta = (BlueprintSpawnableComponent))
class PROJECTHERBALIST_API UPesterComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UPesterComponent();

    void Open();
    void Close();
    void Toggle();
    bool IsOpen() const { return bOpen; }

    // Куда ляжет предмет. Класс -- из реестра ингредиентов (bIsMineral,
    // bIsLiquid и т.п. уже решены вызывающим); без реестра -- по признакам
    // самого предмета.
    static EPesterPouch PouchForItem(const FInventoryItem& Item, EIngredientClass Class, bool bClassKnown);

    // Разложенные сейчас предметы -- для тестов и для подсветки.
    const TArray<TObjectPtr<APesterItemActor>>& GetLaidOutItems() const { return LaidOut; }

    // Ближайший предмет пестеря на луче взгляда, nullptr -- мимо или пестерь
    // закрыт. Мир этих предметов не видит (они не отвечают ни одному каналу),
    // поэтому взаимодействие и подсветка спрашивают сначала здесь.
    APesterItemActor* FindItemUnderView(const FVector& Start, const FVector& End) const;

    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
    UFUNCTION()
    void OnInventoryChanged();

    void Rebuild();
    void ClearLayout();
    void PlaceItems();

    bool bOpen = false;

    UPROPERTY()
    TArray<TObjectPtr<APesterItemActor>> LaidOut;

    // Место каждого разложенного предмета внутри пестеря -- смещение от его
    // «дна», в осях взгляда.
    TArray<FVector> LocalOffsets;
};
