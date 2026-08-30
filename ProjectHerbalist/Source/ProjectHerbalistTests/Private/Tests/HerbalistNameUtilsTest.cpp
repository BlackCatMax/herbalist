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
