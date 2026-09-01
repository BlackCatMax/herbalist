// Core/World/GridWorldManagerArtifacts.cpp
//
// Артефакты Легендарных сущностей (02_GDD/21_Journey_And_Artifacts.md
// §21.3-21.4, 2026-09-01, ревизия "Ending and artifacts"). Дубинка (Дубыня)
// изъята из дизайна этой ревизией — Смешанный лес теперь Баба-Яга/
// Шапка-невидимка (см. GridWorldManagerBases.cpp/LegendaryEntityTypes.h).
// Восемь эффектов §21.3 переписаны с "модификатора клетки" на "действие
// игрока", реализованы отдельными Exec-командами на контроллере, не здесь
// — этот файл остаётся общей инфраструктурой (реестр + проверка
// проявленности + честный/обманный путь получения), не местом для эффектов.

#include "Core/World/GridWorldManager.h"
#include "Core/Config/HerbalistSettings.h"
#include "Core/Simulation/Private/PerceptionService.h"
#include "Player/HerbalistPlayerController.h"
#include "ProjectHerbalist.h"
#include "HerbalistLogChannels.h"

bool AGridWorldManager::IsArtifactWarmed(FName ArtifactID) const
{
    const UHerbalistSettings* Settings = GetHerbalistSettings();

    // Фонарь — исключение (§21.4): греется от общей GlobalPerceptionClarity,
    // не от Warmth/зелья/региона. Не требует записи в AcquiredArtifacts для
    // этой проверки саму по себе, но в игре недостижимо без предварительной
    // честной... то есть обманной добычи (bDeceptionOnly) — проверка ниже
    // остаётся честной сама по себе, вызывающая сторона уже гарантирует
    // добычу перед использованием эффекта.
    if (ArtifactID == FName(TEXT("Фонарь")))
    {
        const float ClarityThreshold = Settings ? Settings->LanternWarmClarityThreshold : 0.7f;
        return GlobalPerceptionClarity >= ClarityThreshold;
    }

    const float WarmthThreshold = Settings ? Settings->ArtifactWarmthThreshold : 1.0f;
    for (const FAcquiredArtifact& Artifact : AcquiredArtifacts)
    {
        if (Artifact.ArtifactID == ArtifactID)
        {
            return Artifact.Warmth >= WarmthThreshold;
        }
    }
    return false;
}

bool AGridWorldManager::IsLegendaryManifested(FName EntityID) const
{
    const FIntPoint* Anchor = LegendaryAnchors.Find(EntityID);
    if (!Anchor) return false;

    const FGridCell* Cell = GetCellConst(Anchor->X, Anchor->Y);
    return Cell && Cell->ManifestedEntityID == EntityID;
}

bool AGridWorldManager::IsBereginyaManifested() const
{
    static const FName BereginyaID(TEXT("Берегиня"));
    for (const FGridCell& Cell : Cells)
    {
        if (Cell.ManifestedEntityID == BereginyaID) return true;
    }
    return false;
}

bool AGridWorldManager::TryAcquireArtifact(FName ArtifactID, const TArray<FInventoryItem>& Offered, bool& bOutViaDeception)
{
    bOutViaDeception = false;
    if (Offered.Num() == 0) return false;

    const FArtifactDefinition* Def = FindArtifactDefinition(ArtifactID);
    if (!Def) return false;

    // Уже добыт — не повторно (Зеркальце/Клубочек теперь тоже в этом
    // списке, см. комментарий у AcquiredArtifacts.Add ниже).
    for (const FAcquiredArtifact& Existing : AcquiredArtifacts)
    {
        if (Existing.ArtifactID == ArtifactID) return false;
    }

    // Доступен только когда сущность уже проявлена (§21.3) — Гребень идёт
    // через отдельную ветку (Берегиня не в LegendaryEntityTypes.h).
    const bool bManifested = Def->LegendaryEntityID.IsNone()
        ? IsBereginyaManifested()
        : IsLegendaryManifested(Def->LegendaryEntityID);
    if (!bManifested) return false;

    // Честный/обманный путь — то же различие S_real/S_Perceived, что уже
    // отличает тултип (AlchemySlotWidget.cpp) от настоящего Cell.State.
    // Собственный фиксированный сид на артефакт, не WorldRNG — наблюдение/
    // оценка подношения не должна возмущать детерминированный поток
    // симуляции (тот же приём, что уже MirrorPerceptionRng/PerceptionRng).
    FRandomStream Rng(20260901 + GetTypeHash(ArtifactID));
    float SumRealPurity = 0.0f;
    float SumPerceivedPurity = 0.0f;
    for (const FInventoryItem& Item : Offered)
    {
        SumRealPurity += Item.State.Meta.Purity;
        SumPerceivedPurity += Simulation::FPerceptionService::PerceiveRealState(Item.State, Rng, GlobalPerceptionClarity).Meta.Purity;
    }
    const float AvgRealPurity = SumRealPurity / Offered.Num();
    const float AvgPerceivedPurity = SumPerceivedPurity / Offered.Num();

    const UHerbalistSettings* Settings = GetHerbalistSettings();
    const float HonestThreshold = Settings ? Settings->ArtifactHonestPurityThreshold : 0.6f;

    if (Def->bDeceptionOnly)
    {
        // Фонарь — единственный артефакт только через обман (§21.3: Болото
        // не держит благого Легендарного, честного пути к нему нет вовсе).
        // Настоящий Purity подношения не открывает отдельный честный путь —
        // только воспринятый.
        if (AvgPerceivedPurity < HonestThreshold) return false;
        bOutViaDeception = true;
    }
    else if (AvgRealPurity >= HonestThreshold)
    {
        bOutViaDeception = false;
    }
    else if (AvgPerceivedPurity >= HonestThreshold)
    {
        bOutViaDeception = true;
    }
    else
    {
        return false;   // ни честно, ни обманом — подношение просто недостаточно
    }

    // §21.2 (ревизия "Update docs", 2026-09-01): Зеркальце/Клубочек больше
    // не особый случай — "работают по общему правилу §21.3" — тоже
    // получают запись в AcquiredArtifacts (нужна для Warmth/прогрева §21.4,
    // тот же механизм, что у остальных шести). bWarmsCompanionItem теперь
    // значит только "вызывающая сторона (OfferForArtifact) дополнительно
    // выставляет bHasMirror/bHasYarnBall на контроллере", не "пропустить
    // запись здесь".
    FAcquiredArtifact Acquired;
    Acquired.ArtifactID = ArtifactID;
    Acquired.bAcquiredViaDeception = bOutViaDeception;
    AcquiredArtifacts.Add(Acquired);

    // НИТЬ МАТЕРИ (17_Hero_And_Community.md §17.7, Степь, событийный
    // триггер YarnBallAcquired) — Клубочек получен, "второй биом
    // путешествия". Доставляется напрямую здесь, тем же приёмом, что уже
    // BuyanPathChosen/CoherentBrew — не через периодический опрос.
    if (ArtifactID == FName(TEXT("Клубочек")))
    {
        AHerbalistPlayerController* PC = Cast<AHerbalistPlayerController>(GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr);
        CollectMemoryFragment(FName(TEXT("NIT_MATERI")), /*bIsFalse=*/false, PC);
    }

    UE_LOG(LogHerbalistWorld, Log, TEXT("[Artifact] %s acquired (%s), RealPurity=%.2f, PerceivedPurity=%.2f"),
        *ArtifactID.ToString(), bOutViaDeception ? TEXT("via deception") : TEXT("honestly"),
        AvgRealPurity, AvgPerceivedPurity);
    return true;
}
