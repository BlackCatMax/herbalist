// Core/Simulation/Private/PerceptionComponent.cpp
#include "Core/Simulation/Public/PerceptionComponent.h"
#include "Core/Simulation/Public/SnapshotService.h"
#include "Core/Simulation/Private/PerceptionService.h"
#include "Core/World/GridWorldManager.h"
#include "Engine/World.h"
#include "EngineUtils.h"

UPerceptionComponent::UPerceptionComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickInterval = 0.5f; // обновляем дважды в секунду
}

void UPerceptionComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    // Мир (GetPerceivedWorld) больше НЕ считается здесь -- 2026-09-03, см.
    // подробный комментарий в .h. Тут остался только инвентарь: маленький,
    // реально используется (InventorySlotWidget/ItemTooltipWidget), дёшев
    // даже безусловно каждые 0.5с.

    // GlobalPerceptionClarity (обсуждение в сессии 2026-08-24, "Прогрессия
    // через Заряну") — компонент живёт прямо на AGridWorldManager (см.
    // AGridWorldManager::PerceptionComponent), не нужен отдельный поиск.
    const AGridWorldManager* Owner = Cast<AGridWorldManager>(GetOwner());
    const float Clarity = Owner ? Owner->GetGlobalPerceptionClarity() : 0.0f;

    // Больше НЕ общий Rng от тикового сида (аудит 2026-09-05: держи тултип
    // открытым 10-15с — каждый опрос давал новый шум от свежего
    // CurrentTickID, усреднение таких независимых просмотров восстанавливало
    // честное S_real в обход всей механики искажения). ComputePerceivedInventory
    // сама сеет шум каждого предмета от его identity+State — см. подробный
    // комментарий в PerceptionService.h/.cpp.
    FInventorySnapshot InvSnap = Simulation::FSnapshotService::CaptureInventory();
    CachedPerceivedInventory = Simulation::FPerceptionService::ComputePerceivedInventory(InvSnap, Clarity);
}

const FPerceivedWorld& UPerceptionComponent::GetPerceivedWorld() const
{
    // Тяжёлый путь (весь грид) — считается по требованию, не в тике. См.
    // подробный комментарий в .h: до 2026-09-03 это безусловно платилось
    // каждые 0.5с, хотя ни один вызывающий в проекте сюда не заходил.
    // const_cast — тот же приём, что уже у UIngredientRegistrySubsystem::
    // EnsureLoaded: снаружи чтение остаётся логически константным, меняется
    // только момент заполнения кэша.
    FWorldSnapshot WorldSnap = Simulation::FSnapshotService::CaptureWorld();
    if (WorldSnap.GridState.Num() == 0)
    {
        // Мир ещё не инициализирован -- отдаём то, что уже было в кэше
        // (пусто при самом первом вызове), не перетираем валидный кэш
        // пустышкой посреди сессии.
        return CachedPerceivedWorld;
    }

    const AGridWorldManager* Owner = Cast<AGridWorldManager>(GetOwner());
    const float Clarity = Owner ? Owner->GetGlobalPerceptionClarity() : 0.0f;

    FRandomStream Rng(WorldSnap.WorldSeed);
    const_cast<UPerceptionComponent*>(this)->CachedPerceivedWorld =
        Simulation::FPerceptionService::ComputePerceivedWorld(WorldSnap, Rng, Clarity);
    return CachedPerceivedWorld;
}
