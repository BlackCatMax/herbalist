// HerbalistInventoryComponent.h
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Core/Types/HerbalistCoreTypes.h"
#include "Math/RandomStream.h"
#include "HerbalistInventoryComponent.generated.h"

struct FInventorySnapshot;
struct FStateDelta;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnInventoryChanged);

// Порча трав (2026-08-29, прямая правка пользователя): "не зависит от
// клеток биомов и чего бы то ни было. Травы портятся сами по себе, это
// естественный процесс. На сохранность влияют сами контейнеры хранения" —
// отменяет и капищную защиту инвентаря (эффект 4, §15.5), и location-based
// порчу от Ржавых духов/Водяных бесов/Злыдней (§16.2, тот же день, раньше
// в этой же сессии). None — то, что игрок физически несёт на себе, без
// структуры хранения вовсе (не "контейнер получше корзины", а отсутствие
// контейнера, отсюда и множитель 1.0, старый глобальный дефолт без
// модификации).
//
// Изначально заведено только два полюса (по прямому решению пользователя,
// "инфраструктура + 2-3 примера") — Basket/Cellar. Три остальных добавлены
// 2026-08-29, тем же днём ("проработка инвентаря и систем хранения" ->
// "больше типов контейнеров"), тем же принципом: реальное свойство места
// хранения (закрытость/проветриваемость/герметичность), не выдуманная
// метрика. Шесть множителей образуют спектр от худшего к лучшему:
// Sack(1.4) > Basket(1.3) > None(1.0) > Tues(0.85) > Cabinet(0.7) > Cellar(0.4) > Jar(0.25).
//
// Tues добавлен 2026-09-04 (переносные контейнеры игрока, "разберём
// тщательно" систему хранения, прямой запрос пользователя) — берестяной
// туёс, единственный ПЕРЕНОСНОЙ (не стационарная мебель дома) контейнер
// лучше базовой линии: реальное природное антисептическое/влагостойкое
// свойство бересты (см. карточку компендиума, 04_Compendium/Утварь/Туёс.md)
// ставит его выше None/Basket/Sack, но намеренно ХУЖЕ Cabinet/Cellar/Jar —
// то, что несёшь на себе в дороге, не может быть лучше настоящего погреба,
// иначе это разрушило бы смысл "дом лучше похода".
UENUM(BlueprintType)
enum class EStorageContainerType : uint8
{
    None,      // на себе, без контейнера
    Basket,    // корзина/лукошко — хуже базовой линии, открытая, дышащая
    Sack,      // мешок (дерюга/рогожа) — хуже корзины: влагу держит, а не проветривает, моль/вредители
    Tues,      // туёс (берестяной короб) — лучше базовой линии, но переносной, не мебель: хуже шкафа
    Cabinet,   // шкаф — лучше базовой линии, закрыт от пыли/вредителей, но комнатная температура/влажность
    Cellar,    // погреб — лучше шкафа, тёмный, прохладный, стабильная влажность
    Jar        // банка (герметичная) — лучшее хранение: почти нет доступа воздуха/влаги
};

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class PROJECTHERBALIST_API UHerbalistInventoryComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UHerbalistInventoryComponent();
	void ApplyStateDelta(const FStateDelta& Delta);

    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory")
    int32 MaxSlots = 20;

    // Тип контейнера этого конкретного инвентаря — определяет множитель
    // порчи (см. комментарий у EStorageContainerType выше). Игрок сам несёт
    // None по умолчанию; AStorageContainer в конструкторе ставит Basket как
    // разумный дефолт для найденного в мире контейнера, редактируемо per-instance.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory")
    EStorageContainerType ContainerType = EStorageContainerType::None;

    static constexpr int32 MAX_STACK_SIZE = 9;

    UFUNCTION(BlueprintCallable, Category = "Inventory")
    bool AddItem(const FInventoryItem& Item, int32 Amount = 1);

    UFUNCTION(BlueprintCallable, Category = "Inventory")
    bool RemoveItem(int32 Index, int32 Amount = 1);

    UFUNCTION(BlueprintCallable, Category = "Inventory")
    bool TransferOneItem(int32 SourceIndex, int32 TargetIndex);

    UFUNCTION(BlueprintCallable, Category = "Inventory")
    bool TransferItemTo(int32 SourceIndex, UHerbalistInventoryComponent* TargetInventory);

    // Разделить стопку
    UFUNCTION(BlueprintCallable, Category = "Inventory")
    bool SplitStack(int32 Index, int32 Amount, FInventoryItem& OutItem);

    UFUNCTION(BlueprintCallable, Category = "Inventory")
    TArray<FInventoryItem> GetItems() const { return Items; }

    int32 GetNumSlots() const { return Items.Num(); }

    const FInventoryItem* GetSlot(int32 Index) const;

    UFUNCTION(BlueprintCallable, Category = "Inventory")
    void Clear();

	FInventorySnapshot CaptureState() const;

    // Точное восстановление из сохранения (Core/Save/HerbalistSaveTypes.h) —
    // в отличие от AddItem, не пытается стекать/сливать слоты с уже
    // существующим содержимым, просто ставит ровно то, что было сохранено.
    void RestoreItems(const TArray<FInventoryItem>& InItems) { Items = InItems; }

    UPROPERTY(BlueprintAssignable, Category = "Inventory")
    FOnInventoryChanged OnInventoryChanged;

    // Переносные контейнеры игрока (2026-09-04, EquipContainer Exec,
    // HerbalistPlayerController.cpp) — тестируемое ядро, отделённое от
    // Exec-обёртки: та резолвит FIngredientTableRow::GrantsContainerType
    // через IngredientRegistrySubsystem (недоступный в Editor-мире
    // автотестов, тот же класс пробела, что уже у ActivateWard/
    // TradeWithCommunity/PlantSeed, см. ROADMAP.md), эта функция принимает
    // уже резолвленный GrantsType и оперирует только собственным Items —
    // проверяема напрямую. Владение — по имени, тот же приём, что уже
    // OfferToCommunity/ActivateWard, но БЕЗ списания (контейнер носишь,
    // не сжигаешь, тот же принцип, что уже у активации оберегов).
    // GrantsType == None (карточка не даёт контейнера) или предмета нет в
    // инвентаре — отказ, ContainerType не трогается.
    UFUNCTION(BlueprintCallable, Category = "Inventory")
    bool TryEquipContainer(FName IngredientID, EStorageContainerType GrantsType);

protected:
    UPROPERTY()
    TArray<FInventoryItem> Items;

    int32 FindStackableSlot(const FInventoryItem& Item) const;
    bool AreItemsStackable(const FInventoryItem& A, const FInventoryItem& B) const;
    void MergeStack(FInventoryItem& Target, const FInventoryItem& Source, int32 AddedCount);

    // Применить порчу к одному предмету (если bSubjectToDecay == true)
    void ApplyDecayToItem(FInventoryItem& Item, float DeltaTime, float DecayRate);

public:
    // Гниение как терминальное состояние (2026-09-04) -- чистая функция от
    // Meta (без обращения к реестрам/GameInstance), чтобы её можно было
    // протестировать напрямую в Editor-мире автотестов, где IngredientRegistrySubsystem
    // недоступен (тот же класс ограничения, что уже у TradeWithCommunity/PlantSeed).
    static bool ShouldConvertToPeregnoy(const FMeta& Meta, float PurityThreshold, float DistortionThreshold);

    // ID новой карточки-компоста (DT_IngredientClass, PeregnoyAppendCommandlet) --
    // единственное место, где это имя жёстко зашито в коде, остальные места
    // читают эту константу.
    static const FName PeregnoyIngredientID;

protected:

    // Периодичность обновления (в секундах)
    static constexpr float DecayUpdateInterval = 1.0f;
    float TimeSinceLastDecayUpdate = 0.0f;

    // Джиттер осей при порче раньше брался из глобального FMath::FRandRange —
    // недетерминированно, не воспроизводимо по сиду (тот же класс бага, что и
    // у спавна ресурсов, AUDIT_AND_REFACTORING_PLAN §1.3/META_AUDIT §1.1).
    // Сид фиксированный, не WorldRNG: инвентарь — состояние актора-владельца,
    // не сетки мира, но детерминизм внутри одной сессии всё равно нужен для
    // трассировки/реплея.
    FRandomStream DecayRng = FRandomStream(20260823);
};