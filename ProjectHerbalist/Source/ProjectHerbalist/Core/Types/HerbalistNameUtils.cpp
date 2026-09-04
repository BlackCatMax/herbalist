#include "HerbalistNameUtils.h"
#include "Core/Subsystems/IngredientRegistrySubsystem.h"
#include "Core/Data/IngredientTableRow.h"

// ============================================================================
// Фольклорные имена зелий (2026-08-30, "не топорное 'Сильное зелье здоровья',
// а явно фольклорное и с нормальными падежами").
//
// Раньше GeneratePotionName собирала имя по шаблону "<Качество> <Ось> зелье"
// (Чистое Телесное зелье) — абстрактные слова-ярлыки, ничем не отличающиеся
// от голого статус-бара. Компендиум (04_Compendium/Растительность) уже
// говорит другим голосом — конкретные, персонифицированные фольклорные имена
// (Сон-трава, Одолень-трава), не формулы "прилагательное+существительное по
// оси". Эта система приближается к тому же регистру: зелье называется одним
// из пяти конкретных фольклорных слов-основ (не всегда "зелье"), к которому
// клеится один фольклорный эпитет — тот же принцип, что уже даёт лору игры
// пара "живая вода"/"мёртвая вода" (Буян, DESIGN_Narrative_And_Craft.md).
// Сама эта пара НАРОЧНО не переиспользуется здесь буквально — она зарезерв-
// ирована за концовкой, обычная варка получает СОСЕДНИЕ, не тождественные
// имена того же регистра ("непочатая вода", не "живая вода").
//
// Порядок выбора слов:
//   1. Исход варки (FInventoryItem::BrewOutcome) решает, из какой СЕМЬИ слов
//      берётся основа — обычная варка (Valid), редкая удачная чистка
//      (Purified) или катастрофа (Catastrophe) звучат заметно по-разному.
//      Ash/BoiledWater — вырожденные исходы, у них уже были фиксированные
//      простые имена ("Зола"/"Кипячёная вода") — здесь только доводим их до
//      того же уровня склонения, словарь под них не меняется.
//   2. Для Valid — основа зависит от доминирующей оси Direction (Body/Mind/
//      Spirit/Nature): у каждой оси свой, отдельно подобранный фольклорный
//      термин (взвар/дурман/вода/настой), не один термин на все случаи.
//   3. Эпитет — по самому выраженному качественному сигналу среди Corruption/
//      Distortion/Purity/Stability/Potency, в этом порядке приоритета (порча
//      и морок — самые "громкие" по нарративу сигналы, поэтому проверяются
//      первыми). Если ни один качественный сигнал не выделился, но зелье
//      само по себе объективно МОЩНОЕ (Magnitude > 0.45, см. диагностику
//      2026-09-04 в CHANGELOG.md) — "могутный", седьмой эпитет, а не
//      "хилый": честная сила должна звучать честно, не тонуть в фолбэке
//      вместе с реально невыразительной варкой. Purified/Catastrophe
//      используют свою отдельную, более узкую пару эпитетов вместо общей
//      семёрки — редкие исходы должны звучать особенно, не просто "очень
//      чистая обычная вода".
//
// Склонение — по прямому решению пользователя не алгоритмическое (риск
// тонких грамматических ошибок на нерегулярных фольклорных словах), а
// заранее прописанные таблицы форм на каждое слово словаря. Стандартное
// твёрдое/мягкое склонение прилагательных (-ый/-ая/-ое, -ий/-ая/-ое,
// -ой/-ая/-ое) и существительных 1-го/2-го склонения — везде, кроме "навье"
// (притяжательного типа, как "оленье", своя, отдельно прописанная парадигма).
// ============================================================================

namespace
{
    enum class EPotionGender : uint8 { Masculine, Feminine, Neuter };

    // Формы одного слова по 6 падежам (Им/Род/Дат/Вин/Твор/Пред), единственное число.
    struct FCaseForms
    {
        const TCHAR* Nom;
        const TCHAR* Gen;
        const TCHAR* Dat;
        const TCHAR* Acc;
        const TCHAR* Instr;
        const TCHAR* Prep;

        const TCHAR* Get(EGrammaticalCase C) const
        {
            switch (C)
            {
                case EGrammaticalCase::Genitive:      return Gen;
                case EGrammaticalCase::Dative:        return Dat;
                case EGrammaticalCase::Accusative:    return Acc;
                case EGrammaticalCase::Instrumental:  return Instr;
                case EGrammaticalCase::Prepositional: return Prep;
                default:                               return Nom;
            }
        }
    };

    struct FPotionNoun
    {
        EPotionGender Gender;
        FCaseForms Forms;
    };

    // Эпитет — прилагательное, обязано согласовываться в роде с существи-
    // тельным, к которому клеится, поэтому хранит все три родовые формы разом.
    struct FPotionEpithet
    {
        FCaseForms Masc, Fem, Neut;

        const FCaseForms& ForGender(EPotionGender G) const
        {
            switch (G)
            {
                case EPotionGender::Feminine: return Fem;
                case EPotionGender::Neuter:   return Neut;
                default:                       return Masc;
            }
        }
    };

    // ---- Основы (существительные) ----

    // Взвар — Body (телесная, "на силу и рост" сторона): старинное слово для
    // густого травяного варева, никак не пересекается с абстрактным "зелье".
    const FPotionNoun NounVzvar{ EPotionGender::Masculine,
        { TEXT("взвар"), TEXT("взвара"), TEXT("взвару"), TEXT("взвар"), TEXT("взваром"), TEXT("о взваре") } };

    // Дурман — Mind: то же слово, что уже называет ингредиент "Багульник...
    // дурман-трава проклятая" в компендиуме (04_Compendium/Растительность) —
    // прямая перекличка регистра, не изобретённое с нуля слово.
    const FPotionNoun NounDurman{ EPotionGender::Masculine,
        { TEXT("дурман"), TEXT("дурмана"), TEXT("дурману"), TEXT("дурман"), TEXT("дурманом"), TEXT("о дурмане") } };

    // Настой — Nature: самое плоское, "травяное" из пяти слов — нарочно,
    // Природная ось не должна звучать драматичнее самой обычной травы.
    const FPotionNoun NounNastoy{ EPotionGender::Masculine,
        { TEXT("настой"), TEXT("настоя"), TEXT("настою"), TEXT("настой"), TEXT("настоем"), TEXT("о настое") } };

    // Вода — Spirit (заговорённая вода, "заговор на воду" — настоящая форма
    // народной магии) И отдельно, с другими эпитетами, база для Purified.
    const FPotionNoun NounVoda{ EPotionGender::Feminine,
        { TEXT("вода"), TEXT("воды"), TEXT("воде"), TEXT("воду"), TEXT("водой"), TEXT("о воде") } };

    // Варево — база для Catastrophe: разговорно-пренебрежительное слово для
    // неудачного, неопределённого варева ("наварили какого-то варева").
    const FPotionNoun NounVarevo{ EPotionGender::Neuter,
        { TEXT("варево"), TEXT("варева"), TEXT("вареву"), TEXT("варево"), TEXT("варевом"), TEXT("о вареве") } };

    const FPotionNoun NounZola{ EPotionGender::Feminine,
        { TEXT("зола"), TEXT("золы"), TEXT("золе"), TEXT("золу"), TEXT("золой"), TEXT("о золе") } };

    // ---- Общие эпитеты (Valid) — по самому выраженному качеству ----

    // Corruption высокая — "поганое место" уже устойчивое фольклорное
    // выражение для оскверненного/нечистого места, тот же корень здесь.
    const FPotionEpithet EpithetPoganyi{
        { TEXT("поганый"), TEXT("поганого"), TEXT("поганому"), TEXT("поганый"), TEXT("поганым"), TEXT("о поганом") },
        { TEXT("поганая"), TEXT("поганой"), TEXT("поганой"), TEXT("поганую"), TEXT("поганой"), TEXT("о поганой") },
        { TEXT("поганое"), TEXT("поганого"), TEXT("поганому"), TEXT("поганое"), TEXT("поганым"), TEXT("о поганом") }
    };

    // Distortion высокая — рябь, мутность, "неправильность" (см. Восприятие
    // в 01_Glossary/Distortion.md).
    const FPotionEpithet EpithetSmutnyi{
        { TEXT("смутный"), TEXT("смутного"), TEXT("смутному"), TEXT("смутный"), TEXT("смутным"), TEXT("о смутном") },
        { TEXT("смутная"), TEXT("смутной"), TEXT("смутной"), TEXT("смутную"), TEXT("смутной"), TEXT("о смутной") },
        { TEXT("смутное"), TEXT("смутного"), TEXT("смутному"), TEXT("смутное"), TEXT("смутным"), TEXT("о смутном") }
    };

    // Purity высокая (и Stability не низкая) — прозрачность, блеск (см.
    // Восприятие в 01_Glossary/Purity.md).
    const FPotionEpithet EpithetSvetlyi{
        { TEXT("светлый"), TEXT("светлого"), TEXT("светлому"), TEXT("светлый"), TEXT("светлым"), TEXT("о светлом") },
        { TEXT("светлая"), TEXT("светлой"), TEXT("светлой"), TEXT("светлую"), TEXT("светлой"), TEXT("о светлой") },
        { TEXT("светлое"), TEXT("светлого"), TEXT("светлому"), TEXT("светлое"), TEXT("светлым"), TEXT("о светлом") }
    };

    // Stability высокая — обычная бытовая коллокация "крепкий отвар/настой".
    const FPotionEpithet EpithetKrepkiy{
        { TEXT("крепкий"), TEXT("крепкого"), TEXT("крепкому"), TEXT("крепкий"), TEXT("крепким"), TEXT("о крепком") },
        { TEXT("крепкая"), TEXT("крепкой"), TEXT("крепкой"), TEXT("крепкую"), TEXT("крепкой"), TEXT("о крепкой") },
        { TEXT("крепкое"), TEXT("крепкого"), TEXT("крепкому"), TEXT("крепкое"), TEXT("крепким"), TEXT("о крепком") }
    };

    // Potency высокая — тоже бытовая коллокация ("хмельной напиток"),
    // передаёт силу воздействия без привязки к конкретной оси Direction.
    const FPotionEpithet EpithetKhmelnoy{
        { TEXT("хмельной"), TEXT("хмельного"), TEXT("хмельному"), TEXT("хмельной"), TEXT("хмельным"), TEXT("о хмельном") },
        { TEXT("хмельная"), TEXT("хмельной"), TEXT("хмельной"), TEXT("хмельную"), TEXT("хмельной"), TEXT("о хмельной") },
        { TEXT("хмельное"), TEXT("хмельного"), TEXT("хмельному"), TEXT("хмельное"), TEXT("хмельным"), TEXT("о хмельном") }
    };

    // Фолбэк — ни один сигнал не выражен заметно (ближе к нейтральной варке,
    // чем к любой из пяти крайностей выше).
    const FPotionEpithet EpithetKhilyi{
        { TEXT("хилый"), TEXT("хилого"), TEXT("хилому"), TEXT("хилый"), TEXT("хилым"), TEXT("о хилом") },
        { TEXT("хилая"), TEXT("хилой"), TEXT("хилой"), TEXT("хилую"), TEXT("хилой"), TEXT("о хилой") },
        { TEXT("хилое"), TEXT("хилого"), TEXT("хилому"), TEXT("хилое"), TEXT("хилым"), TEXT("о хилом") }
    };

    // Могутный — ни один КАЧЕСТВЕННЫЙ сигнал не выделяется (иначе выше уже
    // сработал бы один из пяти эпитетов), но само зелье объективно СИЛЬНОЕ
    // (Magnitude). Диагностика 2026-09-04 (см. CHANGELOG.md, "Пороги
    // эпитетов..."): GetValidEpithet до этой правки вообще не смотрела на
    // Magnitude (сигнатура принимала только FMeta) — честная находка не в
    // том, что пороги 0.6/0.65 недостижимы (реальные сочетания трав из
    // DT_IngredientClass пересекают их в ~92% случаев, посчитано на полном
    // переборе пар и выборке троек), а в том, что узкий "хилая, но мощная"
    // хвост распределения (мощь выросла, ни одна ось качества не выделилась)
    // молча тонул в общем фолбэке. "Могутный" — от "могута" (сила, мощь),
    // тот же фольклорный/былинный регистр, что и остальные пять эпитетов, не
    // изобретённое с нуля слово.
    const FPotionEpithet EpithetMogutnyi{
        { TEXT("могутный"), TEXT("могутного"), TEXT("могутному"), TEXT("могутный"), TEXT("могутным"), TEXT("о могутном") },
        { TEXT("могутная"), TEXT("могутной"), TEXT("могутной"), TEXT("могутную"), TEXT("могутной"), TEXT("о могутной") },
        { TEXT("могутное"), TEXT("могутного"), TEXT("могутному"), TEXT("могутное"), TEXT("могутным"), TEXT("о могутном") }
    };

    // ---- Особые эпитеты Purified (только форма женского рода, всегда клеится к "вода") ----

    // "Непочатая вода" — настоящий фольклорный термин: вода, набранная до
    // того, как ей кто-либо ещё воспользовался, считалась особенно сильной.
    const FCaseForms EpithetNepochataya{
        TEXT("непочатая"), TEXT("непочатой"), TEXT("непочатой"), TEXT("непочатую"), TEXT("непочатой"), TEXT("о непочатой") };

    // "Заревая вода" — от "заря", поэтический/обрядовый регистр (умывание
    // на заре и т.п.).
    const FCaseForms EpithetZarevaya{
        TEXT("заревая"), TEXT("заревой"), TEXT("заревой"), TEXT("заревую"), TEXT("заревой"), TEXT("о заревой") };

    // ---- Особые эпитеты Catastrophe (только форма среднего рода, всегда клеится к "варево") ----

    // "Мороковое варево" — от уже устоявшегося в проекте "Морок" (искажающая
    // сила, 01_Glossary/Morok.md), не изобретённое слово.
    const FCaseForms EpithetMorokovoe{
        TEXT("мороковое"), TEXT("морокового"), TEXT("мороковому"), TEXT("мороковое"), TEXT("мороковым"), TEXT("о мороковом") };

    // "Навье варево" — от "навь" (мир мёртвых/недобрых духов в славянском
    // фольклоре, "навья кость" — устойчивое выражение). Притяжательный тип
    // склонения (как "оленье"), не обычное твёрдое прилагательное.
    const FCaseForms EpithetNavye{
        TEXT("навье"), TEXT("навьего"), TEXT("навьему"), TEXT("навье"), TEXT("навьим"), TEXT("о навьем") };

    // Кипячёная (для "Кипячёная вода", BoiledWater) — обычное прилагательное,
    // клеится к NounVoda.
    const FCaseForms EpithetKipyachyonaya{
        TEXT("кипячёная"), TEXT("кипячёной"), TEXT("кипячёной"), TEXT("кипячёную"), TEXT("кипячёной"), TEXT("о кипячёной") };

    // FString::ToUpper() полагается на локаль C-рантайма (towupper) -- под
    // "C"/нейтральной локалью автотест-раннера это тихий no-op на кириллице
    // (найдено этим же тестом: "Зола" выходило как "зола"). Явная таблица по
    // первым буквам словаря — не зависит от локали процесса вообще.
    FString Capitalize(const FString& S)
    {
        if (S.IsEmpty()) return S;
        FString Result = S;
        TCHAR& First = Result[0];
        switch (First)
        {
            case TEXT('в'): First = TEXT('В'); break;
            case TEXT('д'): First = TEXT('Д'); break;
            case TEXT('з'): First = TEXT('З'); break;
            case TEXT('к'): First = TEXT('К'); break;
            case TEXT('м'): First = TEXT('М'); break;
            case TEXT('н'): First = TEXT('Н'); break;
            case TEXT('п'): First = TEXT('П'); break;
            case TEXT('с'): First = TEXT('С'); break;
            case TEXT('х'): First = TEXT('Х'); break;
            default: break; // уже заглавная или буква вне словаря — не трогаем
        }
        return Result;
    }

    FString Combine(const FCaseForms& Epithet, const FPotionNoun& Noun, EGrammaticalCase Case)
    {
        return Capitalize(FString::Printf(TEXT("%s %s"), Epithet.Get(Case), Noun.Forms.Get(Case)));
    }

    // Доминирующая ось Direction — та же логика выбора, что раньше жила в
    // старой GeneratePotionName (наибольшее из четырёх значений).
    enum class EDominantAxis : uint8 { Body, Mind, Spirit, Nature };

    EDominantAxis GetDominantAxis(const FDirection& Dir)
    {
        EDominantAxis Best = EDominantAxis::Body;
        float BestValue = Dir.Body;
        if (Dir.Mind > BestValue) { Best = EDominantAxis::Mind; BestValue = Dir.Mind; }
        if (Dir.Spirit > BestValue) { Best = EDominantAxis::Spirit; BestValue = Dir.Spirit; }
        if (Dir.Nature > BestValue) { Best = EDominantAxis::Nature; }
        return Best;
    }

    const FPotionNoun& GetValidNoun(EDominantAxis Axis)
    {
        switch (Axis)
        {
            case EDominantAxis::Mind:   return NounDurman;
            case EDominantAxis::Spirit: return NounVoda;
            case EDominantAxis::Nature: return NounNastoy;
            default:                     return NounVzvar; // Body
        }
    }

    const FPotionEpithet& GetValidEpithet(const FMeta& M, float Magnitude)
    {
        // Приоритет: порча и морок — самые "громкие" по нарративу сигналы,
        // проверяются первыми; светлое/крепкое/хмельное — только если ничего
        // тревожного не перевесило. Пороги те же 0.6-0.65, что уже задают
        // пороги гистерезиса/манифестации в остальном проекте (не новая шкала).
        if (M.Corruption > 0.6f) return EpithetPoganyi;
        if (M.Distortion > 0.6f) return EpithetSmutnyi;
        if (M.Purity > 0.6f && M.Stability > 0.5f) return EpithetSvetlyi;
        if (M.Stability > 0.65f) return EpithetKrepkiy;
        if (M.Potency > 0.65f) return EpithetKhmelnoy;

        // Ни одна ось качества не выделилась -- но зелье может быть честно
        // МОЩНЫМ (Magnitude), а не просто невыразительным. Порог 0.45 — не
        // с потолка: диагностика 2026-09-04 (см. CHANGELOG.md) прогнала
        // ComputeApplyResult на полном переборе пар и репрезентативной
        // выборке троек реальных ингредиентов DT_IngredientClass — p90
        // достижимого Magnitude по всей выборке составил 0.442, p95 — 0.482
        // (максимум во всей выборке — 0.612, дальше формула физически не
        // поднимает мощь при 2-3 ингредиентах). 0.45 отсекает примерно
        // верхние ~10% реалистичных варок, тот же принцип калибровки "по
        // верхнему хвосту распределения", каким изначально подбирались и
        // 0.6/0.65 для качественных осей.
        if (Magnitude > 0.45f) return EpithetMogutnyi;

        return EpithetKhilyi;
    }
}

FText GeneratePotionName(EAlchemyOutcome Outcome, const FRealState& State, EGrammaticalCase Case)
{
    // Ash/BoiledWater хранятся отдельными IngredientID (см. GetItemDisplayName
    // ниже) и до сюда обычно не доходят — но склонение и для них должно
    // работать честно, если когда-нибудь понадобится напрямую.
    if (Outcome == EAlchemyOutcome::Ash)
    {
        return FText::FromString(Capitalize(NounZola.Forms.Get(Case)));
    }
    if (Outcome == EAlchemyOutcome::BoiledWater)
    {
        return FText::FromString(Combine(EpithetKipyachyonaya, NounVoda, Case));
    }

    const EDominantAxis Axis = GetDominantAxis(State.Direction);

    if (Outcome == EAlchemyOutcome::Purified)
    {
        // Body/Nature ("земное", осязаемое) -> непочатая; Mind/Spirit
        // ("незримое") -> заревая. Тот же дуализм осей, что уже разводит
        // основы Valid-варки выше, просто на двух особых эпитетах, не пяти.
        const FCaseForms& Epithet = (Axis == EDominantAxis::Body || Axis == EDominantAxis::Nature)
            ? EpithetNepochataya : EpithetZarevaya;
        return FText::FromString(Combine(Epithet, NounVoda, Case));
    }

    if (Outcome == EAlchemyOutcome::Catastrophe)
    {
        const FCaseForms& Epithet = (Axis == EDominantAxis::Body || Axis == EDominantAxis::Nature)
            ? EpithetMorokovoe : EpithetNavye;
        return FText::FromString(Combine(Epithet, NounVarevo, Case));
    }

    // Valid (в т.ч. любые ещё не заведённые в будущем исходы — безопасный
    // дефолт, не крах).
    const FPotionNoun& Noun = GetValidNoun(Axis);
    const FPotionEpithet& Epithet = GetValidEpithet(State.Meta, State.Magnitude);
    return FText::FromString(Combine(Epithet.ForGender(Noun.Gender), Noun, Case));
}

FString GetItemDisplayName(const FInventoryItem& Item, UIngredientRegistrySubsystem* Registry, EGrammaticalCase Case)
{
    if (Item.IngredientID == FName(TEXT("Potion")))
    {
        return GeneratePotionName(Item.BrewOutcome, Item.State, Case).ToString();
    }
    if (Item.IngredientID == FName(TEXT("Ash")))
    {
        return GeneratePotionName(EAlchemyOutcome::Ash, Item.State, Case).ToString();
    }
    if (Item.IngredientID == FName(TEXT("BoiledWater")))
    {
        return GeneratePotionName(EAlchemyOutcome::BoiledWater, Item.State, Case).ToString();
    }
    if (Item.IngredientID == FName(TEXT("Water")))
    {
        return TEXT("Вода");
    }
    if (Registry)
    {
        if (const FIngredientTableRow* Row = Registry->GetRow(Item.IngredientID))
        {
            return Row->DisplayName.ToString();
        }
    }
    return Item.IngredientID.ToString();
}
