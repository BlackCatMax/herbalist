// HerbalistInventoryComponent.h
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Core/Types/HerbalistCoreTypes.h"
#include "Math/RandomStream.h"
#include "HerbalistInventoryComponent.generated.h"

struct FInventorySnapshot;
struct FStateDelta;
struct FIngredientTableRow;

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

// Станции обработки (2026-09-05, многоступенчатые зелья -- "Готовим обычное
// зелье... Отстой... Варка... Фильтрация... Выпаривание", прямой запрос
// пользователя) -- обобщение bool bIsDryingRack (было единственным полем
// "этот инвентарь -- станция-процесс") под появление ещё двух типов станций.
// Один enum на самом инвентаре, НЕ три параллельных bool (bIsSettlingStand/
// bIsEvaporationStill) -- инвентарь физически может быть только ОДНОЙ
// станцией одновременно, взаимоисключающие bool'ы позволили бы невалидное
// состояние "сушилка И отстойник разом", enum делает его непредставимым.
//
// ВАЖНО: обобщается только "какая это станция" (ось конфигурации инвентаря),
// НЕ конкретные таймеры/эффекты процессов -- у Сушки уже своя пара
// bIsDried/DryingTimeRemainingSeconds на FInventoryItem, у Отстоя и
// Выпаривания -- свои отдельные пары (bHasSettled/SettlingTimeRemainingSeconds,
// bHasEvaporated/EvaporationTimeRemainingSeconds, HerbalistCoreTypes.h), не
// один общий "универсальный процесс" -- тот же архитектурный принцип, что
// уже держит три независимых набора полей у оберегов (WardExpiryGameSeconds
// и аналоги, GridWorldManagerWards.cpp): похожие по форме, но разные по
// формуле эффекта механики получают параллельные, не общие, поля.
UENUM(BlueprintType)
enum class EProcessingStationType : uint8
{
    None,             // обычный инвентарь/тара, TickComponent не трогает ни один из трёх процессов
    DryingRack,       // сушилка -- см. довод у bIsDried/DryingTimeRemainingSeconds
    SettlingStand,    // отстойник -- усиление доминирующей оси ценой Magnitude
    EvaporationStill  // выпарной куб -- концентрация Magnitude/Potency ценой Distortion/Corruption
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

    // Станция обработки (DESIGN_Community_And_Homestead.md §2.2, "Хранилища"
    // пункт 3, 2026-09-04; обобщено на три типа 2026-09-05, см. довод у
    // EProcessingStationType выше) — станция-процесс, НЕ тип контейнера
    // (EStorageContainerType остаётся отдельной осью, множитель тары не про
    // обработку вовсе). Тот же приём, что уже ContainerType: плоское поле на
    // самом компоненте, выставляется владельцем-актором (ADryingRackActor/
    // ASettlingStandActor/AEvaporationStillActor в конструкторе, тем же
    // жестом, что AStorageContainer выставляет ContainerType=Basket) — не
    // отдельный подкласс UActorComponent на каждый тип станции, инвентарь
    // функционально тот же, просто с другим поведением TickComponent для
    // предметов внутри него.
    //
    // None (дефолт, обычный инвентарь/тара) — TickComponent не трогает ни
    // один из трёх процессов, ровно как до появления сушки. DryingRack/
    // SettlingStand/EvaporationStill — соответствующий процесс взводит и
    // досчитывает свой таймер на каждом подходящем предмете (см.
    // TickDryingItem/TickSettlingItem/TickEvaporationItem ниже), пока
    // предмет физически лежит здесь (см. довод у
    // FInventoryItem::DryingTimeRemainingSeconds — переложил в обычный
    // карман, таймер замер на месте, не сброшен, тот же принцип у Отстоя/
    // Выпаривания).
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory")
    EProcessingStationType StationType = EProcessingStationType::None;

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

    // Сушка (2026-09-04) -- чистая функция состояния таймера, БЕЗ обращения
    // к реестрам (тот же принцип тестируемости без GameInstance, что уже
    // ShouldConvertToPeregnoy выше): взводит DryingTimeRemainingSeconds при
    // первом вызове (сентинел -1), иначе считает вниз на DeltaTime; по
    // достижении <=0 фиксирует bIsDried=true и возвращает true (сигнал
    // вызывающей стороне -- "только что досохло, применяй DriedStateDelta",
    // требующую реестра, отдельным шагом снаружи, см. TickComponent). Не
    // проверяет bIsWater/bIsDryingRack/Class сама -- эти решения принимает
    // вызывающая сторона (тот же принцип границы, что уже у
    // ShouldConvertToPeregnoy: чистая функция получает уже отфильтрованный
    // вход, а не фильтрует сама).
    static bool TickDryingItem(FInventoryItem& Item, float DeltaTime, float DryingDurationSeconds);

    // Резолвит длительность сушки ДЛЯ ОДНОГО предмета (2026-09-05, "процесс
    // сушки у разных растений разный (длительность)", прямой запрос
    // пользователя) -- приоритет у карточки: Row->DryingDurationSeconds,
    // если он НЕ сентинел -1 (см. довод у FIngredientTableRow::
    // DryingDurationSeconds). Row==nullptr (карточка не резолвится реестром)
    // или сентинел -1 на резолвленной карточке -- оба падают на
    // GlobalFallbackSeconds (UHerbalistSettings::DryingDurationSeconds).
    // Чистая функция БЕЗ обращения к реестрам сама по себе -- принимает уже
    // резолвленный Row (или nullptr), тот же приём границы, что уже
    // TickDryingItem/ShouldConvertToPeregnoy выше: резолв самой карточки
    // через IngredientRegistrySubsystem делает вызывающий TickComponent, эта
    // функция только решает, какое из двух чисел использовать -- поэтому
    // тестируема напрямую с рукописной FIngredientTableRow, без реестра/
    // GameInstance.
    static float ResolveDryingDurationSeconds(const FIngredientTableRow* Row, float GlobalFallbackSeconds);

    // Применяет FIngredientTableRow::DriedStateDelta к Meta предмета в
    // момент завершения сушки -- тот же приём клампа [0,1], что уже
    // ApplyDecayToItem применяет к Distortion/Corruption/Purity/Stability
    // (отдельная функция, не инлайн в TickComponent -- тестируема напрямую
    // без реестра/GameInstance, ровно как TickDryingItem выше).
    static void ApplyDriedStateDelta(FMeta& Meta, const FMeta& Delta);

    // Отстой (2026-09-05) -- чистая функция таймера, ТОТ ЖЕ приём границы,
    // что уже TickDryingItem: взводит SettlingTimeRemainingSeconds при
    // первом вызове, иначе считает вниз; по достижении <=0 фиксирует
    // bHasSettled=true и возвращает true (сигнал вызывающей стороне --
    // "только что отстоялось, применяй ApplySettlingEffect"). Не проверяет
    // bIsWater/StationType/IngredientID сама -- те же решения, что и у
    // TickDryingItem, принимает вызывающая сторона (TickComponent).
    static bool TickSettlingItem(FInventoryItem& Item, float DeltaTime, float SettlingDurationSeconds);

    // Эффект завершённого Отстоя -- "усиление доминирующей оси Direction
    // ценой Magnitude" (прямой запрос пользователя, см. довод у
    // FInventoryItem::bHasSettled, HerbalistCoreTypes.h). Находит argmax
    // среди Body/Mind/Spirit/Nature, прибавляет DominantAxisBoost, зовёт
    // NormalizeSum() (остальные три оси просаживаются пропорционально сами,
    // отдельно ничего вычитать не нужно), затем домножает Magnitude на
    // MagnitudeLossFactor < 1.0 -- цена усиления, "10% силы уходит в
    // осадок". Чистая функция от FRealState, тестируема напрямую.
    static void ApplySettlingEffect(FRealState& State, float DominantAxisBoost, float MagnitudeLossFactor);

    // Выпаривание (2026-09-05) -- тот же приём таймера, что TickSettlingItem
    // выше, своя независимая пара полей (bHasEvaporated/
    // EvaporationTimeRemainingSeconds).
    static bool TickEvaporationItem(FInventoryItem& Item, float DeltaTime, float EvaporationDurationSeconds);

    // Эффект завершённого Выпаривания -- "концентрация ценой риска" (прямой
    // запрос пользователя): Magnitude и Potency растут (усиление), но
    // Distortion И Corruption ОБА домножаются на тот же RiskMultiplier > 1.0
    // -- концентрация не разбирает, что усиливать, грязь концентрируется
    // вместе с силой. Min(..., 1.0f) на Magnitude вместо Clamp -- Magnitude
    // не имеет нижней границы 0 как ось Meta, только верхний потолок 1.0
    // (тот же приём, что уже ApplyDecayToItem::Purity/Stability используют
    // Max, а Distortion/Corruption -- Min, в зависимости от направления).
    static void ApplyEvaporationEffect(FRealState& State, float MagnitudeBoost, float PotencyBoost, float RiskMultiplier);

    // Фильтрация (2026-09-05, прямой запрос пользователя: "4. Фильтрация" --
    // ЕДИНСТВЕННЫЙ мгновенный шаг цепочки, без станции/таймера, см. довод в
    // задаче) -- "чище, но слабее": Purity растёт, Distortion/Corruption
    // падают, Potency падает как цена. Чистая функция клампа, тот же приём,
    // что ApplyDriedStateDelta выше -- тестируема напрямую без реестра.
    //
    // НЕ терминальна отдельным bool-флагом (сознательное решение, см. довод
    // у UHerbalistInventoryComponent::TryFilterPotion ниже) -- повторное
    // применение разрешено и складывается, но самоограничено клампами [0,1]
    // на каждой оси: предел раз за разом стремится к Purity=1/Distortion=0/
    // Corruption=0/Potency=0 ("чистая, но безвкусная вода"), не к
    // бесконечному росту показателей без цены.
    static void ApplyFilterEffect(FMeta& Meta, float PurityBoost, float DistortionReduction, float CorruptionReduction, float PotencyLoss);

    // Обёртка над ApplyFilterEffect для реального инвентаря игрока -- ТОТ ЖЕ
    // приём поиска, что уже AHerbalistPlayerController::UsePotion
    // (IndexOfByPredicate по первому предмету с IngredientID=="Potion" &&
    // Count>0, не более сложный селектор, проект уже принял эту простоту).
    // Живёт на компоненте, а не в Exec-обёртке контроллера (тот же
    // архитектурный уровень, что уже TryEquipContainer выше) -- оперирует
    // только собственным Items, читает пороги из UHerbalistSettings напрямую
    // (GetDefault<>(), не требует GameInstance/реестра — та же граница
    // доступности, что уже у ContainerType decay-множителей в
    // TickComponent), поэтому тестируема без сборки мира.
    UFUNCTION(BlueprintCallable, Category = "Inventory")
    bool TryFilterPotion();

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