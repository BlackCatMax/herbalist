// Source/ProjectHerbalistTests/Private/Tests/HerbalistNameUtilsTest.cpp
//
// Фольклорная система имён зелий (2026-08-30, "не топорное 'Сильное зелье
// здоровья', а явно фольклорное и с нормальными падежами", GeneratePotionName/
// GetItemDisplayName в HerbalistNameUtils.cpp). Проверяет: выбор основы по
// доминирующей оси Direction и исходу варки (Valid/Purified/Catastrophe),
// выбор эпитета по приоритету качественных сигналов, что все 6 падежей
// реально дают разные строки (не забытый Get() на дефолт), и что
// GetItemDisplayName честно читает BrewOutcome с самого предмета.

#include "Core/Types/HerbalistNameUtils.h"
#include "Core/Types/HerbalistCoreTypes.h"
#include "Misc/AutomationTest.h"

#if WITH_AUTOMATION_TESTS

namespace
{
    FRealState MakeState(float Body, float Mind, float Spirit, float Nature,
        float Distortion = 0.1f, float Purity = 0.5f, float Stability = 0.5f,
        float Corruption = 0.1f, float Potency = 0.3f)
    {
        FRealState S;
        S.Direction.Body = Body;
        S.Direction.Mind = Mind;
        S.Direction.Spirit = Spirit;
        S.Direction.Nature = Nature;
        S.Meta.Distortion = Distortion;
        S.Meta.Purity = Purity;
        S.Meta.Stability = Stability;
        S.Meta.Corruption = Corruption;
        S.Meta.Potency = Potency;
        return S;
    }
}

// ---------------------------------------------------------------------------
// Valid: основа зависит от доминирующей оси Direction -- 4 разных слова,
// не одно "зелье" на все случаи.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistNameUtils_ValidNounFollowsDominantAxis,
    "Herbalist.NameUtils.ValidNounFollowsDominantAxis",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistNameUtils_ValidNounFollowsDominantAxis::RunTest(const FString& Parameters)
{
    const FString BodyName = GeneratePotionName(EAlchemyOutcome::Valid, MakeState(1.f, 0.f, 0.f, 0.f)).ToString();
    const FString MindName = GeneratePotionName(EAlchemyOutcome::Valid, MakeState(0.f, 1.f, 0.f, 0.f)).ToString();
    const FString SpiritName = GeneratePotionName(EAlchemyOutcome::Valid, MakeState(0.f, 0.f, 1.f, 0.f)).ToString();
    const FString NatureName = GeneratePotionName(EAlchemyOutcome::Valid, MakeState(0.f, 0.f, 0.f, 1.f)).ToString();

    TestTrue(TEXT("Body-dominant uses vzvar"), BodyName.Contains(TEXT("взвар")));
    TestTrue(TEXT("Mind-dominant uses durman"), MindName.Contains(TEXT("дурман")));
    TestTrue(TEXT("Spirit-dominant uses voda"), SpiritName.Contains(TEXT("вод")));
    TestTrue(TEXT("Nature-dominant uses nastoy"), NatureName.Contains(TEXT("насто")));

    // Все четыре -- разные слова, не совпадающие друг с другом.
    TestNotEqual(TEXT("Body != Mind"), BodyName, MindName);
    TestNotEqual(TEXT("Spirit != Nature"), SpiritName, NatureName);
    return true;
}

// ---------------------------------------------------------------------------
// Эпитет по приоритету: Corruption > Distortion > Purity+Stability > Stability
// > Potency > фолбэк. Каждый порог реально достижим и реально выбирает СВОЙ
// эпитет, не молча проваливается в фолбэк.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistNameUtils_EpithetFollowsQualityPriority,
    "Herbalist.NameUtils.EpithetFollowsQualityPriority",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistNameUtils_EpithetFollowsQualityPriority::RunTest(const FString& Parameters)
{
    // Body-доминанта (взвар, м.р.) для всех, чтобы менялась только ось качества.
    auto NameFor = [](float Distortion, float Purity, float Stability, float Corruption, float Potency)
    {
        return GeneratePotionName(EAlchemyOutcome::Valid,
            MakeState(1.f, 0.f, 0.f, 0.f, Distortion, Purity, Stability, Corruption, Potency)).ToString();
    };

    const FString Corrupted = NameFor(0.1f, 0.1f, 0.1f, 0.9f, 0.1f);
    const FString Distorted = NameFor(0.9f, 0.1f, 0.1f, 0.1f, 0.1f);
    const FString Bright    = NameFor(0.1f, 0.9f, 0.9f, 0.1f, 0.1f);
    const FString Strong    = NameFor(0.1f, 0.3f, 0.9f, 0.1f, 0.1f);
    const FString Heady     = NameFor(0.1f, 0.3f, 0.3f, 0.1f, 0.9f);
    const FString Bland     = NameFor(0.3f, 0.3f, 0.3f, 0.1f, 0.1f);

    // Ищем окончание слова, не начало -- эпитет всегда первое слово фразы и
    // потому с заглавной буквы (Combine/Capitalize); FString::Contains по
    // умолчанию регистронезависим, а регистронезависимое сравнение кириллицы
    // в этом окружении тоже упирается в локаль C-рантайма (та же причина,
    // что и у Capitalize -- см. комментарий в HerbalistNameUtils.cpp),
    // поэтому "П" не находит "п". Само окончание слова везде lowercase,
    // сравнение по нему не задевает эту проблему.
    TestTrue(TEXT("High Corruption -> poganyi"), Corrupted.Contains(TEXT("оганый")));
    TestTrue(TEXT("High Distortion (Corruption low) -> smutnyi"), Distorted.Contains(TEXT("мутный")));
    TestTrue(TEXT("High Purity+Stability -> svetlyi"), Bright.Contains(TEXT("ветлый")));
    TestTrue(TEXT("High Stability alone -> krepkiy"), Strong.Contains(TEXT("репкий")));
    TestTrue(TEXT("High Potency alone -> khmelnoy"), Heady.Contains(TEXT("мельной")));
    TestTrue(TEXT("Nothing stands out -> khilyi fallback"), Bland.Contains(TEXT("илый")));

    // Приоритет реально соблюдается: Corruption ПЕРЕВЕШИВАЕТ одновременно
    // высокий Distortion, не наоборот.
    const FString Both = NameFor(0.9f, 0.1f, 0.1f, 0.9f, 0.1f);
    TestTrue(TEXT("Corruption wins over simultaneous Distortion (priority order)"), Both.Contains(TEXT("оганый")));
    return true;
}

// ---------------------------------------------------------------------------
// Находка диагностики 2026-09-04 (см. CHANGELOG.md, "Пороги эпитетов не
// пересчитаны под новую формулу варки" -- закрыто): пороги 0.6/0.65 сами по
// себе достижимы (полный перебор реальных пар DT_IngredientClass даёт
// эпитет выше "хилого" в ~92% Valid-исходов), но GetValidEpithet вообще не
// смотрела на Magnitude -- честно мощная варка без выраженной оси качества
// молча получала "Хилую", неотличимую от реально слабой. "Могутный" --
// седьмой эпитет для этого хвоста, порог 0.45 откалиброван по измеренному
// распределению (p90 Magnitude по полному перебору пар/выборке троек = 0.442).
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistNameUtils_HighMagnitudeWithoutQualitySignalIsMogutnyNotKhilyi,
    "Herbalist.NameUtils.HighMagnitudeWithoutQualitySignalIsMogutnyNotKhilyi",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistNameUtils_HighMagnitudeWithoutQualitySignalIsMogutnyNotKhilyi::RunTest(const FString& Parameters)
{
    // Все качественные оси нейтральны (0.3, ниже любого из 0.5/0.6/0.65
    // порогов выше) -- без Magnitude это ровно "Bland" case из теста
    // приоритета, честный "хилый" фолбэк.
    FRealState Weak = MakeState(1.f, 0.f, 0.f, 0.f, 0.3f, 0.3f, 0.3f, 0.1f, 0.1f);
    Weak.Magnitude = 0.1f;
    const FString WeakName = GeneratePotionName(EAlchemyOutcome::Valid, Weak).ToString();
    TestTrue(TEXT("Low Magnitude, no quality signal -> khilyi (genuinely weak)"), WeakName.Contains(TEXT("илый")));

    // Ровно та же качественная картина, но Magnitude далеко за порогом
    // (0.45) -- честно мощная варка, обязана звучать иначе.
    FRealState Strong = Weak;
    Strong.Magnitude = 0.6f;
    const FString StrongName = GeneratePotionName(EAlchemyOutcome::Valid, Strong).ToString();
    TestTrue(TEXT("High Magnitude, no quality signal -> mogutnyi, not khilyi"), StrongName.Contains(TEXT("огутн")));
    TestFalse(TEXT("High Magnitude no longer falls back to khilyi"), StrongName.Contains(TEXT("илый")));

    // Порог реально пороговый, не всегда включён: чуть ниже 0.45 всё ещё
    // хилый.
    FRealState JustBelow = Weak;
    JustBelow.Magnitude = 0.4f;
    const FString JustBelowName = GeneratePotionName(EAlchemyOutcome::Valid, JustBelow).ToString();
    TestTrue(TEXT("Magnitude just below 0.45 still falls back to khilyi"), JustBelowName.Contains(TEXT("илый")));

    // Приоритет: Magnitude -- самый последний сигнал, не перевешивает ни один
    // из пяти качественных эпитетов, даже если Magnitude тоже высокий.
    FRealState CorruptAndPowerful = MakeState(1.f, 0.f, 0.f, 0.f, 0.1f, 0.1f, 0.1f, /*Corruption*/ 0.9f, 0.1f);
    CorruptAndPowerful.Magnitude = 0.9f;
    const FString CorruptAndPowerfulName = GeneratePotionName(EAlchemyOutcome::Valid, CorruptAndPowerful).ToString();
    TestTrue(TEXT("Corruption still wins over high Magnitude (priority order unchanged)"), CorruptAndPowerfulName.Contains(TEXT("оганый")));
    return true;
}

// ---------------------------------------------------------------------------
// Purified/Catastrophe -- отдельные, узнаваемые семьи слов, не смешиваются
// с обычной Valid-варкой и друг с другом.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistNameUtils_RareOutcomesGetDistinctNames,
    "Herbalist.NameUtils.RareOutcomesGetDistinctNames",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistNameUtils_RareOutcomesGetDistinctNames::RunTest(const FString& Parameters)
{
    const FString PurifiedBody = GeneratePotionName(EAlchemyOutcome::Purified, MakeState(1.f, 0.f, 0.f, 0.f)).ToString();
    const FString PurifiedMind = GeneratePotionName(EAlchemyOutcome::Purified, MakeState(0.f, 1.f, 0.f, 0.f)).ToString();
    const FString CatastropheBody = GeneratePotionName(EAlchemyOutcome::Catastrophe, MakeState(1.f, 0.f, 0.f, 0.f)).ToString();
    const FString CatastropheMind = GeneratePotionName(EAlchemyOutcome::Catastrophe, MakeState(0.f, 1.f, 0.f, 0.f)).ToString();
    const FString ValidBody = GeneratePotionName(EAlchemyOutcome::Valid, MakeState(1.f, 0.f, 0.f, 0.f)).ToString();

    TestTrue(TEXT("Purified is voda-based"), PurifiedBody.Contains(TEXT("вод")) && PurifiedMind.Contains(TEXT("вод")));
    TestTrue(TEXT("Catastrophe is varevo-based"), CatastropheBody.Contains(TEXT("варев")) && CatastropheMind.Contains(TEXT("варев")));
    TestNotEqual(TEXT("Purified reads differently depending on axis"), PurifiedBody, PurifiedMind);
    TestNotEqual(TEXT("Catastrophe reads differently depending on axis"), CatastropheBody, CatastropheMind);
    TestNotEqual(TEXT("Purified never equals a plain Valid brew of the same axis"), PurifiedBody, ValidBody);
    TestNotEqual(TEXT("Purified never literally reuses Buyan's 'живая вода'"), PurifiedBody, FString(TEXT("Живая вода")));
    TestNotEqual(TEXT("Catastrophe never literally reuses Buyan's 'мёртвая вода'"), CatastropheBody, FString(TEXT("Мёртвая вода")));
    return true;
}

// ---------------------------------------------------------------------------
// Ash/BoiledWater -- фиксированные простые имена, но тоже честно склоняются.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistNameUtils_AshAndBoiledWaterStayFixed,
    "Herbalist.NameUtils.AshAndBoiledWaterStayFixed",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistNameUtils_AshAndBoiledWaterStayFixed::RunTest(const FString& Parameters)
{
    const FRealState Any = MakeState(1.f, 0.f, 0.f, 0.f);
    TestEqual(TEXT("Ash nominative"), GeneratePotionName(EAlchemyOutcome::Ash, Any).ToString(), FString(TEXT("Зола")));
    TestEqual(TEXT("BoiledWater nominative"), GeneratePotionName(EAlchemyOutcome::BoiledWater, Any).ToString(), FString(TEXT("Кипячёная вода")));
    TestEqual(TEXT("Ash genitive"), GeneratePotionName(EAlchemyOutcome::Ash, Any, EGrammaticalCase::Genitive).ToString(), FString(TEXT("Золы")));
    return true;
}

// ---------------------------------------------------------------------------
// Склонение: все 6 падежей реально дают разные, непустые строки -- не забытый
// дефолт на Nominative. Проверяем на богатом (эпитет+существительное) и
// притяжательном (навье, Catastrophe) случаях -- разные парадигмы склонения.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistNameUtils_AllSixCasesProduceDistinctForms,
    "Herbalist.NameUtils.AllSixCasesProduceDistinctForms",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistNameUtils_AllSixCasesProduceDistinctForms::RunTest(const FString& Parameters)
{
    const EGrammaticalCase Cases[] = {
        EGrammaticalCase::Nominative, EGrammaticalCase::Genitive, EGrammaticalCase::Dative,
        EGrammaticalCase::Accusative, EGrammaticalCase::Instrumental, EGrammaticalCase::Prepositional
    };

    // Обычная варка (Body, поганое -- твёрдое прилагательное + сущ. м.р.).
    {
        const FRealState State = MakeState(1.f, 0.f, 0.f, 0.f, 0.1f, 0.1f, 0.1f, 0.9f, 0.1f);
        TSet<FString> Seen;
        for (EGrammaticalCase C : Cases)
        {
            const FString Form = GeneratePotionName(EAlchemyOutcome::Valid, State, C).ToString();
            TestFalse(FString::Printf(TEXT("Case %d produces a non-empty name"), (int32)C), Form.IsEmpty());
            Seen.Add(Form);
        }
        // Именительный/Винительный у "поганый взвар" (неодуш., м.р.) законно
        // совпадают -- это грамматически верно, не баг склонения (стандартное
        // свойство неодушевлённых существительных м.р.). Дательный/Творительный/
        // Предложный/Родительный обязаны отличаться от остальных.
        TestEqual(TEXT("Nominative == Accusative for this inanimate masculine noun (grammatically correct)"),
            GeneratePotionName(EAlchemyOutcome::Valid, State, EGrammaticalCase::Nominative).ToString(),
            GeneratePotionName(EAlchemyOutcome::Valid, State, EGrammaticalCase::Accusative).ToString());
        TestTrue(TEXT("At least 5 distinct surface forms across 6 cases (Nom==Acc is the only legitimate collision)"),
            Seen.Num() >= 5);
    }

    // Навье варево (притяжательная парадигма, Catastrophe/Mind) -- отдельно
    // проверяем, что она не выродилась в одну и ту же форму везде.
    {
        const FRealState State = MakeState(0.f, 1.f, 0.f, 0.f);
        TSet<FString> Seen;
        for (EGrammaticalCase C : Cases)
        {
            Seen.Add(GeneratePotionName(EAlchemyOutcome::Catastrophe, State, C).ToString());
        }
        TestTrue(TEXT("Navye (possessive-type declension) also yields multiple distinct forms"), Seen.Num() >= 4);
    }
    return true;
}

// ---------------------------------------------------------------------------
// Разбор дисбаланса имён зелий (2026-09-04, см. CHANGELOG.md): голый argmax
// по осям Direction почти всегда выбирал Nature -> "Настой" (60% валидных
// варок), причём почти половина этих случаев -- зазор <0.10 между 1-й и 2-й
// осью, то есть argmax звучал куда увереннее, чем сама математика внутри.
// GetDominantAxis теперь при зазоре < NounTieBreakGapThreshold (0.2, по
// медиане измеренного распределения) даёт шанс второй оси -- но не через
// RNG (GeneratePotionName вызывается заново на каждую перерисовку UI), а
// через детерминированный хэш точных чисел State.Direction.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistNameUtils_SameStateAlwaysProducesSameName,
    "Herbalist.NameUtils.TieBreak.SameStateAlwaysProducesSameName",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistNameUtils_SameStateAlwaysProducesSameName::RunTest(const FString& Parameters)
{
    // Близкий зазор (0.50 vs 0.45 = 0.05) -- ровно тот случай, где раньше
    // argmax был бы "уверенным", а теперь есть шанс на вторую ось. Вызов
    // GeneratePotionName дважды с идентичным State обязан дать идентичный
    // результат -- имитирует повторную перерисовку одного и того же слота.
    const FRealState State = MakeState(0.f, 0.45f, 0.f, 0.50f);
    const FString First = GeneratePotionName(EAlchemyOutcome::Valid, State).ToString();
    const FString Second = GeneratePotionName(EAlchemyOutcome::Valid, State).ToString();
    TestEqual(TEXT("Identical State called twice yields identical name (no RNG re-roll per UI redraw)"), First, Second);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistNameUtils_ConfidentMarginAlwaysKeepsDominantAxis,
    "Herbalist.NameUtils.TieBreak.ConfidentMarginAlwaysKeepsDominantAxis",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistNameUtils_ConfidentMarginAlwaysKeepsDominantAxis::RunTest(const FString& Parameters)
{
    // Nature=0.7 vs Mind=0.3 -- зазор 0.4, далеко выше порога 0.2. Body/Spirit
    // варьируются только чтобы поменять хэш (не влияют на топ-2), проверяем,
    // что при явном отрыве вторая ось не получает шанса вообще -- ни при
    // каком хэше.
    TSet<FString> Seen;
    for (int32 i = 0; i < 10; ++i)
    {
        const FRealState State = MakeState(0.001f * i, 0.3f, 0.002f * i, 0.7f);
        Seen.Add(GeneratePotionName(EAlchemyOutcome::Valid, State).ToString());
    }
    TestEqual(TEXT("Wide margin (gap=0.4) always resolves to the dominant axis regardless of hash"), Seen.Num(), 1);

    // Зазор ровно НА пороге (0.5-0.3=0.2) -- по формуле шанс второй оси в
    // этой точке равен нулю (0.5*(1-0.2/0.2)=0), но формально попадает в
    // ветку Gap>=Threshold и решается без обращения к хэшу вовсе.
    const FRealState AtThreshold = MakeState(0.f, 0.3f, 0.f, 0.5f);
    TestTrue(TEXT("Gap exactly at threshold still resolves to the dominant axis (Nature/nastoy)"),
        GeneratePotionName(EAlchemyOutcome::Valid, AtThreshold).ToString().Contains(TEXT("насто")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistNameUtils_CloseMarginSometimesFlipsToSecondAxis,
    "Herbalist.NameUtils.TieBreak.CloseMarginSometimesFlipsToSecondAxis",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistNameUtils_CloseMarginSometimesFlipsToSecondAxis::RunTest(const FString& Parameters)
{
    // Nature=0.50 vs Mind=0.45 -- зазор 0.05, глубоко внутри зоны неуверенности
    // (шанс второй оси = 0.5*(1-0.05/0.2) = 37.5%). Body/Spirit варьируются,
    // чтобы получить 30 разных хэшей при одном и том же зазоре -- если
    // подбрасывание работает, среди 30 бросков почти наверняка встретятся
    // ОБА исхода (P(все одинаковы) < 1e-6 при 37.5%/62.5%).
    TSet<FString> DistinctNouns;
    for (int32 i = 0; i < 30; ++i)
    {
        const FRealState State = MakeState(0.001f * i, 0.45f, 0.002f * i, 0.50f);
        const FString Name = GeneratePotionName(EAlchemyOutcome::Valid, State).ToString();
        DistinctNouns.Add(Name.Contains(TEXT("насто")) ? TEXT("nastoy") : (Name.Contains(TEXT("дурман")) ? TEXT("durman") : TEXT("other")));
    }
    TestTrue(TEXT("Close margin (gap=0.05) produces both nastoy and durman across varied hashes, not always the argmax winner"),
        DistinctNouns.Num() >= 2);
    return true;
}

// ---------------------------------------------------------------------------
// GetItemDisplayName честно читает BrewOutcome с предмета, не всегда Valid.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistNameUtils_DisplayNameReadsBrewOutcomeFromItem,
    "Herbalist.NameUtils.DisplayNameReadsBrewOutcomeFromItem",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistNameUtils_DisplayNameReadsBrewOutcomeFromItem::RunTest(const FString& Parameters)
{
    FInventoryItem ValidPotion;
    ValidPotion.IngredientID = FName(TEXT("Potion"));
    ValidPotion.State = MakeState(1.f, 0.f, 0.f, 0.f);
    ValidPotion.BrewOutcome = EAlchemyOutcome::Valid;

    FInventoryItem CatastrophePotion = ValidPotion;
    CatastrophePotion.BrewOutcome = EAlchemyOutcome::Catastrophe;

    const FString ValidName = GetItemDisplayName(ValidPotion, nullptr);
    const FString CatastropheName = GetItemDisplayName(CatastrophePotion, nullptr);

    TestNotEqual(TEXT("Same State, different BrewOutcome -> different displayed name"), ValidName, CatastropheName);
    TestTrue(TEXT("Catastrophe potion is displayed as varevo, not a normal brew"), CatastropheName.Contains(TEXT("варев")));
    return true;
}

#endif
