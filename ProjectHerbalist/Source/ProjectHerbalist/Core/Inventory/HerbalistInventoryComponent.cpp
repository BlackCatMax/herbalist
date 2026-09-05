// HerbalistInventoryComponent.cpp
#include "HerbalistInventoryComponent.h"
#include "ProjectHerbalist.h"
#include "HerbalistLogChannels.h"
#include "Core/Types/HerbalistCoreMath.h"
#include "Core/Config/HerbalistSettings.h"
#include "Core/Simulation/Public/DeltaTypes.h"
#include "Core/Simulation/Public/SnapshotTypes.h"
#include "Core/Subsystems/IngredientRegistrySubsystem.h"
#include "Core/Subsystems/WaterTypeRegistrySubsystem.h"
#include "Core/Data/IngredientTableRow.h"

const FName UHerbalistInventoryComponent::PeregnoyIngredientID = FName(TEXT("Перегной"));

UHerbalistInventoryComponent::UHerbalistInventoryComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickInterval = 0.2f;
}

void UHerbalistInventoryComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    TimeSinceLastDecayUpdate += DeltaTime;
    if (TimeSinceLastDecayUpdate < DecayUpdateInterval)
        return;

    TimeSinceLastDecayUpdate = 0.0f;

    const UHerbalistSettings* Settings = GetDefault<UHerbalistSettings>();
    const float GlobalDecayRate = Settings ? Settings->InventoryDecayRate : 0.02f;

    // Порча — естественный процесс, не зависящий от места в мире (прямая
    // правка пользователя 2026-08-29, отменяет и капищную защиту §15.5
    // эффект 4, и location-based порчу от Ржавых духов/Водяных бесов/
    // Злыдней §16.2, обе были здесь раньше в этой же сессии). Единственный
    // модификатор — тип контейнера, в котором предмет физически хранится.
    float ContainerDecayMultiplier = 1.0f;
    switch (ContainerType)
    {
    case EStorageContainerType::Basket:  ContainerDecayMultiplier = Settings ? Settings->BasketDecayMultiplier  : 1.3f;  break;
    case EStorageContainerType::Sack:    ContainerDecayMultiplier = Settings ? Settings->SackDecayMultiplier    : 1.4f;  break;
    case EStorageContainerType::Tues:    ContainerDecayMultiplier = Settings ? Settings->TuesDecayMultiplier    : 0.85f; break;
    case EStorageContainerType::Cabinet: ContainerDecayMultiplier = Settings ? Settings->CabinetDecayMultiplier : 0.7f;  break;
    case EStorageContainerType::Cellar:  ContainerDecayMultiplier = Settings ? Settings->CellarDecayMultiplier  : 0.4f;  break;
    case EStorageContainerType::Jar:     ContainerDecayMultiplier = Settings ? Settings->JarDecayMultiplier     : 0.25f; break;
    default: break;   // None — на себе, базовая линия без модификации
    }
    const float EffectiveGlobalDecayRate = GlobalDecayRate * ContainerDecayMultiplier;

    UIngredientRegistrySubsystem* IngredientReg = nullptr;
    UWaterTypeRegistrySubsystem* WaterReg = nullptr;
    if (UWorld* World = GetWorld())
    {
        if (UGameInstance* GI = World->GetGameInstance())
        {
            IngredientReg = GI->GetSubsystem<UIngredientRegistrySubsystem>();
            WaterReg = GI->GetSubsystem<UWaterTypeRegistrySubsystem>();
        }
    }

    const float RotPurityThreshold = Settings ? Settings->RotConversionPurityThreshold : 0.05f;
    const float RotDistortionThreshold = Settings ? Settings->RotConversionDistortionThreshold : 0.95f;

    // Сушка (2026-09-04) -- глобальный ФОЛБЭК читается один раз на весь тик
    // компонента, тем же приёмом, что и GlobalDecayRate/RotPurityThreshold
    // выше. Длительность конкретного предмета (карточка приоритетнее
    // фолбэка, 2026-09-05) резолвится ниже, ВНУТРИ цикла по предметам --
    // разным ингредиентам в одном инвентаре нужны разные числа, поэтому
    // читать один раз здесь для всех уже нельзя, см. довод у
    // FIngredientTableRow::DryingDurationSeconds.
    const float GlobalDryingDurationFallback = Settings ? Settings->DryingDurationSeconds : 1920.0f;
    const float DriedDecayMultiplier = Settings ? Settings->DriedItemDecayMultiplier : 0.1f;

    // Отстой/Выпаривание (2026-09-05, многоступенчатые зелья) -- значения
    // читаются один раз на весь тик компонента, тем же приёмом, что и
    // GlobalDryingDurationFallback выше: в отличие от сушки, эти два
    // процесса не резолвят карточку (см. довод у StationType==Potion-гейта
    // ниже -- эффект применяется только к готовому зелью, не к сырой траве,
    // у зелий нет собственной строки DT_IngredientClass с индивидуальными
    // числами).
    const float SettlingDuration = Settings ? Settings->SettlingDurationSeconds : 960.0f;
    const float SettlingDominantAxisBoost = Settings ? Settings->SettlingDominantAxisBoost : 0.15f;
    const float SettlingMagnitudeLossFactor = Settings ? Settings->SettlingMagnitudeLossFactor : 0.9f;

    const float EvaporationDuration = Settings ? Settings->EvaporationDurationSeconds : 1920.0f;
    const float EvaporationMagnitudeBoost = Settings ? Settings->EvaporationMagnitudeBoost : 1.2f;
    const float EvaporationPotencyBoost = Settings ? Settings->EvaporationPotencyBoost : 0.15f;
    const float EvaporationRiskMultiplier = Settings ? Settings->EvaporationRiskMultiplier : 1.2f;

    // "Potion" — тот же жёстко зашитый общий IngredientID готового зелья,
    // что уже AHerbalistPlayerController::UsePotion/AlchemySlotWidget.cpp
    // используют инлайн (не заводим новую именованную константу поверх уже
    // устоявшегося в проекте приёма). Отстой/Выпаривание применяются ТОЛЬКО
    // к готовым зельям (не к сырым травам/минералам) -- "усиление
    // доминирующей оси"/"концентрация" описаны пользователем именно про
    // готовый состав, у которого уже есть осмысленная доминанта из смешения
    // нескольких ингредиентов; одиночная трава, случайно оказавшаяся на
    // этих станциях, была бы всего лишь странно резонирующей с формулой,
    // рассчитанной на другой случай.
    static const FName PotionIngredientID(TEXT("Potion"));

    for (FInventoryItem& Item : Items)
    {
        if (Item.bSubjectToDecay)
        {
            // Вода живёт в отдельной таблице (FWaterTypeRow), не в
            // FIngredientTableRow — резолвить нужно по bIsWater, иначе
            // множитель воды тихо падает на дефолт 1.0, как у обычной травы
            // (05_Systems.md требует, чтобы вода портилась быстрее).
            float IngredientDecay = 1.0f;
            if (Item.bIsWater)
            {
                if (WaterReg)
                {
                    if (const FWaterTypeRow* Row = WaterReg->GetWaterType(Item.IngredientID))
                    {
                        IngredientDecay = Row->DecayRate;
                    }
                }
            }
            else if (IngredientReg)
            {
                if (const FIngredientTableRow* Row = IngredientReg->GetRow(Item.IngredientID))
                {
                    IngredientDecay = Row->DecayRate;
                }
            }

            // Сушёный предмет портится на порядки медленнее свежего (2026-09-04)
            // -- ОТДЕЛЬНЫЙ множитель от ContainerDecayMultiplier выше (сушка
            // про воду в самом растении, тара -- про воздух/влагу вокруг
            // него, см. довод у DriedItemDecayMultiplier, HerbalistSettings.h).
            // Читается ДО возможного завершения сушки этим же тиком ниже --
            // предмет, досохший только что, получает пониженный decay уже
            // со СЛЕДУЮЩЕГО тика, не задним числом на этот же.
            const float ItemDecayMultiplier = Item.bIsDried ? DriedDecayMultiplier : 1.0f;

            ApplyDecayToItem(Item, DecayUpdateInterval, EffectiveGlobalDecayRate * IngredientDecay * ItemDecayMultiplier);

            // Гниение как терминальное состояние (2026-08-29) -- только
            // не-водная органика (вода протухшая -- не то же самое, что
            // трава, сгнившая в перегной; минералы/обереги сюда не попадают
            // сами по себе, у них DecayRate=0, порог никогда не достигается).
            // ShouldConvertToPeregnoy -- чистая функция от State (без
            // обращения к реестрам), тестируется напрямую без GameInstance.
            if (!Item.bIsWater && ShouldConvertToPeregnoy(Item.State.Meta, RotPurityThreshold, RotDistortionThreshold))
            {
                Item.IngredientID = PeregnoyIngredientID;
                Item.bSubjectToDecay = false;   // терминальное состояние, дальше решать нечего
                if (IngredientReg)
                {
                    if (const FIngredientTableRow* PeregnoyRow = IngredientReg->GetRow(PeregnoyIngredientID))
                    {
                        Item.State = PeregnoyRow->BaseState;
                    }
                }
            }
            // Сушилка (2026-09-04) -- только если этот КОНКРЕТНЫЙ инвентарь
            // сейчас сушилка (StationType==DryingRack, обобщено с bool
            // bIsDryingRack 2026-09-05, см. довод у EProcessingStationType,
            // HerbalistInventoryComponent.h), предмет не вода (сушат
            // листья/корни/грибы, не воду) и ещё не высох. "else if", не
            // отдельный if -- предмет, только что превратившийся в Перегной
            // строкой выше, больше не тот ингредиент, которым был секунду
            // назад, и сушить (взводить его же таймер) уже нечего в этом же
            // тике.
            else if (StationType == EProcessingStationType::DryingRack && !Item.bIsWater && !Item.bIsDried)
            {
                // Приоритет карточки над глобальным фолбэком (2026-09-05,
                // "процесс сушки у разных растений разный") -- резолв самой
                // карточки здесь (обращение к реестру), решение "какое число
                // использовать" -- в чистой ResolveDryingDurationSeconds.
                const FIngredientTableRow* DryingRow = IngredientReg ? IngredientReg->GetRow(Item.IngredientID) : nullptr;
                const float ItemDryingDuration = ResolveDryingDurationSeconds(DryingRow, GlobalDryingDurationFallback);

                if (TickDryingItem(Item, DecayUpdateInterval, ItemDryingDuration))
                {
                    // Только что досохло этим тиком -- честная дельта
                    // алхимических осей (если карточка её несёт, см. довод у
                    // FIngredientTableRow::DriedStateDelta). DryingRow уже
                    // резолвлен выше для длительности -- переиспользуем, не
                    // ходим в реестр второй раз за тот же предмет.
                    if (DryingRow)
                    {
                        ApplyDriedStateDelta(Item.State.Meta, DryingRow->DriedStateDelta);
                    }
                }
            }
            // Отстойник (2026-09-05) -- только готовое зелье (см. довод у
            // PotionIngredientID выше), ещё не отстоявшееся.
            else if (StationType == EProcessingStationType::SettlingStand && !Item.bIsWater
                && !Item.bHasSettled && Item.IngredientID == PotionIngredientID)
            {
                if (TickSettlingItem(Item, DecayUpdateInterval, SettlingDuration))
                {
                    ApplySettlingEffect(Item.State, SettlingDominantAxisBoost, SettlingMagnitudeLossFactor);
                }
            }
            // Выпарной куб (2026-09-05) -- та же оговорка "только готовое
            // зелье", ещё не выпаренное.
            else if (StationType == EProcessingStationType::EvaporationStill && !Item.bIsWater
                && !Item.bHasEvaporated && Item.IngredientID == PotionIngredientID)
            {
                if (TickEvaporationItem(Item, DecayUpdateInterval, EvaporationDuration))
                {
                    ApplyEvaporationEffect(Item.State, EvaporationMagnitudeBoost, EvaporationPotencyBoost, EvaporationRiskMultiplier);
                }
            }
        }
    }
}

bool UHerbalistInventoryComponent::ShouldConvertToPeregnoy(const FMeta& Meta, float PurityThreshold, float DistortionThreshold)
{
    return Meta.Purity < PurityThreshold && Meta.Distortion > DistortionThreshold;
}

bool UHerbalistInventoryComponent::TickDryingItem(FInventoryItem& Item, float DeltaTime, float DryingDurationSeconds)
{
    if (Item.DryingTimeRemainingSeconds < 0.0f)
    {
        // Первое попадание в сушилку -- взводим таймер, ничего не решаем
        // этим же вызовом (симметрично тому, как StartRegeneration ставит
        // таймер и ждёт СЛЕДУЮЩЕГО срабатывания, не решает синхронно).
        Item.DryingTimeRemainingSeconds = DryingDurationSeconds;
        return false;
    }

    Item.DryingTimeRemainingSeconds -= DeltaTime;
    if (Item.DryingTimeRemainingSeconds <= 0.0f)
    {
        Item.DryingTimeRemainingSeconds = 0.0f;
        Item.bIsDried = true;
        return true;
    }
    return false;
}

float UHerbalistInventoryComponent::ResolveDryingDurationSeconds(const FIngredientTableRow* Row, float GlobalFallbackSeconds)
{
    if (Row && Row->DryingDurationSeconds >= 0.0f)
    {
        return Row->DryingDurationSeconds;
    }
    return GlobalFallbackSeconds;
}

void UHerbalistInventoryComponent::ApplyDriedStateDelta(FMeta& Meta, const FMeta& Delta)
{
    Meta.Distortion = FMath::Clamp(Meta.Distortion + Delta.Distortion, 0.0f, 1.0f);
    Meta.Stability   = FMath::Clamp(Meta.Stability  + Delta.Stability,  0.0f, 1.0f);
    Meta.Purity      = FMath::Clamp(Meta.Purity     + Delta.Purity,     0.0f, 1.0f);
    Meta.Potency     = FMath::Clamp(Meta.Potency    + Delta.Potency,    0.0f, 1.0f);
    Meta.Resonance   = FMath::Clamp(Meta.Resonance  + Delta.Resonance,  0.0f, 1.0f);
    Meta.Corruption  = FMath::Clamp(Meta.Corruption + Delta.Corruption, 0.0f, 1.0f);
}

bool UHerbalistInventoryComponent::TickSettlingItem(FInventoryItem& Item, float DeltaTime, float SettlingDurationSeconds)
{
    if (Item.SettlingTimeRemainingSeconds < 0.0f)
    {
        // Первое попадание на отстойник -- взводим таймер, ничего не решаем
        // этим же вызовом (тот же приём, что TickDryingItem).
        Item.SettlingTimeRemainingSeconds = SettlingDurationSeconds;
        return false;
    }

    Item.SettlingTimeRemainingSeconds -= DeltaTime;
    if (Item.SettlingTimeRemainingSeconds <= 0.0f)
    {
        Item.SettlingTimeRemainingSeconds = 0.0f;
        Item.bHasSettled = true;
        return true;
    }
    return false;
}

void UHerbalistInventoryComponent::ApplySettlingEffect(FRealState& State, float DominantAxisBoost, float MagnitudeLossFactor)
{
    // argmax по четырём осям Direction -- при равенстве побеждает первая по
    // порядку объявления (Body), тот же произвольный, но детерминированный
    // порядок тай-брейка, что уже неявно есть у любого линейного сравнения
    // в проекте (не важно какая из равных осей "доминирует", лишь бы
    // предсказуемо).
    float* Dominant = &State.Direction.Body;
    if (State.Direction.Mind > *Dominant)   Dominant = &State.Direction.Mind;
    if (State.Direction.Spirit > *Dominant) Dominant = &State.Direction.Spirit;
    if (State.Direction.Nature > *Dominant) Dominant = &State.Direction.Nature;
    *Dominant += DominantAxisBoost;

    // Остальные три оси просаживаются пропорционально сами -- NormalizeSum()
    // уже существует (HerbalistCoreTypes.h), отдельно вычитать не нужно
    // (прямая инструкция задачи).
    State.Direction.NormalizeSum();

    // Цена: часть силы уходит в осадок.
    State.Magnitude *= MagnitudeLossFactor;
}

bool UHerbalistInventoryComponent::TickEvaporationItem(FInventoryItem& Item, float DeltaTime, float EvaporationDurationSeconds)
{
    if (Item.EvaporationTimeRemainingSeconds < 0.0f)
    {
        Item.EvaporationTimeRemainingSeconds = EvaporationDurationSeconds;
        return false;
    }

    Item.EvaporationTimeRemainingSeconds -= DeltaTime;
    if (Item.EvaporationTimeRemainingSeconds <= 0.0f)
    {
        Item.EvaporationTimeRemainingSeconds = 0.0f;
        Item.bHasEvaporated = true;
        return true;
    }
    return false;
}

void UHerbalistInventoryComponent::ApplyEvaporationEffect(FRealState& State, float MagnitudeBoost, float PotencyBoost, float RiskMultiplier)
{
    // Усиление: Magnitude/Potency растут (Min/Clamp вместо простого
    // умножения+прибавления -- обе оси имеют потолок 1.0, ApplyEvaporationEffect
    // не должен вывести их за пределы, как и остальные Meta-функции проекта).
    State.Magnitude = FMath::Min(State.Magnitude * MagnitudeBoost, 1.0f);
    State.Meta.Potency = FMath::Clamp(State.Meta.Potency + PotencyBoost, 0.0f, 1.0f);

    // Цена: концентрируется и грязь тоже -- тот же RiskMultiplier на ОБЕИХ
    // осях порчи (прямая формулировка задачи: "концентрация не разбирает,
    // что усиливать").
    State.Meta.Distortion = FMath::Clamp(State.Meta.Distortion * RiskMultiplier, 0.0f, 1.0f);
    State.Meta.Corruption = FMath::Clamp(State.Meta.Corruption * RiskMultiplier, 0.0f, 1.0f);
}

void UHerbalistInventoryComponent::ApplyFilterEffect(FMeta& Meta, float PurityBoost, float DistortionReduction, float CorruptionReduction, float PotencyLoss)
{
    Meta.Purity     = FMath::Clamp(Meta.Purity     + PurityBoost,          0.0f, 1.0f);
    Meta.Distortion = FMath::Clamp(Meta.Distortion - DistortionReduction,  0.0f, 1.0f);
    Meta.Corruption = FMath::Clamp(Meta.Corruption - CorruptionReduction,  0.0f, 1.0f);
    // Цена: чище, но слабее -- реальный компромисс фильтрации/отжима.
    Meta.Potency    = FMath::Clamp(Meta.Potency    - PotencyLoss,          0.0f, 1.0f);
}

bool UHerbalistInventoryComponent::TryFilterPotion()
{
    // Тот же простой селектор, что уже AHerbalistPlayerController::UsePotion
    // -- первый предмет с IngredientID=="Potion" && Count>0, не более
    // сложный выбор (проект уже принял эту простоту, прямая инструкция
    // задачи "используй ТОТ ЖЕ приём").
    const int32 PotionIndex = Items.IndexOfByPredicate([](const FInventoryItem& Item)
    {
        return Item.IngredientID == FName(TEXT("Potion")) && Item.Count > 0;
    });
    if (PotionIndex == INDEX_NONE)
    {
        return false;
    }

    const UHerbalistSettings* Settings = GetDefault<UHerbalistSettings>();
    const float PurityBoost = Settings ? Settings->FilterPurityBoost : 0.15f;
    const float DistortionReduction = Settings ? Settings->FilterDistortionReduction : 0.1f;
    const float CorruptionReduction = Settings ? Settings->FilterCorruptionReduction : 0.1f;
    const float PotencyLoss = Settings ? Settings->FilterPotencyLoss : 0.1f;

    ApplyFilterEffect(Items[PotionIndex].State.Meta, PurityBoost, DistortionReduction, CorruptionReduction, PotencyLoss);
    OnInventoryChanged.Broadcast();
    return true;
}

bool UHerbalistInventoryComponent::TryEquipContainer(FName IngredientID, EStorageContainerType GrantsType)
{
    // Карточка не описывает контейнер вовсе (обычная трава/минерал) --
    // нечего экипировать, независимо от владения.
    if (GrantsType == EStorageContainerType::None)
    {
        return false;
    }

    // Владение -- тот же поиск по имени, что уже OfferToCommunity/ActivateWard
    // в контроллере, но здесь без обращения к GameInstance: только Items
    // этого инвентаря. Не списывается -- контейнер носишь, не сжигаешь,
    // тот же принцип, что уже у активации оберегов (ActivateWard).
    bool bOwnsItem = false;
    for (const FInventoryItem& Item : Items)
    {
        if (Item.IngredientID == IngredientID && Item.Count > 0)
        {
            bOwnsItem = true;
            break;
        }
    }
    if (!bOwnsItem)
    {
        return false;
    }

    ContainerType = GrantsType;
    return true;
}

void UHerbalistInventoryComponent::ApplyDecayToItem(FInventoryItem& Item, float DeltaTime, float DecayRate)
{
    const float Instability = 1.0f - Item.State.Meta.Stability;
    const float DecayFactor = DecayRate * DeltaTime * Instability;

    Item.State.Meta.Distortion = FMath::Min(Item.State.Meta.Distortion + DecayFactor * 0.5f, 1.0f);
    Item.State.Meta.Corruption = FMath::Min(Item.State.Meta.Corruption + DecayFactor * 0.3f, 1.0f);
    Item.State.Meta.Purity      = FMath::Max(Item.State.Meta.Purity      - DecayFactor * 0.2f, 0.0f);
    Item.State.Meta.Stability   = FMath::Max(Item.State.Meta.Stability   - DecayFactor * 0.1f, 0.0f);

    Item.State.Direction.Body   = FMath::Clamp(Item.State.Direction.Body   + DecayRng.FRandRange(-0.01f, 0.01f) * Instability, 0.0f, 1.0f);
    Item.State.Direction.Mind   = FMath::Clamp(Item.State.Direction.Mind   + DecayRng.FRandRange(-0.01f, 0.01f) * Instability, 0.0f, 1.0f);
    Item.State.Direction.Spirit = FMath::Clamp(Item.State.Direction.Spirit + DecayRng.FRandRange(-0.01f, 0.01f) * Instability, 0.0f, 1.0f);
    Item.State.Direction.Nature = FMath::Clamp(Item.State.Direction.Nature + DecayRng.FRandRange(-0.01f, 0.01f) * Instability, 0.0f, 1.0f);
    Item.State.Direction.NormalizeSum();
}

bool UHerbalistInventoryComponent::AddItem(const FInventoryItem& Item, int32 Amount)
{
    if (Amount <= 0 || Item.IngredientID.IsNone() || Item.Count <= 0)
    {
        UE_LOG(LogHerbalistInventory, Warning, TEXT("AddItem: invalid parameters"));
        return false;
    }

    int32 Remaining = Amount;
    FInventoryItem TempItem = Item;

    while (Remaining > 0)
    {
        int32 SlotIndex = FindStackableSlot(TempItem);
        if (SlotIndex == INDEX_NONE) break;

        FInventoryItem& Slot = Items[SlotIndex];
        int32 Space = MAX_STACK_SIZE - Slot.Count;
        int32 ToAdd = FMath::Min(Remaining, Space);

        MergeStack(Slot, TempItem, ToAdd);
        Remaining -= ToAdd;
    }

    while (Remaining > 0)
    {
        if (Items.Num() >= MaxSlots)
        {
            UE_LOG(LogHerbalistInventory, Warning, TEXT("AddItem: inventory full, %d items not added"), Remaining);
            break;
        }

        FInventoryItem NewSlot = TempItem;
        NewSlot.Count = FMath::Min(Remaining, MAX_STACK_SIZE);
        Items.Add(NewSlot);
        Remaining -= NewSlot.Count;
    }

    if (Amount != Remaining)
    {
        OnInventoryChanged.Broadcast();
        return true;
    }
    return false;
}

bool UHerbalistInventoryComponent::RemoveItem(int32 Index, int32 Amount)
{
    if (!Items.IsValidIndex(Index) || Amount <= 0) return false;

    FInventoryItem& Slot = Items[Index];
    if (Slot.Count < Amount) return false;

    Slot.Count -= Amount;
    if (Slot.Count <= 0)
    {
        Items.RemoveAt(Index);
    }

    OnInventoryChanged.Broadcast();
    return true;
}

bool UHerbalistInventoryComponent::TransferOneItem(int32 SourceIndex, int32 TargetIndex)
{
    if (!Items.IsValidIndex(SourceIndex) || !Items.IsValidIndex(TargetIndex)) return false;

    FInventoryItem& Source = Items[SourceIndex];
    FInventoryItem& Target = Items[TargetIndex];

    if (Source.Count <= 0) return false;

    if (Target.IngredientID.IsNone() || Target.Count == 0)
    {
        Target = Source;
        Target.Count = 1;
        Source.Count -= 1;
        if (Source.Count <= 0)
            Items.RemoveAt(SourceIndex);
        OnInventoryChanged.Broadcast();
        return true;
    }

    if (Target.IngredientID == Source.IngredientID && Target.Count < MAX_STACK_SIZE && AreItemsStackable(Target, Source))
    {
        int32 Space = MAX_STACK_SIZE - Target.Count;
        int32 ToAdd = FMath::Min(1, Space);
        if (ToAdd > 0)
        {
            MergeStack(Target, Source, ToAdd);
            Source.Count -= ToAdd;
            if (Source.Count <= 0)
                Items.RemoveAt(SourceIndex);
            OnInventoryChanged.Broadcast();
            return true;
        }
    }

    return false;
}

bool UHerbalistInventoryComponent::TransferItemTo(int32 SourceIndex, UHerbalistInventoryComponent* TargetInventory)
{
    if (!TargetInventory || !Items.IsValidIndex(SourceIndex))
        return false;

    FInventoryItem& SourceItem = Items[SourceIndex];
    if (SourceItem.Count <= 0)
        return false;

    if (TargetInventory->AddItem(SourceItem, 1))
    {
        SourceItem.Count--;
        if (SourceItem.Count <= 0)
            Items.RemoveAt(SourceIndex);
        OnInventoryChanged.Broadcast();
        return true;
    }
    return false;
}

bool UHerbalistInventoryComponent::SplitStack(int32 Index, int32 Amount, FInventoryItem& OutItem)
{
    if (!Items.IsValidIndex(Index) || Amount <= 0)
        return false;

    FInventoryItem& Source = Items[Index];
    if (Source.Count <= Amount)
        return false;

    OutItem = Source;
    OutItem.Count = Amount;

    Source.Count -= Amount;
    OnInventoryChanged.Broadcast();
    return true;
}

const FInventoryItem* UHerbalistInventoryComponent::GetSlot(int32 Index) const
{
    return Items.IsValidIndex(Index) ? &Items[Index] : nullptr;
}

void UHerbalistInventoryComponent::Clear()
{
    Items.Empty();
    OnInventoryChanged.Broadcast();
}

int32 UHerbalistInventoryComponent::FindStackableSlot(const FInventoryItem& Item) const
{
    for (int32 i = 0; i < Items.Num(); ++i)
    {
        const FInventoryItem& Slot = Items[i];
        if (Slot.IngredientID == Item.IngredientID && Slot.Count < MAX_STACK_SIZE && AreItemsStackable(Slot, Item))
            return i;
    }
    return INDEX_NONE;
}

bool UHerbalistInventoryComponent::AreItemsStackable(const FInventoryItem& A, const FInventoryItem& B) const
{
    if (A.IngredientID != B.IngredientID) return false;

    // Посадочный материал (DESIGN_Community_And_Homestead.md §2.4, PlantSeed,
    // 2026-09-04) несёт тот же IngredientID, что обычный собранный
    // ингредиент того же вида (bIsPlantingStock — единственное, что их
    // различает) — без этой проверки они бы молча слились в один стек при
    // первом же AddItem (MergeStack не трогает bIsPlantingStock вовсе), и
    // PlantSeed либо TestNewApply нашли бы в инвентаре не то, что искали.
    if (A.bIsPlantingStock != B.bIsPlantingStock) return false;

    // Сушка (2026-09-04): сушёный и свежий предмет одного вида -- уже разные
    // алхимические сущности (DriedStateDelta честно меняет часть карточек,
    // см. IngredientTableRow.h), тот же довод, что и у bIsPlantingStock
    // строкой выше -- молчаливое слияние стёрло бы это различие.
    if (A.bIsDried != B.bIsDried) return false;

    // Предмет, уже сушащийся (таймер взведён, но ещё не досчитал), не
    // стекуется вовсе -- даже с другим таким же "в процессе" предметом: у
    // каждого слота свой независимый DryingTimeRemainingSeconds, а MergeStack
    // ниже это поле не усредняет (в отличие от CreationTime), молчаливое
    // слияние либо потеряло бы прогресс одного из двух, либо держало бы в
    // одном слоте два разных таймера одновременно -- оба варианта хуже,
    // чем просто не дать таким предметам разделить слот, пока сушка идёт.
    if (A.DryingTimeRemainingSeconds >= 0.0f || B.DryingTimeRemainingSeconds >= 0.0f) return false;

    // Отстой/Выпаривание (2026-09-05) -- тот же довод и тот же приём, что
    // bIsDried/DryingTimeRemainingSeconds выше: терминальный флаг различает
    // алхимические сущности (отстоявшееся/выпаренное зелье уже не то же
    // самое, чем было), а предмет, ещё не досчитавший свой таймер, не
    // стекуется вовсе -- ни с другим "в процессе", ни с уже завершённым
    // (у каждого свой независимый *TimeRemainingSeconds, MergeStack его не
    // усредняет).
    if (A.bHasSettled != B.bHasSettled) return false;
    if (A.SettlingTimeRemainingSeconds >= 0.0f || B.SettlingTimeRemainingSeconds >= 0.0f) return false;

    if (A.bHasEvaporated != B.bHasEvaporated) return false;
    if (A.EvaporationTimeRemainingSeconds >= 0.0f || B.EvaporationTimeRemainingSeconds >= 0.0f) return false;

    return HerbalistCore::Math::AreStatesSimilar(A.State, B.State);
}

void UHerbalistInventoryComponent::MergeStack(FInventoryItem& Target, const FInventoryItem& Source, int32 AddedCount)
{
    int32 NewCount = Target.Count + AddedCount;
    float OldWeight = (float)Target.Count / NewCount;
    float NewWeight = (float)AddedCount / NewCount;

    FRealState& T = Target.State;
    const FRealState& S = Source.State;

    T.Magnitude = T.Magnitude * OldWeight + S.Magnitude * NewWeight;

    T.Direction.Body   = T.Direction.Body   * OldWeight + S.Direction.Body   * NewWeight;
    T.Direction.Mind   = T.Direction.Mind   * OldWeight + S.Direction.Mind   * NewWeight;
    T.Direction.Spirit = T.Direction.Spirit * OldWeight + S.Direction.Spirit * NewWeight;
    T.Direction.Nature = T.Direction.Nature * OldWeight + S.Direction.Nature * NewWeight;
    T.Direction.NormalizeSum();

    T.Meta.Distortion = T.Meta.Distortion * OldWeight + S.Meta.Distortion * NewWeight;
    T.Meta.Stability  = T.Meta.Stability  * OldWeight + S.Meta.Stability  * NewWeight;
    T.Meta.Purity     = T.Meta.Purity     * OldWeight + S.Meta.Purity     * NewWeight;
    T.Meta.Potency    = T.Meta.Potency    * OldWeight + S.Meta.Potency    * NewWeight;
    T.Meta.Resonance  = T.Meta.Resonance  * OldWeight + S.Meta.Resonance  * NewWeight;
    T.Meta.Corruption = T.Meta.Corruption * OldWeight + S.Meta.Corruption * NewWeight;

    float DistortionDiff = FMath::Abs(T.Meta.Distortion - S.Meta.Distortion);
    T.Meta.Distortion = FMath::Clamp(T.Meta.Distortion + DistortionDiff * 0.15f, 0.0f, 1.0f);

    float PurityDiff = FMath::Abs(T.Meta.Purity - S.Meta.Purity);
    T.Meta.Purity = FMath::Clamp(T.Meta.Purity - PurityDiff * 0.1f, 0.0f, 1.0f);

    Target.CreationTime = Target.CreationTime * OldWeight + Source.CreationTime * NewWeight;
    Target.bSubjectToDecay = Target.bSubjectToDecay && Source.bSubjectToDecay;

    Target.Count = NewCount;
}

// ---- CaptureState & ApplyStateDelta (единственные правильные версии) ----
FInventorySnapshot UHerbalistInventoryComponent::CaptureState() const
{
    FInventorySnapshot Snapshot;
    Snapshot.ContainerContents.Add(0, Items);   // 0 – ID игрока
    return Snapshot;
}

void UHerbalistInventoryComponent::ApplyStateDelta(const FStateDelta& Delta)
{
    bool bChanged = false;
    for (const FInventoryOperation& Op : Delta.InventoryOps)
    {
        if (Op.ContainerID != 0) continue;   // только инвентарь игрока (ID = 0)

        if (Op.OpType == EInventoryOpType::Add)
        {
            if (AddItem(Op.Ingredient, Op.Amount))
                bChanged = true;
        }
        else if (Op.OpType == EInventoryOpType::Remove)
        {
            for (int32 i = 0; i < Items.Num(); ++i)
            {
                if (Items[i].IngredientID == Op.Ingredient.IngredientID)
                {
                    if (RemoveItem(i, FMath::Min(Op.Amount, Items[i].Count)))
                        bChanged = true;
                    break;
                }
            }
        }
    }
    if (bChanged)
        OnInventoryChanged.Broadcast();
}