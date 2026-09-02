// MemoryFragmentsCreateCommandlet.cpp
#include "Commandlets/MemoryFragmentsCreateCommandlet.h"
#include "Core/Zaryana/MemoryFragmentTypes.h"
#include "Engine/DataTable.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "Misc/PackageName.h"
#include "AssetRegistry/AssetRegistryModule.h"

namespace
{
    // Построчная транскрипция прежнего литерального массива
    // MemoryFragmentDefinitions.h::GetAllMemoryFragmentDefinitions()
    // (10 карточек) -- тексты и значения не менялись, только способ хранения.
    TArray<FMemoryFragmentDefinition> BuildMemoryFragmentRows()
    {
        TArray<FMemoryFragmentDefinition> Out;
        int32 Order = 0;

        FMemoryFragmentDefinition TikhoeMesto;
        TikhoeMesto.ID = FName(TEXT("TIKHOE_MESTO"));
        TikhoeMesto.SortOrder = Order++;
        TikhoeMesto.Trigger = EMemoryFragmentTrigger::LowLocalDistortion;
        TikhoeMesto.TrueText = FText::FromString(TEXT(
            "Здесь было так же тихо, как на болоте у брода — мох не скрипел, вода в бочажке не хмурилась. "
            "Аграфена говорила: там, где Навь не дышит в затылок, слышно, как растёт трава. "
            "Я тогда не понимала её. Теперь — да."));
        TikhoeMesto.FalseText = FText::FromString(TEXT(
            "Здесь было так же тихо, как на болоте у брода... только Аграфена стояла рядом и молчала — "
            "а она умерла задолго до того, как я впервые пришла на болото."));
        TikhoeMesto.ClarityGain = 0.05f;
        Out.Add(TikhoeMesto);

        FMemoryFragmentDefinition PervayaVarka;
        PervayaVarka.ID = FName(TEXT("PERVAYA_VARKA"));
        PervayaVarka.SortOrder = Order++;
        PervayaVarka.Trigger = EMemoryFragmentTrigger::CoherentBrew;
        PervayaVarka.TrueText = FText::FromString(TEXT(
            "Руки помнят лучше головы. Аграфена не учила словами — брала мои пальцы в свои и вела ложку по кругу, "
            "пока варево не переставало противиться. \"Не мешай — веди\", — говорила она. "
            "Зелье, что не сопротивляется руке, не солжёт потом языку."));
        PervayaVarka.FalseText = FText::FromString(TEXT(
            "Руки помнят лучше головы. Аграфена стояла за спиной и хвалила каждое движение, ни разу не поправив — "
            "а ведь она никогда никого не хвалила, пока варево само не докажет себя."));
        PervayaVarka.ClarityGain = 0.05f;
        Out.Add(PervayaVarka);

        FMemoryFragmentDefinition Podnoshenie;
        Podnoshenie.ID = FName(TEXT("PODNOSHENIE"));
        Podnoshenie.SortOrder = Order++;
        Podnoshenie.Trigger = EMemoryFragmentTrigger::ShrineRestored;
        Podnoshenie.TrueText = FText::FromString(TEXT(
            "Мать оставляла у капища не еду — нитку от своей рубахи. \"Дар должен быть от тебя, не от печи\", — говорила она. "
            "Я не понимала, зачем отдавать вещь, которую после не вернуть. "
            "Теперь понимаю: капище помнит не то, что ты дал, а то, чего тебе будет не хватать."));
        Podnoshenie.FalseText = FText::FromString(TEXT(
            "Мать оставляла у капища еду — свежий хлеб, каждое полнолуние, без пропуска ни разу за всю жизнь."));
        Podnoshenie.ClarityGain = 0.05f;
        Out.Add(Podnoshenie);

        FMemoryFragmentDefinition KhlebSol;
        KhlebSol.ID = FName(TEXT("KHLEB_SOL"));
        KhlebSol.SortOrder = Order++;
        KhlebSol.Trigger = EMemoryFragmentTrigger::HighCommunityTrust;
        KhlebSol.TrueText = FText::FromString(TEXT(
            "Первый раз мне оставили не крапиву и не медяк -- каравай, ещё тёплый, на пороге, пока я спала. "
            "Никто не постучал. Аграфена, узнав, только кивнула: \"Значит, поверили -- не испугались, не "
            "разболтали, ждали, что не похвалишь себя за это\". Я тогда не поняла: разве доверие -- это "
            "когда тебя боятся достаточно, чтобы молчать?"));
        KhlebSol.FalseText = FText::FromString(TEXT(
            "Первый раз мне оставили не крапиву и не медяк -- каравай, ещё тёплый, у порога. Хозяйка сама "
            "дождалась, пока я выйду, и сказала у порога вслух: \"Спасибо тебе, дай Бог здоровья\" -- и я "
            "запомнила её лицо, полное благодарности."));
        KhlebSol.ClarityGain = 0.05f;
        Out.Add(KhlebSol);

        FMemoryFragmentDefinition BuyanGuardian;
        BuyanGuardian.ID = FName(TEXT("BUYAN_GUARDIAN"));
        BuyanGuardian.SortOrder = Order++;
        BuyanGuardian.Trigger = EMemoryFragmentTrigger::BuyanPathChosen;
        BuyanGuardian.TrueText = FText::FromString(TEXT(
            "Я проснулась в Яви -- не помню как. У порога стояла вода, свежая, будто кто-то принёс её, пока "
            "меня не было, и ушёл, не разбудив. Теперь я хожу мимо старого дуба на опушке и чувствую: там "
            "кто-то есть -- тихий, не злой, только имени у него нет. Люди при мне о нём молчат или пугаются, "
            "как пугались бы любого другого хозяина места. Может, так теперь будет всегда: то, что меня "
            "спасло, останется без лица -- жертва, о которой я никогда не узнаю и не смогу отблагодарить."));
        BuyanGuardian.ClarityGain = 0.05f;
        Out.Add(BuyanGuardian);

        FMemoryFragmentDefinition BuyanTradePlaces;
        BuyanTradePlaces.ID = FName(TEXT("BUYAN_TRADE_PLACES"));
        BuyanTradePlaces.SortOrder = Order++;
        BuyanTradePlaces.Trigger = EMemoryFragmentTrigger::BuyanPathChosen;
        BuyanTradePlaces.TrueText = FText::FromString(TEXT(
            "Заговор был сложен для меня, не для него -- он всё равно прочёл его над собой, стоя рядом. Помню "
            "только край: голос, который должен был вести меня назад, к телу, а повёл нас двоих сразу, в "
            "разные стороны одного и того же шага. Я проснулась -- не вся. Что-то моё осталось там, где он "
            "теперь застрял наполовину: не так глубоко, как была я, но и не в Яви целиком. Ни один из нас не "
            "получил чистого разрешения. Просто цена, которую раньше платила бы я одна, теперь поделена на "
            "двоих -- не легче, только иначе тяжело."));
        BuyanTradePlaces.ClarityGain = 0.05f;
        Out.Add(BuyanTradePlaces);

        FMemoryFragmentDefinition BuyanAcceptReality;
        BuyanAcceptReality.ID = FName(TEXT("BUYAN_ACCEPT_REALITY"));
        BuyanAcceptReality.SortOrder = Order++;
        BuyanAcceptReality.Trigger = EMemoryFragmentTrigger::BuyanPathChosen;
        BuyanAcceptReality.TrueText = FText::FromString(TEXT(
            "...я слышу шаги, но не подхожу ближе. Кто-то стоит у Алатыря и не читает надо мной заговор -- не "
            "пробует обмануть, не остаётся стражем. Просто стоит. Я не знаю, кто это, я почти не знаю, кто я "
            "сама -- эта мысль не досказана, как и всё здесь, на границе. Может, так и должно быть: не каждая "
            "цена искупается, не каждая потеря становится смыслом. Пусть эта останется просто горем, без "
            "утешительного слоя магии, -- и пусть тот, кто стоит там, живёт с этим дальше, не притворяясь, "
            "что нашёл ответ."));
        BuyanAcceptReality.ClarityGain = 0.05f;
        Out.Add(BuyanAcceptReality);

        FMemoryFragmentDefinition TishinaLesa;
        TishinaLesa.ID = FName(TEXT("TISHINA_LESA"));
        TishinaLesa.SortOrder = Order++;
        TishinaLesa.Trigger = EMemoryFragmentTrigger::LowLocalDistortion;
        TishinaLesa.TrueText = FText::FromString(TEXT(
            "Аграфена долго не давала мне резать кору. Заставляла сперва час стоять и слушать -- не говорить "
            "со мной, просто ждать, пока сама не услышу, где дерево дышит ровно, а где нет. Я злилась -- "
            "думала, тянет время. Она сказала только: \"Разрежешь не там -- не узнаешь, что не там разрезала. "
            "Дерево не переспросит\". Индрик слушает так же -- рогом, не глазами. Я тогда не поняла, при чём "
            "тут зверь, которого никто не видел. Теперь понимаю."));
        TishinaLesa.FalseText = FText::FromString(TEXT(
            "Аграфена долго не давала мне резать кору. Взяла нож сама, показала, где резать -- три быстрых "
            "движения, готово. \"Вот так. Запоминай\". Я запомнила с первого раза."));
        TishinaLesa.ClarityGain = 0.05f;
        Out.Add(TishinaLesa);

        FMemoryFragmentDefinition OjidanieBuri;
        OjidanieBuri.ID = FName(TEXT("OJIDANIE_BURI"));
        OjidanieBuri.SortOrder = Order++;
        OjidanieBuri.Trigger = EMemoryFragmentTrigger::HighStability;
        OjidanieBuri.TrueText = FText::FromString(TEXT(
            "Пурга шла на нас два дня, и Аграфена не спешила в укрытие -- сидела у входа в землянку, смотрела "
            "на снег и молчала. Я просила уйти глубже. Она сказала: \"Кто бежит от бури первым, тот и решение "
            "примет первым -- и торопливое\". Мы вышли, только когда снег сам лёг ровно. Я тогда думала -- она "
            "просто не боится холода. Теперь думаю -- она ждала не бурю. Ждала, пока во мне уляжется паника."));
        OjidanieBuri.FalseText = FText::FromString(TEXT(
            "Пурга шла на нас два дня, и Аграфена сразу увела нас глубже в землянку, укрыла заранее -- она "
            "всегда знала наперёд, что будет с погодой, и мы ничего не боялись."));
        OjidanieBuri.ClarityGain = 0.05f;
        Out.Add(OjidanieBuri);

        FMemoryFragmentDefinition NePokhvalila;
        NePokhvalila.ID = FName(TEXT("NE_POKHVALILA"));
        NePokhvalila.SortOrder = Order++;
        NePokhvalila.Trigger = EMemoryFragmentTrigger::LegendaryZoneResolved;
        NePokhvalila.TrueText = FText::FromString(TEXT(
            "Я в первый раз сама, без неё, прошла мимо места, где чуяла что-то нехорошее -- не побежала, не "
            "свернула, просто прошла ровным шагом, как она учила. Вернулась гордая, ждала слова. Аграфена "
            "посмотрела на меня, кивнула один раз -- и спросила, что было на ужин. Ни слова о пройденном. Я "
            "обиделась тогда. Теперь понимаю -- не потому что не заметила. Потому что заметила и сочла "
            "достаточным просто кивнуть."));
        NePokhvalila.FalseText = FText::FromString(TEXT(
            "Я в первый раз сама, без неё, прошла мимо места, где чуяла что-то нехорошее. Вернулась, и "
            "Аграфена обняла меня, сказала, что гордится, что я справилась не хуже неё самой."));
        NePokhvalila.ClarityGain = 0.05f;
        Out.Add(NePokhvalila);

        FMemoryFragmentDefinition NeudobnayaPravda;
        NeudobnayaPravda.ID = FName(TEXT("NEUDOBNAYA_PRAVDA"));
        NeudobnayaPravda.SortOrder = Order++;
        NeudobnayaPravda.Trigger = EMemoryFragmentTrigger::ShrineRestored;
        NeudobnayaPravda.TrueText = FText::FromString(TEXT(
            "Я принесла ей отвар, которым гордилась -- три дня варила, всё по науке. Она попробовала, "
            "поморщилась и сказала прямо: \"Горький и бесполезный. Ты боялась ошибиться и потому не "
            "рискнула\". Я плакала в тот вечер. Она не пришла утешать. Только на следующий день сказала: "
            "\"Правда не становится добрее, если её смягчить. Она просто перестаёт быть правдой.\""));
        NeudobnayaPravda.FalseText = FText::FromString(TEXT(
            "Я принесла ей отвар, которым гордилась. Она попробовала, сказала, что неплохо для первого раза, "
            "и что дальше будет только лучше -- не хотела меня расстраивать перед сложным годом."));
        NeudobnayaPravda.ClarityGain = 0.05f;
        Out.Add(NeudobnayaPravda);

        FMemoryFragmentDefinition NitMateri;
        NitMateri.ID = FName(TEXT("NIT_MATERI"));
        NitMateri.SortOrder = Order++;
        NitMateri.Trigger = EMemoryFragmentTrigger::YarnBallAcquired;
        NitMateri.TrueText = FText::FromString(TEXT(
            "В степи всё тянется до горизонта одинаково, и я боялась заблудиться. Мать -- не Аграфена, мать, "
            "ещё до неё -- учила меня не смотреть под ноги, а держать в голове одну далёкую точку и идти на "
            "неё, не сверяясь ежеминутно. \"Земля не обманет, если не бежать от неё глазами\". Я тогда была "
            "совсем маленькой и не запомнила её лица толком. Только это -- не смотреть под ноги."));
        NitMateri.FalseText = FText::FromString(TEXT(
            "В степи всё тянется до горизонта одинаково, и я боялась заблудиться. Мать дала мне клубок "
            "цветной пряжи -- привязывала конец к колышку у дома, и я разматывала его, идя вперёд, чтобы "
            "найти дорогу назад по нитке."));
        NitMateri.ClarityGain = 0.05f;
        Out.Add(NitMateri);

        return Out;
    }
}

int32 UMemoryFragmentsCreateCommandlet::Main(const FString& Params)
{
    const TCHAR* AssetPath = TEXT("/Game/Herbalist/Data/DT_MemoryFragments");

    if (UDataTable* Existing = LoadObject<UDataTable>(nullptr, AssetPath))
    {
        UE_LOG(LogTemp, Display, TEXT("MemoryFragmentsCreate: %s уже существует (%d рядов), ничего не делаю"),
            AssetPath, Existing->GetRowMap().Num());
        return 0;
    }

    UPackage* Package = CreatePackage(AssetPath);
    if (!Package)
    {
        UE_LOG(LogTemp, Error, TEXT("MemoryFragmentsCreate: не удалось создать пакет %s"), AssetPath);
        return 1;
    }

    UDataTable* Table = NewObject<UDataTable>(Package, FName(TEXT("DT_MemoryFragments")), RF_Public | RF_Standalone);
    if (!Table)
    {
        UE_LOG(LogTemp, Error, TEXT("MemoryFragmentsCreate: не удалось создать UDataTable"));
        return 1;
    }
    Table->RowStruct = FMemoryFragmentDefinition::StaticStruct();

    FAssetRegistryModule::AssetCreated(Table);

    int32 AddedCount = 0;
    for (const FMemoryFragmentDefinition& Row : BuildMemoryFragmentRows())
    {
        Table->AddRow(Row.ID, Row);
        ++AddedCount;
    }

    Table->MarkPackageDirty();

    const FString PackageFileName = FPackageName::LongPackageNameToFilename(
        Package->GetName(), FPackageName::GetAssetPackageExtension());

    FSavePackageArgs SaveArgs;
    SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
    SaveArgs.SaveFlags = SAVE_NoError;

    const bool bSuccess = UPackage::SavePackage(Package, Table, *PackageFileName, SaveArgs);
    if (!bSuccess)
    {
        UE_LOG(LogTemp, Error, TEXT("MemoryFragmentsCreate: не удалось сохранить пакет %s"), *PackageFileName);
        return 1;
    }

    UE_LOG(LogTemp, Display, TEXT("MemoryFragmentsCreate: создан %s, %d рядов добавлено"), AssetPath, AddedCount);
    return 0;
}
