// Core/Simulation/Private/PipelineV2.cpp
#include "PipelineV2.h"
#include "ProjectHerbalist.h"
#include "HerbalistLogChannels.h"
#include "Core/Types/BiomeTypes.h"
#include "Core/Types/HerbalistCoreMath.h"
#include "Core/Config/HerbalistSettings.h"

namespace Simulation
{
    // ---------------------------------------------------------
    // Вспомогательные функции
    // ---------------------------------------------------------

    static const FInventoryItem* FindItemInSnapshot(const FInventorySnapshot& InvSnap,
                                                    int32 ContainerID,
                                                    FName IngredientID)
    {
        const TArray<FInventoryItem>* Items = InvSnap.ContainerContents.Find(ContainerID);
        if (!Items) return nullptr;

        for (const FInventoryItem& Item : *Items)
        {
            if (Item.IngredientID == IngredientID)
                return &Item;
        }
        return nullptr;
    }

    // Результат сбора = природа ингредиента, подтянутая к состоянию места:
    //     Result = Lerp(BaseState, CellState, k)
    //
    // Ровно то, что описывает 05_Systems.md: «ресурсы не существуют до момента
    // сбора как фиксированные сущности — они формируются в момент взаимодействия
    // как результат преобразования локального состояния».
    //
    // Почему именно интерполяция, а не прежнее `Base + k*(Biome - S0)`:
    // результат Lerp всегда лежит между Base и Cell, оба из [0,1] — выйти за
    // границы **невозможно по построению**, кламп не нужен. Прежняя аддитивная
    // форма была неограниченной (диапазон [-k, 1+k]) и на реальных данных
    // упиралась в кламп в 44% замеров: у сильных трав параметры уже стоят у
    // края, и любой толчок биома в ту же сторону выбивал их за единицу — из-за
    // чего Potency/Resonance становились константой во всех биомах сразу.
    //
    // S0 (Алатырь) из формулы сбора ушёл намеренно: он плохо работал началом
    // координат (лежит вне диапазона реальных биомов по всем шести мета-осям),
    // и теперь свободен для своей настоящей роли — недостижимого ориентира, до
    // которого меряют расстояние в прогрессии.
    // Инструмент сбора (DESIGN_Community_And_Homestead.md §2.3, 2026-08-31) —
    // множитель качества, не шанса спавна: неверный инструмент портит уже
    // найденную траву при сборе, не мешает ей существовать в мире (тот вопрос
    // уже решён IngredientSuitabilityFalloff в IngredientRegistrySubsystem).
    // Реальный фольклор, найденный проходом по компендиуму и дописанный в
    // карточки тем же числом: Плакун-трава/Чистотел (bIronAverse — железо
    // отпугивает силу) и Медуница (bDelicate — костяной нож сохраняет её).
    static float ToolQualityMultiplier(EGatheringTool Tool, bool bIronAverse, bool bDelicate, const UHerbalistSettings* Settings)
    {
        const float BareHands  = Settings ? Settings->GatheringToolBareHandsMultiplier   : 0.7f;
        const float NonIron    = Settings ? Settings->GatheringToolNonIronMultiplier     : 0.9f;
        const float IronAverse = Settings ? Settings->GatheringToolIronAverseMultiplier  : 0.3f;
        const float DelicateBone = Settings ? Settings->GatheringToolDelicateBoneBonus   : 1.15f;

        float Mult;
        switch (Tool)
        {
            case EGatheringTool::CopperBlade: Mult = NonIron; break;
            case EGatheringTool::BoneKnife:   Mult = NonIron; break;
            case EGatheringTool::IronBlade:   Mult = 1.0f; break;
            case EGatheringTool::BareHands:
            default:                          Mult = BareHands; break;
        }

        if (bIronAverse)
        {
            // Уважительный сбор (не железо) на такой траве не наказывается
            // вовсе — полный множитель, не обычный базовый инструмента.
            Mult = (Tool == EGatheringTool::IronBlade) ? IronAverse : 1.0f;
        }

        if (bDelicate && Tool == EGatheringTool::BoneKnife)
        {
            // Перекрывает даже ветку bIronAverse выше — кость и так не
            // железо, получает бонус поверх снятого табу.
            Mult = DelicateBone;
        }

        return Mult;
    }

    static FInventoryItem GenerateHarvestResult(const FGridCell& Cell,
                                               FName IngredientID,
                                               const FRealState& IngredientBaseState,
                                               float IngredientResilience,
                                               EMoonPhase MoonPhase,
                                               EGatheringTool Tool,
                                               bool bIronAverse,
                                               bool bDelicate,
                                               FRandomStream& Rng)
    {
        const UHerbalistSettings* Settings = GetHerbalistSettings();
        const float BiomeWeight = Settings ? Settings->HarvestBiomeWeight : 0.4f;
        const float MoonWaxingBoost = Settings ? Settings->MoonWaxingBoostStrength : 0.15f;
        const float MoonFullBoost   = Settings ? Settings->MoonFullBoostStrength   : 0.15f;

        const FRealState& BiomeState = Cell.State;

        // Если базовое состояние ингредиента не резолвлено (нулевое) —
        // деградируем к биому как единственному источнику.
        const bool bHasBase = IngredientBaseState.Magnitude > KINDA_SMALL_NUMBER || IngredientBaseState.Meta.Distortion > KINDA_SMALL_NUMBER;
        const FRealState& Base = bHasBase ? IngredientBaseState : BiomeState;

        // Сопротивляемость травы гасит влияние места: Resilience=1 — трава
        // собирается ровно собой, Resilience=0 — целиком принимает биом.
        const float K = FMath::Clamp(BiomeWeight * (1.f - FMath::Clamp(IngredientResilience, 0.f, 1.f)), 0.f, 1.f);
        auto Blend = [K](float BaseValue, float CellValue) { return BaseValue + (CellValue - BaseValue) * K; };

        FRealState State;
        State.Direction.Body   = Blend(Base.Direction.Body,   BiomeState.Direction.Body);
        State.Direction.Mind   = Blend(Base.Direction.Mind,   BiomeState.Direction.Mind);
        State.Direction.Spirit = Blend(Base.Direction.Spirit, BiomeState.Direction.Spirit);
        State.Direction.Nature = Blend(Base.Direction.Nature, BiomeState.Direction.Nature);
        State.Magnitude        = Blend(Base.Magnitude,        BiomeState.Magnitude);
        State.Meta.Distortion  = Blend(Base.Meta.Distortion,  BiomeState.Meta.Distortion);
        State.Meta.Stability   = Blend(Base.Meta.Stability,   BiomeState.Meta.Stability);
        State.Meta.Purity      = Blend(Base.Meta.Purity,      BiomeState.Meta.Purity);
        State.Meta.Potency     = Blend(Base.Meta.Potency,     BiomeState.Meta.Potency);
        State.Meta.Resonance   = Blend(Base.Meta.Resonance,   BiomeState.Meta.Resonance);
        State.Meta.Corruption  = Blend(Base.Meta.Corruption,  BiomeState.Meta.Corruption);

        // Лунный цикл (15_Cycles_And_Shrines.md §15.3), v1 — только сбор:
        // Растущая усиливает Body/Nature/Magnitude ("на силу и рост"),
        // Полнолуние — Spirit/Potency/Resonance ("пик, самое сильное окно").
        // Новолуние/Убывающая усиливают применённое зелье, не сбор — не
        // трогаются здесь, это отдельный, ещё не сделанный кусок (влияет на
        // Apply, не Harvest). Direction-компоненты не клампятся тут же —
        // NormalizeSum() ниже сам разберётся с относительным ростом;
        // Magnitude/Potency/Resonance клампятся явно, т.к. дальше их уже
        // никто не трогает (кроме джиттера на Magnitude).
        if (MoonPhase == EMoonPhase::WaxingMoon)
        {
            State.Direction.Body   *= (1.0f + MoonWaxingBoost);
            State.Direction.Nature *= (1.0f + MoonWaxingBoost);
            State.Magnitude = FMath::Clamp(State.Magnitude * (1.0f + MoonWaxingBoost), 0.0f, 1.0f);
        }
        else if (MoonPhase == EMoonPhase::FullMoon)
        {
            State.Direction.Spirit  *= (1.0f + MoonFullBoost);
            State.Meta.Potency   = FMath::Clamp(State.Meta.Potency   * (1.0f + MoonFullBoost), 0.0f, 1.0f);
            State.Meta.Resonance = FMath::Clamp(State.Meta.Resonance * (1.0f + MoonFullBoost), 0.0f, 1.0f);
        }

        // Инструмент сбора (DESIGN_Community_And_Homestead.md §2.3) — тот же
        // приём, что лунный цикл выше, второй потребитель тех же трёх осей.
        // После луны, до джиттера: неверный инструмент портит уже усиленную
        // луной траву, не спорит с ней за первенство.
        const float ToolMult = ToolQualityMultiplier(Tool, bIronAverse, bDelicate, Settings);
        State.Meta.Potency   = FMath::Clamp(State.Meta.Potency   * ToolMult, 0.0f, 1.0f);
        State.Meta.Resonance = FMath::Clamp(State.Meta.Resonance * ToolMult, 0.0f, 1.0f);
        State.Magnitude      = FMath::Clamp(State.Magnitude      * ToolMult, 0.0f, 1.0f);

        // Джиттер — условия сбора (FConditionModifier), которые игрок явно не
        // задаёт. Единственное место, где кламп ещё нужен: шум добавляется
        // поверх и сам по себе границ не соблюдает.
        State.Magnitude = FMath::Clamp(State.Magnitude + Rng.FRandRange(-0.03f, 0.03f), 0.0f, 1.0f);

        State.Direction.NormalizeSum();

        FInventoryItem Result;
        Result.IngredientID = IngredientID;
        Result.State = State;
        Result.Count = 1;
        // CreationTime проставляет вызывающая сторона (ProcessHarvestCommand) из
        // WorldSnap.WorldTime — здесь его не знаем.
        Result.bSubjectToDecay = true;
        // Межбиомная варка (FInventoryItem::SourceBiome, HerbalistCoreTypes.h,
        // 2026-09-04) — Cell уже под рукой, тот же источник, что и BiomeState
        // выше, отдельно протаскивать биом через FHarvestCommand не нужно.
        Result.SourceBiome = Cell.Biome;
        return Result;
    }

    // ---------------------------------------------------------
    // Взвешенное накопление FRealState (используется и для Fold ингредиентов,
    // и для усреднения воды)
    // ---------------------------------------------------------
    static void AccumulateWeighted(FRealState& Agg, const FRealState& S, float Weight)
    {
        Agg.Magnitude += S.Magnitude * Weight;
        Agg.Direction.Body += S.Direction.Body * Weight;
        Agg.Direction.Mind += S.Direction.Mind * Weight;
        Agg.Direction.Spirit += S.Direction.Spirit * Weight;
        Agg.Direction.Nature += S.Direction.Nature * Weight;
        Agg.Meta.Distortion += S.Meta.Distortion * Weight;
        Agg.Meta.Stability += S.Meta.Stability * Weight;
        Agg.Meta.Purity += S.Meta.Purity * Weight;
        Agg.Meta.Potency += S.Meta.Potency * Weight;
        Agg.Meta.Resonance += S.Meta.Resonance * Weight;
        Agg.Meta.Corruption += S.Meta.Corruption * Weight;
    }

    static void DivideRealState(FRealState& Agg, float Divisor)
    {
        if (Divisor <= KINDA_SMALL_NUMBER) return;
        Agg.Magnitude /= Divisor;
        Agg.Direction.Body /= Divisor;
        Agg.Direction.Mind /= Divisor;
        Agg.Direction.Spirit /= Divisor;
        Agg.Direction.Nature /= Divisor;
        Agg.Meta.Distortion /= Divisor;
        Agg.Meta.Stability /= Divisor;
        Agg.Meta.Purity /= Divisor;
        Agg.Meta.Potency /= Divisor;
        Agg.Meta.Resonance /= Divisor;
        Agg.Meta.Corruption /= Divisor;
    }

    // ---------------------------------------------------------
    // Согласие/конфликт трав (2026-08-30, "докручиваем варку, учитывая
    // реальные сильные стороны трав и их сочетаний"). Раньше не-водные
    // ингредиенты сворачивались одним симметричным взвешенным средним —
    // сильная ось одной травы всегда гасилась слабой той же осью другой,
    // независимо от того, реально ли они "спелись". Здесь ингредиенты
    // обрабатываются последовательно (см. основной цикл ниже, "Сборка
    // не-водных ингредиентов"): первый — затравка, каждый следующий
    // РЕАГИРУЕТ на уже накопленный результат.
    // ---------------------------------------------------------

    enum class EDirectionAxis : uint8 { Body, Mind, Spirit, Nature };

    static EDirectionAxis GetDominantDirectionAxis(const FDirection& Dir)
    {
        EDirectionAxis Best = EDirectionAxis::Body;
        float BestValue = Dir.Body;
        if (Dir.Mind > BestValue)   { Best = EDirectionAxis::Mind;   BestValue = Dir.Mind; }
        if (Dir.Spirit > BestValue) { Best = EDirectionAxis::Spirit; BestValue = Dir.Spirit; }
        if (Dir.Nature > BestValue) { Best = EDirectionAxis::Nature; }
        return Best;
    }

    // Голосование по 7 сигналам (6 Meta-осей + совпадение доминанты
    // Direction) — доля согласных минус доля конфликтующих, в [-1, 1].
    // "Согласие" по Meta-оси — обе стороны 0.5 (обе выше или обе ниже),
    // не точное равенство: две разные "порченые" травы всё ещё согласны
    // в том, что зелье должно быть порченым.
    static float ComputeHarmony(const FRealState& Current, const FRealState& Incoming)
    {
        int32 Agree = 0, Total = 0;
        auto Check = [&](float A, float B)
        {
            ++Total;
            const bool bBothHigh = A > 0.5f && B > 0.5f;
            const bool bBothLow  = A < 0.5f && B < 0.5f;
            if (bBothHigh || bBothLow) ++Agree;
        };
        Check(Current.Meta.Distortion, Incoming.Meta.Distortion);
        Check(Current.Meta.Stability,  Incoming.Meta.Stability);
        Check(Current.Meta.Purity,     Incoming.Meta.Purity);
        Check(Current.Meta.Potency,    Incoming.Meta.Potency);
        Check(Current.Meta.Resonance,  Incoming.Meta.Resonance);
        Check(Current.Meta.Corruption, Incoming.Meta.Corruption);
        ++Total;
        if (GetDominantDirectionAxis(Current.Direction) == GetDominantDirectionAxis(Incoming.Direction)) ++Agree;

        return (2.0f * static_cast<float>(Agree) / static_cast<float>(Total)) - 1.0f;
    }

    // Одна Meta-ось реагирует на один входящий ингредиент. Согласие (обе
    // стороны 0.5) — НЕ Lerp к полюсу (тот всегда даёт результат МЕЖДУ
    // текущим значением и полюсом, то есть не выше самого убедительного
    // входа — проверено на реальных числах при разработке, это не
    // "усиление", а просто иначе взвешенное среднее). Вместо этого —
    // "шумные И/ИЛИ" (стандартная форма для сочетания независимых
    // убеждений в одну сторону): 1-(1-A)(1-B)^w при обоих высоких
    // гарантированно даёт результат ВЫШЕ каждого из входов по отдельности
    // (две уверенно "порченые" травы совместно убедительнее одной),
    // симметрично A·B^w при обоих низких. Конфликт (разные стороны 0.5) —
    // тянет к нейтральной середине пропорционально силе расхождения.
    static void ReactAxis(float& Current, float Incoming, float Weight, float AgreementRate, float ConflictRate)
    {
        const bool bBothHigh = Current > 0.5f && Incoming > 0.5f;
        const bool bBothLow  = Current < 0.5f && Incoming < 0.5f;
        const float W = FMath::Clamp(Weight * AgreementRate, 0.0f, 1.0f);
        if (bBothHigh)
        {
            Current = 1.0f - (1.0f - Current) * FMath::Pow(1.0f - Incoming, W);
        }
        else if (bBothLow)
        {
            Current = Current * FMath::Pow(FMath::Max(Incoming, KINDA_SMALL_NUMBER), W);
        }
        else
        {
            const float Discord = FMath::Abs(Current - Incoming);
            Current = FMath::Lerp(Current, 0.5f, Weight * Discord * ConflictRate);
        }
    }

    // Direction — симплекс (сумма=1), не независимая биполярная ось: здесь
    // "согласие" значит "та же доминирующая ось у обоих", а не то же число.
    static void ReactDirection(FDirection& Current, const FDirection& Incoming, float Weight, float AgreementRate, float ConflictRate)
    {
        if (GetDominantDirectionAxis(Current) == GetDominantDirectionAxis(Incoming))
        {
            // Согласны в характере -- усиливаем уже доминирующую ось.
            const float Rate = Weight * AgreementRate * 0.5f;
            switch (GetDominantDirectionAxis(Current))
            {
                case EDirectionAxis::Body:   Current.Body   = FMath::Min(1.0f, Current.Body   + Rate); break;
                case EDirectionAxis::Mind:   Current.Mind   = FMath::Min(1.0f, Current.Mind   + Rate); break;
                case EDirectionAxis::Spirit: Current.Spirit = FMath::Min(1.0f, Current.Spirit + Rate); break;
                case EDirectionAxis::Nature: Current.Nature = FMath::Min(1.0f, Current.Nature + Rate); break;
            }
        }
        else
        {
            // Разные характеры -- размывает специализацию к равномерной
            // (0.25 каждой оси), не выраженный характер вместо конфликта осей.
            const float Rate = Weight * ConflictRate * 0.5f;
            Current.Body   = FMath::Lerp(Current.Body,   0.25f, Rate);
            Current.Mind   = FMath::Lerp(Current.Mind,   0.25f, Rate);
            Current.Spirit = FMath::Lerp(Current.Spirit, 0.25f, Rate);
            Current.Nature = FMath::Lerp(Current.Nature, 0.25f, Rate);
        }
        Current.NormalizeSum();
    }

    // ---------------------------------------------------------
    // L1 (симплекс, сумма=1) <-> единичный 4D-вектор — нужен для Morok/Zaryana,
    // где по легаси-коду требуется геометрия сферы (вращение/смешивание осей),
    // а не барицентрические координаты. Легаси (Core/Pipeline/PipelineMorok.cpp,
    // PipelineZaryana.cpp, до коммита 1539015) использовал отдельный тип
    // FL2Direction с FRngState для вырожденного случая (ToL2/NormalizeL2,
    // HerbalistCoreTypes.h) — здесь это не нужно: после FDirection::NormalizeSum()
    // вектор гарантированно не нулевой (вырожденный случай уже даёт 0.25 по
    // каждой оси), а Pipeline держит единый тип ГПСЧ (FRandomStream), не смешивая
    // его с FRngState.
    // ---------------------------------------------------------
    static FVector4 DirectionToUnitVector(const FDirection& Dir)
    {
        const FVector4 V(Dir.Body, Dir.Mind, Dir.Spirit, Dir.Nature);
        const float LenSq = V.X * V.X + V.Y * V.Y + V.Z * V.Z + V.W * V.W;
        if (LenSq > KINDA_SMALL_NUMBER)
        {
            return V * FMath::InvSqrt(LenSq);
        }
        return FVector4(0.5f, 0.5f, 0.5f, 0.5f);
    }

    static FDirection UnitVectorToDirection(const FVector4& V)
    {
        FDirection Dir;
        Dir.Body = V.X;
        Dir.Mind = V.Y;
        Dir.Spirit = V.Z;
        Dir.Nature = V.W;
        Dir.NormalizeSum();
        return Dir;
    }

    // Портировано из легаси PipelineMorok.cpp::ApplyMorokDistortion — настоящее
    // матричное смешивание осей ("обмен осями"), а не случайная перестановка.
    static void ApplyMorokAxisMix(FVector4& Dir, float Distortion, FRandomStream& Rng)
    {
        const float Mix = Distortion * 0.7f;
        const float Rnd = Rng.FRandRange(-1.f, 1.f);
        const float K = Rnd * Distortion * 0.5f;

        const float B = Dir.X, M = Dir.Y, S = Dir.Z, N = Dir.W;

        Dir.X = (1.f - Mix) * B + K * M + Mix * S;
        Dir.Y = -K * B + (1.f - Mix) * M + Mix * N;
        Dir.Z = Mix * B + (1.f - Mix) * S + K * N;
        Dir.W = Mix * M - K * S + (1.f - Mix) * N;

        const float LengthScale = 1.f + Distortion * 0.5f;
        Dir *= LengthScale;

        const float MaxLenSq = 4.f;
        const float LenSq = Dir.X * Dir.X + Dir.Y * Dir.Y + Dir.Z * Dir.Z + Dir.W * Dir.W;
        if (LenSq > MaxLenSq)
        {
            Dir *= FMath::Sqrt(MaxLenSq / LenSq);
        }
    }

    // Портировано из легаси PipelineZaryana.cpp::ApplyZaryanaStructuring — усиление
    // осей выше среднего, подавление ниже среднего, мягкая tanh-нелинейность.
    static void ApplyZaryanaAxisMix(FVector4& Dir, float ZaryanaStrength, const UHerbalistSettings* Settings)
    {
        const float BoostFactor = Settings ? Settings->ZaryanaBoostFactor : 0.5f;
        const float SuppressFactor = Settings ? Settings->ZaryanaSuppressFactor : 0.3f;
        const float Avg = (Dir.X + Dir.Y + Dir.Z + Dir.W) / 4.f;

        auto Process = [&](double& V)
        {
            if (V > Avg) V *= (1.0 + ZaryanaStrength * BoostFactor);
            else V *= (1.0 - ZaryanaStrength * SuppressFactor);
        };
        Process(Dir.X); Process(Dir.Y); Process(Dir.Z); Process(Dir.W);

        const float Scale = 1.f + ZaryanaStrength * 0.8f;
        Dir.X = FMath::Tanh(Dir.X * Scale);
        Dir.Y = FMath::Tanh(Dir.Y * Scale);
        Dir.Z = FMath::Tanh(Dir.Z * Scale);
        Dir.W = FMath::Tanh(Dir.W * Scale);

        const float LenSq = Dir.X * Dir.X + Dir.Y * Dir.Y + Dir.Z * Dir.Z + Dir.W * Dir.W;
        if (LenSq > KINDA_SMALL_NUMBER)
        {
            Dir *= FMath::InvSqrt(LenSq);
        }
        else
        {
            Dir = FVector4(0.5f, 0.5f, 0.5f, 0.5f);
        }
    }

    // ---------------------------------------------------------
    // Intent/Coherence — портировано из легаси IntentResolver.cpp::ComputeIntentCoherence
    // (последний живой коммит 1539015, до перехода на PipelineV2). Раньше Coherence
    // был захардкожен как 0.5f во всех вызывающих местах — Pipeline теперь считает
    // его сам из фактических ингредиентов (вес по позиции, согласие доминирующих
    // осей, качество ингредиентов, бонус воды), вызывающий код это значение
    // больше не задаёт (см. ProcessApplyCommand).
    // ---------------------------------------------------------
    static float ComputeIntentCoherence(const TArray<FInventoryItem>& Ingredients)
    {
        const UHerbalistSettings* Settings = GetHerbalistSettings();
        const float WeightDecay = Settings ? Settings->FoldWeightDecay : 0.8f;

        TArray<const FInventoryItem*> NonWater;
        TArray<const FInventoryItem*> Water;
        for (const FInventoryItem& Item : Ingredients)
        {
            (Item.bIsWater ? Water : NonWater).Add(&Item);
        }

        const int32 N = NonWater.Num();
        if (N == 0) return 0.5f; // нет ингредиентов — нейтральное намерение

        float AxisWeights[4] = { 0.f, 0.f, 0.f, 0.f };
        float TotalWeight = 0.f;
        float WeightedPurity = 0.f;
        float WeightedStability = 0.f;
        float Weight = 1.f;

        for (const FInventoryItem* Item : NonWater)
        {
            const FDirection& Dir = Item->State.Direction;
            const float Vals[4] = { Dir.Body, Dir.Mind, Dir.Spirit, Dir.Nature };
            int32 Dominant = 0;
            for (int32 i = 1; i < 4; ++i)
            {
                if (Vals[i] > Vals[Dominant]) Dominant = i;
            }
            AxisWeights[Dominant] += Weight;

            WeightedPurity += Item->State.Meta.Purity * Weight;
            WeightedStability += Item->State.Meta.Stability * Weight;

            TotalWeight += Weight;
            Weight *= WeightDecay;
        }

        float MaxAxisWeight = 0.f;
        for (float W : AxisWeights) MaxAxisWeight = FMath::Max(MaxAxisWeight, W);
        const float AxisAgreement = (TotalWeight > KINDA_SMALL_NUMBER) ? (MaxAxisWeight / TotalWeight) : 0.f;

        const float IngredientQuality = (TotalWeight > KINDA_SMALL_NUMBER)
            ? (WeightedPurity / TotalWeight + WeightedStability / TotalWeight) * 0.5f
            : 0.f;

        float WaterBonus = 0.f;
        if (Water.Num() > 0)
        {
            float AvgWaterPurity = 0.f;
            for (const FInventoryItem* W : Water) AvgWaterPurity += W->State.Meta.Purity;
            AvgWaterPurity /= Water.Num();
            WaterBonus = AvgWaterPurity * 0.2f;
        }

        const float Coherence = FMath::Lerp(AxisAgreement, IngredientQuality, 0.5f) + WaterBonus;
        return FMath::Clamp(Coherence, 0.f, 1.f);
    }

    // ---------------------------------------------------------
    // Пайплайн варки зелья — по мотивам 9 шагов из 05_Systems.md. Фактический
    // порядок выполнения ниже (не совпадает с нумерацией GDD дословно, но
    // семантически эквивалентен — вода трогает только Magnitude/Purity, биом
    // только Direction, порядок между ними не влияет на результат):
    // 1-2. Сбор параметров + Агрегация (Fold, с затуханием по порядку)
    // 4. Применение воды (обязательность, "только вода", разбавление, штраф >80%)
    // 3. Biome Context Injection (MorokField/ZaryanaField/Affinity/AxisDrift)
    // 5. Нормализация осей
    // 6. Morok (нелинейное искажение + обмен осями)
    // 7. Zaryana (усиление доминирующей оси, стабильность/чистота)
    // 8. Bifurcation (Collapse/Purification)
    // ---------------------------------------------------------
    static FRealState ComputeApplyResult(const TArray<FInventoryItem>& Ingredients,
                                        const FIntent& Intent,
                                        const FBiomeFieldContext* BiomeCtx,
                                        float CollapseThreshold,
                                        FRandomStream& Rng,
                                        EAlchemyOutcome& OutOutcome,
                                        FVector4& OutAxisDeltaForFootprint,
                                        bool bIsRitual = false,
                                        bool bBifurcationCharmActive = false,
                                        EMoonPhase MoonPhase = EMoonPhase::NewMoon)
    {
        OutOutcome = EAlchemyOutcome::Valid;
        OutAxisDeltaForFootprint = FVector4(0.f, 0.f, 0.f, 0.f);

        if (Ingredients.Num() == 0) return FRealState();

        // --- 0. Уже готовый результат варки, не сырые ингредиенты --
        // применение зелья на клетку (UsePotion -> AGridWorldManager::
        // ApplyPotionToCell) заворачивает готовый предмет в тот же список
        // Ingredients, что и сырые материалы при варке. Без этой проверки
        // единственный небольшой предмет без воды в списке безусловно
        // попадал бы под правило "обязательность воды" ниже (шаг 4a) и
        // становился золой ЗАНОВО, стирая реально сваренное качество --
        // находка сессии 2026-08-30 (PlaySessionIntegrationTest.cpp поймал
        // сваренное Purity=0.69 зелье, ставшее золой при применении).
        // Признак "это уже готовый результат, не сырьё" -- IngredientID
        // совпадает с одним из трёх исходов этой же функции; собранные в
        // мире ингредиенты этих ID не носят (зарезервированные имена
        // выходов пайплайна, не строки из реестра ингредиентов).
        if (Ingredients.Num() == 1)
        {
            const FName ID = Ingredients[0].IngredientID;
            if (ID == FName(TEXT("Potion")) || ID == FName(TEXT("Ash")) || ID == FName(TEXT("BoiledWater")))
            {
                return Ingredients[0].State;
            }
        }

        const UHerbalistSettings* Settings = GetHerbalistSettings();

        // --- 1-2. Разделяем на воду и не-воду. Вода по-прежнему агрегируется
        // простым взвешенным средним (не её сегодня докручиваем — только
        // сочетание трав, см. комментарий у ReactAxis выше) ---
        const float OrderDecay = Settings ? Settings->FoldWeightDecay : 0.8f;
        const float AgreementRate  = Settings ? Settings->AlchemyAgreementRate  : 0.6f;
        const float ConflictRate   = Settings ? Settings->AlchemyConflictRate   : 0.5f;
        const float PowerGrowthRate = Settings ? Settings->AlchemyPowerGrowthRate : 0.5f;
        const float PowerDecayRate  = Settings ? Settings->AlchemyPowerDecayRate  : 0.6f;

        TArray<const FInventoryItem*> NonWaterItems;
        int32 NonWaterCount = 0;

        FRealState WaterAgg;
        float WaterWeightSum = 0.f;
        int32 WaterCount = 0;

        for (const FInventoryItem& Item : Ingredients)
        {
            if (Item.bIsWater)
            {
                const float Weight = static_cast<float>(Item.Count);
                AccumulateWeighted(WaterAgg, Item.State, Weight);
                WaterWeightSum += Weight;
                WaterCount += Item.Count;
            }
            else
            {
                NonWaterItems.Add(&Item);
                NonWaterCount += Item.Count;
            }
        }

        DivideRealState(WaterAgg, WaterWeightSum);

        // --- Сборка не-водных ингредиентов -- ПОСЛЕДОВАТЕЛЬНО, не одним
        // симметричным средним (по прямому запросу "докручиваем варку,
        // учитывая реальные сильные стороны трав и их сочетаний"; находка
        // сессии — честный набор трав без выраженного согласия давал
        // "Хилую воду", технически верно, но невыразительно). Первый
        // ингредиент — затравка (сырой базовый характер), каждый следующий
        // РЕАГИРУЕТ на уже накопленный результат: согласные оси усиливают
        // друг друга, конфликтующие гасят к нейтральной середине. Мощь
        // (Magnitude) — прямое следствие того, насколько ингредиенты
        // "спелись" (ComputeHarmony), не просто их число: слаженное
        // сочетание сильнее любого компонента по отдельности, разнородное —
        // слабее, даже если веществ больше. Порядок теперь и правда важен:
        // добавление 3-го согласного/конфликтующего ингредиента — не то же
        // самое, что взять те же три сразу одним средним.
        FRealState NonWaterAgg;
        if (NonWaterItems.Num() > 0)
        {
            NonWaterAgg = NonWaterItems[0]->State;
            NonWaterAgg.Direction.NormalizeSum();
            for (int32 i = 1; i < NonWaterItems.Num(); ++i)
            {
                const FRealState& Incoming = NonWaterItems[i]->State;
                const float Weight = FMath::Pow(OrderDecay, static_cast<float>(i));
                const float Harmony = ComputeHarmony(NonWaterAgg, Incoming);

                ReactAxis(NonWaterAgg.Meta.Distortion, Incoming.Meta.Distortion, Weight, AgreementRate, ConflictRate);
                ReactAxis(NonWaterAgg.Meta.Stability,  Incoming.Meta.Stability,  Weight, AgreementRate, ConflictRate);
                ReactAxis(NonWaterAgg.Meta.Purity,     Incoming.Meta.Purity,     Weight, AgreementRate, ConflictRate);
                ReactAxis(NonWaterAgg.Meta.Potency,    Incoming.Meta.Potency,    Weight, AgreementRate, ConflictRate);
                ReactAxis(NonWaterAgg.Meta.Resonance,  Incoming.Meta.Resonance,  Weight, AgreementRate, ConflictRate);
                ReactAxis(NonWaterAgg.Meta.Corruption, Incoming.Meta.Corruption, Weight, AgreementRate, ConflictRate);
                ReactDirection(NonWaterAgg.Direction, Incoming.Direction, Weight, AgreementRate, ConflictRate);

                NonWaterAgg.Magnitude = Harmony >= 0.f
                    ? FMath::Lerp(NonWaterAgg.Magnitude, 1.0f, PowerGrowthRate * Harmony * Weight)
                    : FMath::Lerp(NonWaterAgg.Magnitude, 0.0f, PowerDecayRate * (-Harmony) * Weight);
            }
        }

        // --- 4a. Обязательность воды: ингредиенты без единой капли воды дают золу ---
        if (NonWaterCount > 0 && WaterCount == 0)
        {
            OutOutcome = EAlchemyOutcome::Ash;
            FRealState Ash;
            Ash.Magnitude = 0.05f;
            Ash.Meta.Distortion = 0.9f;
            Ash.Meta.Corruption = 0.8f;
            Ash.Meta.Purity = 0.05f;
            Ash.Meta.Stability = 0.1f;
            Ash.Direction = NonWaterAgg.Direction;
            Ash.Direction.NormalizeSum();
            return Ash;
        }

        // --- 4b. Только вода — варёная вода: высокая Purity, нулевой Distortion ---
        if (NonWaterCount == 0 && WaterCount > 0)
        {
            OutOutcome = EAlchemyOutcome::BoiledWater;
            FRealState Boiled = WaterAgg;
            Boiled.Meta.Purity = FMath::Clamp(Boiled.Meta.Purity + 0.3f, 0.f, 1.f);
            Boiled.Meta.Distortion = 0.f;
            Boiled.Magnitude = FMath::Min(Boiled.Magnitude, 0.2f);
            Boiled.Direction.NormalizeSum();
            return Boiled;
        }

        // --- Смесь: Fold ингредиентов + разбавление водой ---
        FRealState Result = NonWaterAgg;

        const int32 TotalCount = NonWaterCount + WaterCount;
        const float WaterFraction = TotalCount > 0 ? static_cast<float>(WaterCount) / static_cast<float>(TotalCount) : 0.f;

        // 4c. Разбавление: Magnitude снижается пропорционально доле воды
        Result.Magnitude = FMath::Clamp(Result.Magnitude * (1.f - WaterFraction), 0.f, 1.f);

        // 4d. Избыток воды: дополнительный штраф — "водянистое" зелье
        const float MaxWaterRatio = Settings ? Settings->MaxWaterRatio : 0.8f;
        const float WaterDilutionPenalty = Settings ? Settings->WaterDilutionPenalty : 0.2f;
        if (WaterFraction > MaxWaterRatio)
        {
            const float Excess = (WaterFraction - MaxWaterRatio) / FMath::Max(1.f - MaxWaterRatio, KINDA_SMALL_NUMBER);
            Result.Magnitude = FMath::Clamp(Result.Magnitude * (1.f - Excess * WaterDilutionPenalty), 0.f, 1.f);
        }

        // Вода как растворитель слегка подтягивает Purity к своей
        Result.Meta.Purity = FMath::Clamp(FMath::Lerp(Result.Meta.Purity, WaterAgg.Meta.Purity, WaterFraction * 0.5f), 0.f, 1.f);

        // --- 3. Biome Context Injection: сдвиг осей от Memory.AxisDrift,
        // эффективная сила Morok/Zaryana = поле узла * аффинити биома ---
        float EffectiveMorok = 0.f;
        float EffectiveZaryana = 0.f;
        if (BiomeCtx)
        {
            EffectiveMorok = FMath::Clamp(BiomeCtx->MorokField * BiomeCtx->MorokAffinity, 0.f, 1.f);
            EffectiveZaryana = FMath::Clamp(BiomeCtx->ZaryanaField * BiomeCtx->ZaryanaAffinity, 0.f, 1.f);

            const float DriftStrength = Settings ? Settings->BiomeAxisDriftWeight : 0.1f;
            Result.Direction.Body   = FMath::Max(0.f, Result.Direction.Body   + (BiomeCtx->AxisDrift.X - 0.25f) * DriftStrength);
            Result.Direction.Mind   = FMath::Max(0.f, Result.Direction.Mind   + (BiomeCtx->AxisDrift.Y - 0.25f) * DriftStrength);
            Result.Direction.Spirit = FMath::Max(0.f, Result.Direction.Spirit + (BiomeCtx->AxisDrift.Z - 0.25f) * DriftStrength);
            Result.Direction.Nature = FMath::Max(0.f, Result.Direction.Nature + (BiomeCtx->AxisDrift.W - 0.25f) * DriftStrength);
        }

        // --- 5. Нормализация осей ---
        Result.Direction.NormalizeSum();

        // --- 6-7. Morok/Zaryana — портировано из легаси PipelineMorok.cpp /
        // PipelineZaryana.cpp / HerbalistPipeline.cpp::ApplyMorok (коммит 1539015).
        // ZaryanaStrength тем выше, чем согласованнее Intent (Coherence) и чем
        // ниже давление Морока в биоме — легаси-формула Coherence*(1-Distortion),
        // но Coherence теперь настоящий (ComputeIntentCoherence), а не константа. ---
        const float Coherence = FMath::Clamp(Intent.Coherence, 0.f, 1.f);
        const float MorokPressure = Settings ? Settings->MorokPressure : 1.0f;
        const float BiomeZaryanaInfluence = Settings ? Settings->BiomeZaryanaInfluence : 0.3f;
        const float ZaryanaStrength = FMath::Clamp(Coherence * (1.f - EffectiveMorok), 0.f, 1.f);

        // Morok и Zaryana — одна операция в две стороны: возведение параметра
        // из [0,1] в степень. Морок понижает показатель (тянет к 1), Заряна
        // повышает (тянет к 0). Свойства, ради которых выбрана эта форма:
        //
        //   * ограничена [0,1] ПО ПОСТРОЕНИЮ (x^p при x,p>0 не покидает отрезок) —
        //     клампы здесь больше не нужны, как и в GenerateHarvestResult;
        //   * x=0 и x=1 — неподвижные точки: абсолютно чистое неуязвимо для
        //     Морока, абсолютно искажённое не вытянуть Заряной;
        //   * Морок УСИЛИВАЕТ уже имеющееся, а не впрыскивает своё —
        //     03_Narrative: "Морок не действует как отдельная сущность.
        //     Он проявляется через поведение системы, изменяя её структуру".
        //
        // Прежняя форма (D += Noise*(1-D)) была насыщающей: чем грязнее смесь,
        // тем МЕНЬШЕ мог добавить Морок. На замерах злого состава (D=0.82)
        // весь диапазон влияния мира составлял 0.072 — исход определялся
        // ингредиентами примерно вчетверо сильнее, чем местом, вопреки
        // Core Lock §2, а Bifurcation не срабатывал ни разу за 200 варок даже
        // при MorokAffinity=1.0. Насыщение убрано: показатель степени зависит
        // от давления Морока и гасится Stability — 03_Narrative,
        // "нестабильность усиливается в нарушенной среде, она ослабевает
        // в согласованных условиях".
        // Полнолуние, §15.3: "вместе с силой растёт и Morok... ставки выше в
        // обе стороны" -- та же надбавка, что уже усиливает сбор при полной
        // луне (MoonFullBoostStrength, GenerateHarvestResult), здесь толкает
        // экспоненту Морока, а не оси напрямую: чем она выше, тем сильнее
        // ApplyMorokPush тянет Distortion/Corruption к 1, а значит и вероятнее
        // пробить EffectiveCollapseThreshold ниже (шаг 8) -- ставки растут в
        // обе стороны эффекта, не только в сторону "хуже", т.к. тот же бросок
        // может и очиститься (bRolledPurify), не только сорваться.
        const float MoonFullMorokBoostStrength = Settings ? Settings->MoonFullMorokBoostStrength : 0.15f;
        const float MoonFullMorokBoost = (MoonPhase == EMoonPhase::FullMoon) ? MoonFullMorokBoostStrength : 0.0f;
        const float MorokExponent = EffectiveMorok * MorokPressure * (1.f - Result.Meta.Stability) * (1.f + MoonFullMorokBoost);
        auto ApplyMorokPush = [MorokExponent](float Value)
        {
            return (Value > KINDA_SMALL_NUMBER) ? FMath::Pow(Value, 1.f / (1.f + MorokExponent)) : Value;
        };
        Result.Meta.Distortion = ApplyMorokPush(Result.Meta.Distortion);
        Result.Meta.Corruption = ApplyMorokPush(Result.Meta.Corruption);

        FVector4 UnitDir = DirectionToUnitVector(Result.Direction);
        ApplyMorokAxisMix(UnitDir, EffectiveMorok, Rng);

        // Zaryana: обратная операция — тот же показатель, но в другую сторону.
        Result.Meta.Distortion = FMath::Pow(Result.Meta.Distortion, 1.f + ZaryanaStrength * 0.25f);
        Result.Meta.Corruption = FMath::Pow(Result.Meta.Corruption, 1.f + ZaryanaStrength * 0.20f);
        Result.Meta.Stability = FMath::Lerp(Result.Meta.Stability, 1.f, ZaryanaStrength * 0.15f);
        Result.Meta.Purity = FMath::Clamp(Result.Meta.Purity + ZaryanaStrength * Result.Meta.Stability * 0.2f, 0.f, 1.f);

        // Поле биома (EffectiveZaryana) — отдельная надбавка сверх Coherence-driven
        // эффекта (Biome Context Injection, 14_Biome_Graph.md)
        Result.Meta.Stability = FMath::Clamp(Result.Meta.Stability + EffectiveZaryana * BiomeZaryanaInfluence * 0.3f, 0.f, 1.f);
        Result.Meta.Purity = FMath::Clamp(Result.Meta.Purity + EffectiveZaryana * BiomeZaryanaInfluence * 0.5f, 0.f, 1.f);

        ApplyZaryanaAxisMix(UnitDir, ZaryanaStrength, Settings);
        Result.Direction = UnitVectorToDirection(UnitDir);

        // --- 8. Bifurcation: при критическом Distortion — Collapse или Purification.
        // Чем выше текущая Stability, тем вероятнее очищение, а не схлопывание.
        //
        // Градации сложности/опасности (2026-08-30, "2 просто, 3 риск, 4
        // опасно, 5 смертельно" -- прямой запрос): опасность растёт с числом
        // РАЗНЫХ не-водных ингредиентов, не их суммарным количеством в стопке
        // -- котёл не прощает сложность рецепта, не объём. На каждый
        // ингредиент сверх двух порог срыва снижается (AlchemyRiskThresholdStep)
        // и шанс "повезло" при уже случившемся срыве падает
        // (AlchemyRiskPurifyOddsStep) -- крепкая Stability всё ещё может
        // спасти на 3-4, но всё менее надёжно. На AlchemyGuaranteedCatastropheCount
        // (по умолчанию 5) и выше -- катастрофа безусловна, без исключения
        // для удачно подобранных ингредиентов: "смертельно" в буквальном
        // смысле — не "скорее всего", а всегда.
        //
        // Правильно исполненный ритуал (bIsRitual, AGridWorldManager::
        // TryAdvanceRitual) обходит все градации риска целиком -- игрок
        // сварил по верному месту/времени/порядку, не закинул всё разом.
        // Котёл наказывает за проигнорированную сложность, не за укрощённую. ---
        const int32 GuaranteedCatastropheCount = Settings ? Settings->AlchemyGuaranteedCatastropheCount : 5;
        if (!bIsRitual && NonWaterItems.Num() >= GuaranteedCatastropheCount)
        {
            Result.Meta.Distortion = 0.2f;
            Result.Meta.Stability = FMath::Clamp(Result.Meta.Stability - 0.3f, 0.f, 1.f);
            Result.Meta.Corruption = FMath::Clamp(Result.Meta.Corruption + 0.2f, 0.f, 1.f);
            OutOutcome = EAlchemyOutcome::Catastrophe;
        }
        else
        {
            const int32 RiskyCount = bIsRitual ? 0 : FMath::Max(0, NonWaterItems.Num() - 2);
            const float RiskThresholdStep = Settings ? Settings->AlchemyRiskThresholdStep : 0.15f;
            const float RiskPurifyOddsStep = Settings ? Settings->AlchemyRiskPurifyOddsStep : 0.3f;
            const float EffectiveCollapseThreshold = FMath::Max(0.1f, CollapseThreshold - RiskThresholdStep * RiskyCount);
            const float PurifyOddsMultiplier = FMath::Clamp(1.f - RiskPurifyOddsStep * RiskyCount, 0.f, 1.f);

            if (Result.Meta.Distortion >= EffectiveCollapseThreshold)
            {
                // Всегда тянем бросок, даже с активным заговором — держание
                // Камня-оберега не должно менять потребление Rng для
                // последующих бросков того же тика (детерминизм/трассировка).
                const bool bRolledPurify = Rng.FRand() < Result.Meta.Stability * PurifyOddsMultiplier;
                // Камень-оберег (21_Journey_And_Artifacts.md §21.3,
                // 2026-09-01) — "гасит худший исход один раз, не гарантирует
                // успех": не трогает GuaranteedCatastropheCount-ветку выше
                // (та безусловна по дизайну), только этот вероятностный
                // бросок — превращает неудачный в Purified.
                const bool bPurify = bRolledPurify || bBifurcationCharmActive;
                if (bPurify)
                {
                    Result.Meta.Distortion = 0.4f;
                    Result.Meta.Purity = FMath::Clamp(Result.Meta.Purity + 0.2f, 0.f, 1.f);
                    Result.Meta.Stability = FMath::Clamp(Result.Meta.Stability + 0.2f, 0.f, 1.f);
                    OutOutcome = EAlchemyOutcome::Purified;
                }
                else
                {
                    Result.Meta.Distortion = 0.2f;
                    Result.Meta.Stability = FMath::Clamp(Result.Meta.Stability - 0.3f, 0.f, 1.f);
                    Result.Meta.Corruption = FMath::Clamp(Result.Meta.Corruption + 0.2f, 0.f, 1.f);
                    OutOutcome = EAlchemyOutcome::Catastrophe;
                }
            }
        }

        OutAxisDeltaForFootprint = FVector4(Result.Direction.Body, Result.Direction.Mind, Result.Direction.Spirit, Result.Direction.Nature)
            - FVector4(0.25f, 0.25f, 0.25f, 0.25f);

        return Result;
    }

    // ---------------------------------------------------------
    // Обработчики команд
    // ---------------------------------------------------------

    static void ProcessHarvestCommand(const FHarvestCommand& Cmd,
                                     const FWorldSnapshot& WorldSnap,
                                     FRandomStream& Rng,
                                     FStateDelta& OutDelta)
    {
        const FGridCell* Cell = WorldSnap.GridState.Find(Cmd.TargetCell);
        if (!Cell)
        {
            UE_LOG(LogHerbalistSimulation, Warning, TEXT("PipelineV2: Harvest cell (%d,%d) not found"), Cmd.TargetCell.X, Cmd.TargetCell.Y);
            return;
        }
        
        // ====================================================================
        // ВОДА: не деградирует – только добавляем предмет в инвентарь
        // ====================================================================
        if (Cell->bIsWater)
        {
            FInventoryItem WaterItem;
            WaterItem.IngredientID = Cell->WaterTypeID.IsNone() ? FName(TEXT("Water")) : Cell->WaterTypeID;
            WaterItem.State = Cell->State;                 // копируем состояние воды
            WaterItem.Count = 1;
            WaterItem.CreationTime = WorldSnap.WorldTime;
            WaterItem.bSubjectToDecay = true;
            WaterItem.bIsWater = true;
            // Вода тоже честно помнит клетку сбора (не участвует в подсчёте
            // межбиомности -- вызывающая сторона, ApplyAlchemyResult, игнорирует
            // bIsWater-предметы, но поле не должно молчать о том, что известно).
            WaterItem.SourceBiome = Cell->Biome;

            FInventoryOperation Op;
            Op.ContainerID = 0;
            Op.Ingredient = WaterItem;
            Op.OpType = EInventoryOpType::Add;
            Op.Amount = 1;
            OutDelta.InventoryOps.Add(Op);

            // Не добавляем WorldChanges – клетка не меняется
            UE_LOG(LogHerbalistSimulation, Verbose, TEXT("Water harvested at (%d,%d)"), Cmd.TargetCell.X, Cmd.TargetCell.Y);
            return;
        }

        // ====================================================================
        // РАСТЕНИЯ: деградация (медленная)
        // ====================================================================
        const float DegradationStep = 0.002f;
        // Прирост стресса из настроек: раньше здесь стояли захардкоженные 0.001,
        // ровно равные тогдашнему спаду за секунду — след одного сбора стирался
        // за секунду, и механика «сбор истощает место» не работала вовсе.
        const UHerbalistSettings* HarvestSettings = GetHerbalistSettings();
        const float StressStep = HarvestSettings ? HarvestSettings->HarvestStressIncrement : 0.1f;

        FGridCell Modified = *Cell;
        Modified.HarvestStress = FMath::Clamp(Cell->HarvestStress + StressStep, 0.0f, 1.0f);
        Modified.State.Meta.Distortion = FMath::Clamp(Cell->State.Meta.Distortion + DegradationStep, 0.0f, 1.0f);
        Modified.State.Meta.Purity      = FMath::Clamp(Cell->State.Meta.Purity      - DegradationStep, 0.0f, 1.0f);
        Modified.State.Meta.Stability   = FMath::Clamp(Cell->State.Meta.Stability   - DegradationStep, 0.0f, 1.0f);
        Modified.State.Magnitude        = FMath::Clamp(Cell->State.Magnitude        - DegradationStep * 0.5f, 0.0f, 1.0f);
        
        Modified.State.Direction.Nature = FMath::Clamp(Cell->State.Direction.Nature - 0.001f, 0.0f, 1.0f);
        Modified.State.Direction.Body   = FMath::Clamp(Cell->State.Direction.Body   + 0.001f, 0.0f, 1.0f);
        Modified.State.Direction.NormalizeSum();
        Modified.Memory.AccumulatedDistortion = Modified.State.Meta.Distortion;
        
        FInventoryItem Harvested = GenerateHarvestResult(*Cell, Cmd.IngredientID, Cmd.BaseState, Cmd.Resilience, Cmd.MoonPhase, Cmd.Tool, Cmd.bIronAverse, Cmd.bDelicate, Rng);
        Harvested.CreationTime = WorldSnap.WorldTime;

        // Посадочный материал (SetHarvestIntent "seed", DESIGN_Community_And_
        // Homestead.md §2.4, 2026-09-04) -- тот же дикий куст, другое
        // назначение собранного предмета, не отдельный расчёт State: клетка
        // деградирует/накапливает HarvestStress ровно как при обычном сборе
        // выше (сбор есть сбор независимо от цели), различие только в том,
        // ЧТО в итоге кладётся в инвентарь.
        Harvested.bIsPlantingStock = Cmd.bForPlanting;

        FInventoryOperation Op;
        Op.ContainerID = 0;
        Op.Ingredient = Harvested;
        Op.OpType = EInventoryOpType::Add;
        Op.Amount = Cmd.Amount;
        OutDelta.InventoryOps.Add(Op);
        OutDelta.WorldChanges.Add(Cmd.TargetCell, Modified);
        
        UE_LOG(LogHerbalistSimulation, Verbose, TEXT("Harvest: cell (%d,%d) Dist=%.3f"),
            Cmd.TargetCell.X, Cmd.TargetCell.Y, Modified.State.Meta.Distortion);
    }

    static void ProcessTransferCommand(const FTransferCommand& Cmd,
                                      const FInventorySnapshot& InvSnap,
                                      FStateDelta& OutDelta)
    {
        const FInventoryItem* SourceItem = FindItemInSnapshot(InvSnap, Cmd.SourceContainerID, Cmd.IngredientID);
        if (!SourceItem)
        {
            UE_LOG(LogHerbalistSimulation, Warning, TEXT("PipelineV2: Transfer source item %s not found in container %d"),
                *Cmd.IngredientID.ToString(), Cmd.SourceContainerID);
            return;
        }

        FInventoryOperation RemoveOp;
        RemoveOp.ContainerID = Cmd.SourceContainerID;
        RemoveOp.Ingredient = *SourceItem;
        RemoveOp.OpType = EInventoryOpType::Remove;
        RemoveOp.Amount = Cmd.Amount;
        OutDelta.InventoryOps.Add(RemoveOp);

        FInventoryOperation AddOp;
        AddOp.ContainerID = Cmd.TargetContainerID;
        AddOp.Ingredient = *SourceItem;
        AddOp.Ingredient.Count = Cmd.Amount;
        AddOp.OpType = EInventoryOpType::Add;
        AddOp.Amount = Cmd.Amount;
        OutDelta.InventoryOps.Add(AddOp);
    }

    static void ProcessApplyCommand(const FApplyCommand& Cmd,
                                   const FWorldSnapshot& WorldSnap,
                                   const FBiomeSnapshot& BiomeSnap,
                                   FRandomStream& Rng,
                                   FStateDelta& OutDelta)
    {
        // 0. Контекст биома клетки-цели. При крафте (вне мира, TargetCell = (-1,-1))
        // контекста нет — Biome Context Injection применяется только при варке
        // непосредственно в мире, как описано в 05_Systems.md/14_Biome_Graph.md.
        const FGridCell* TargetCell = Cmd.bIsCrafting ? nullptr : WorldSnap.GridState.Find(Cmd.TargetCell);
        const FBiomeFieldContext* BiomeCtx = nullptr;
        FName TargetBiomeID;
        if (TargetCell)
        {
            TargetBiomeID = FBiomeDefaults::BiomeTypeToName(TargetCell->Biome);
            BiomeCtx = BiomeSnap.Contexts.Find(TargetBiomeID);
        }

        // 1. Вычисляем результирующее состояние зелья. Coherence считается
        // Pipeline'ом из фактических ингредиентов (см. ComputeIntentCoherence) —
        // то, что вызывающий код положил в Cmd.Intent.Coherence, не используется:
        // Intent_system не может быть решением игрока/UI, только функцией процесса.
        FIntent EffectiveIntent = Cmd.Intent;
        EffectiveIntent.Coherence = ComputeIntentCoherence(Cmd.Ingredients);

        // Капище, эффект 2 (02_GDD/15_Cycles_And_Shrines.md §15.5, 11_Intent_Evolution
        // §11.7) — надбавка к Coherence в радиусе влияния. Читается независимо от
        // TargetCell/BiomeCtx выше: тот пуст при крафте намеренно (Biome Context
        // Injection — только "варка в мире"), а капище должно видеть клетку котла
        // даже при крафте, это и есть единственный способ подношению вообще сработать.
        const UHerbalistSettings* ShrineSettings = GetHerbalistSettings();
        const float ShrineInfluence = HerbalistCore::Shrine::GetInfluenceAt(
            Cmd.TargetCell, WorldSnap.Shrines, ShrineSettings ? ShrineSettings->ShrineInfluenceRadius : 3);
        if (ShrineInfluence > 0.0f)
        {
            const float Bonus = ShrineSettings ? ShrineSettings->ShrineCoherenceBonus : 0.15f;
            EffectiveIntent.Coherence = FMath::Clamp(EffectiveIntent.Coherence + ShrineInfluence * Bonus, 0.0f, 1.0f);
        }

        // Оберег BrewBoost (Громовая стрела, DESIGN_Community_And_Homestead.md
        // §2.4, 2026-09-04) -- та же надбавка к Coherence, что и у капища
        // выше, но БЕЗ радиуса влияния (личный эффект ношения, не место) и
        // заметно слабее -- плоский WardBrewBoostCoherenceBonus вместо
        // ShrineInfluence-масштабированного ShrineCoherenceBonus. Массовый/
        // крафтящийся аналог Камня-оберега (не трогает Bifurcation -- та
        // ветка остаётся исключительно за Cmd.bBifurcationCharmActive).
        if (Cmd.bWardBrewBoostActive)
        {
            const float WardBonus = ShrineSettings ? ShrineSettings->WardBrewBoostCoherenceBonus : 0.05f;
            EffectiveIntent.Coherence = FMath::Clamp(EffectiveIntent.Coherence + WardBonus, 0.0f, 1.0f);
        }

        // Межбиомная варка (DESIGN_Community_And_Homestead.md §2.4, 2026-09-04)
        // -- та же плоская надбавка к Coherence, что и у оберега BrewBoost
        // выше, но её сила зависит от того, СКОЛЬКО разных биомов собрано в
        // котле (Cmd.DistinctIngredientBiomeCount, готовое число от
        // ApplyAlchemyResult -- см. CommandTypes.h). Две ступени, не плавная
        // формула: 2 разных биома -- обычный бонус, все 3 (физический предел
        // котла, AlchemyTransferWidget.cpp) -- удвоенный, тем же простым
        // приёмом "полный набор сильнее частичного", что уже bBothHigh/
        // bBothLow-ветки ReactAxis выше в этом файле.
        if (Cmd.DistinctIngredientBiomeCount >= 2)
        {
            const float BaseBonus = ShrineSettings ? ShrineSettings->CrossBiomeCoherenceBonus : 0.03f;
            const float CrossBiomeBonus = (Cmd.DistinctIngredientBiomeCount >= 3) ? BaseBonus * 2.0f : BaseBonus;
            EffectiveIntent.Coherence = FMath::Clamp(EffectiveIntent.Coherence + CrossBiomeBonus, 0.0f, 1.0f);
        }

        EAlchemyOutcome Outcome = EAlchemyOutcome::Valid;
        FVector4 AxisDeltaForFootprint;
        FRealState PotionState = ComputeApplyResult(Cmd.Ingredients, EffectiveIntent, BiomeCtx, BiomeSnap.CollapseThreshold, Rng, Outcome, AxisDeltaForFootprint, Cmd.bIsRitual, Cmd.bBifurcationCharmActive, Cmd.MoonPhase);

        // 2. Удаляем использованные ингредиенты из инвентаря
        for (const FInventoryItem& Ing : Cmd.Ingredients)
        {
            FInventoryOperation RemoveOp;
            RemoveOp.ContainerID = 0;
            RemoveOp.Ingredient = Ing;
            RemoveOp.Ingredient.Count = 1;
            RemoveOp.OpType = EInventoryOpType::Remove;
            RemoveOp.Amount = 1;
            OutDelta.InventoryOps.Add(RemoveOp);
        }

        // Footprint (14_Biome_Graph.md) — только при варке/применении непосредственно
        // в мире, привязанной к конкретному биому.
        if (TargetCell && !TargetBiomeID.IsNone())
        {
            FBiomeFootprintEntry Footprint;
            Footprint.BiomeID = TargetBiomeID;
            Footprint.MorokImpact = PotionState.Meta.Distortion;
            Footprint.ZaryanaImpact = 1.f - PotionState.Meta.Distortion;
            Footprint.AxisDelta = AxisDeltaForFootprint;
            OutDelta.Footprints.Add(Footprint);
        }

        // 3. Если крафт – создаём зелье в инвентаре и выходим
        if (Cmd.bIsCrafting)
        {
            // Ash/BoiledWater — не зелье, а вырожденный результат (05_Systems.md:
            // "обязательность воды"/"только вода"). UI (AlchemySlotWidget,
            // ItemTooltipWidget) уже умеет отображать эти IngredientID отдельно —
            // раньше сюда всегда попадал "Potion" независимо от Outcome, и эти
            // ветки в UI были недостижимы. Collapse (Catastrophe) остаётся
            // "Potion" — по GDD это по-прежнему зелье, просто испорченное.
            FName PotionIngredientID = FName(TEXT("Potion"));
            if (Outcome == EAlchemyOutcome::Ash) PotionIngredientID = FName(TEXT("Ash"));
            else if (Outcome == EAlchemyOutcome::BoiledWater) PotionIngredientID = FName(TEXT("BoiledWater"));

            FInventoryItem PotionItem;
            PotionItem.IngredientID = PotionIngredientID;
            PotionItem.State = PotionState;
            PotionItem.Count = 1;
            PotionItem.CreationTime = WorldSnap.WorldTime;
            PotionItem.bSubjectToDecay = false;
            // Честный факт исхода варки на самом предмете (см. FInventoryItem::
            // BrewOutcome) -- нужен фольклорной системе имён (HerbalistNameUtils.cpp),
            // чтобы Purified/Catastrophe получали своё собственное имя, а не
            // угадывались задним числом по осям.
            PotionItem.BrewOutcome = Outcome;

            FInventoryOperation AddOp;
            AddOp.ContainerID = 0;
            AddOp.Ingredient = PotionItem;
            AddOp.OpType = EInventoryOpType::Add;
            AddOp.Amount = 1;
            AddOp.Coherence = EffectiveIntent.Coherence;
            OutDelta.InventoryOps.Add(AddOp);

            UE_LOG(LogHerbalistSimulation, Log, TEXT("Crafted potion: Outcome=%d M=%.2f, Dist=%.2f, Purity=%.2f"),
                (int32)Outcome, PotionState.Magnitude, PotionState.Meta.Distortion, PotionState.Meta.Purity);
            return;
        }

        // 4. Иначе – применение на клетку
        if (!TargetCell)
        {
            UE_LOG(LogHerbalistSimulation, Warning, TEXT("Apply target cell (%d,%d) not found"), Cmd.TargetCell.X, Cmd.TargetCell.Y);
            return;
        }

        FGridCell Modified = *TargetCell;
        Modified.State = PotionState;

        // Передозировка (обсуждение в сессии 2026-08-24) — см. HerbalistCoreMath.h.
        const UHerbalistSettings* OverdoseSettings = GetHerbalistSettings();
        HerbalistCore::Math::ApplyOverdosePenalty(Modified.State,
            OverdoseSettings ? OverdoseSettings->PotionOverdoseThreshold : 0.75f,
            OverdoseSettings ? OverdoseSettings->PotionOverdosePenalty : 0.5f);

        Modified.HarvestStress = FMath::Clamp(TargetCell->HarvestStress + 0.2f, 0.f, 1.f);
        OutDelta.WorldChanges.Add(Cmd.TargetCell, Modified);

        UE_LOG(LogHerbalistSimulation, Log, TEXT("Applied potion to cell (%d,%d): Outcome=%d M=%.2f, Dist=%.2f"),
            Cmd.TargetCell.X, Cmd.TargetCell.Y, (int32)Outcome, PotionState.Magnitude, PotionState.Meta.Distortion);
    }

    // ---------------------------------------------------------
    // Главная точка входа
    // ---------------------------------------------------------
    FStateDelta ExecutePipeline(const FWorldSnapshot& WorldSnapshot,
                                const FInventorySnapshot& InventorySnapshot,
                                const FBiomeSnapshot& BiomeSnapshot,
                                const FCommandBatch& Commands,
                                FRandomStream& Rng)
    {
        FStateDelta Delta;

        for (const FCommandEntry& Entry : Commands.Commands)
        {
            if (Entry.bCancelled) continue;

            switch (Entry.Primitive)
            {
            case ECommandPrimitive::Harvest:
                ProcessHarvestCommand(Entry.Harvest, WorldSnapshot, Rng, Delta);
                break;
            case ECommandPrimitive::Transfer:
                ProcessTransferCommand(Entry.Transfer, InventorySnapshot, Delta);
                break;
            case ECommandPrimitive::Apply:
                ProcessApplyCommand(Entry.Apply, WorldSnapshot, BiomeSnapshot, Rng, Delta);
                break;
            default:
                break;
            }
        }

        return Delta;
    }
}