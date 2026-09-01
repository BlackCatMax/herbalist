// MemoryFragmentDefinitions.h
#pragma once

#include "CoreMinimal.h"
#include "Core/Zaryana/MemoryFragmentTypes.h"

// Три подлинных воспоминания (по одному на триггер, см. MemoryFragmentTypes.h) —
// хардкод, не DataAsset-конвейер: v1, тот же принцип вертикального среза, что
// и у бестиария/капищ. Тексты — не заимствованы, написаны в тон уже принятому
// в проекте фольклорному языку (см. компендиум, 17_Hero_And_Community.md —
// Аграфена как наставница). Ложные версии несут скрытый сбой — деталь,
// противоречащую уже установленному канону (кто где был, чей это был
// характер) — игрок должен заметить несостыковку сам, не через подсказку в UI.
namespace HerbalistCore::Zaryana
{
    inline const TArray<FMemoryFragmentDefinition>& GetAllMemoryFragmentDefinitions()
    {
        static const TArray<FMemoryFragmentDefinition> Definitions = []()
        {
            TArray<FMemoryFragmentDefinition> Out;

            FMemoryFragmentDefinition TikhoeMesto;
            TikhoeMesto.ID = FName(TEXT("TIKHOE_MESTO"));
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
            Podnoshenie.Trigger = EMemoryFragmentTrigger::ShrineRestored;
            Podnoshenie.TrueText = FText::FromString(TEXT(
                "Мать оставляла у капища не еду — нитку от своей рубахи. \"Дар должен быть от тебя, не от печи\", — говорила она. "
                "Я не понимала, зачем отдавать вещь, которую после не вернуть. "
                "Теперь понимаю: капище помнит не то, что ты дал, а то, чего тебе будет не хватать."));
            Podnoshenie.FalseText = FText::FromString(TEXT(
                "Мать оставляла у капища еду — свежий хлеб, каждое полнолуние, без пропуска ни разу за всю жизнь."));
            Podnoshenie.ClarityGain = 0.05f;
            Out.Add(Podnoshenie);

            // ХЛЕБ-СОЛЬ (17_Hero_And_Community.md §17.6, 2026-09-01) —
            // четвёртый триггер, HighCommunityTrust. Тексты — точная цитата
            // из главы, не перефразированы. Ложная версия несёт скрытый
            // сбой того же типа, что и у трёх остальных: слово "спасибо",
            // сказанное вслух — прямое нарушение поверья §17.2.1 ("за
            // лечение нельзя говорить спасибо"), которое игрок должен
            // заметить сам, без подсказки в UI.
            FMemoryFragmentDefinition KhlebSol;
            KhlebSol.ID = FName(TEXT("KHLEB_SOL"));
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

            // Три исхода у Буяна (18_Ending.md §18.1, 21_Journey_And_Artifacts.md
            // §21.1, 2026-09-01, ревизия "Ending and artifacts") — гарантированный
            // финальный фрагмент, три вариации, по одной на EBuyanPath. Триггер
            // BuyanPathChosen, доставляются напрямую из
            // AGridWorldManager::TryChooseBuyanPath (CollectMemoryFragment),
            // не через TrySpawnStateBasedFragment/случайный false-ролл — это
            // единственный, гарантированно получаемый в момент выбора момент,
            // не то, что можно наткнуться "не тем" вариантом. FalseText здесь
            // не используется вовсе (bIsFalse всегда false у прямой доставки) —
            // оставлен пустым, а не выдуман ради заполнения поля.
            //
            // §18.1 описывает все три пути прозой от третьего лица, не готовой
            // цитатой в формате уже реализованных четырёх фрагментов (все —
            // от первого лица, голос Заряны-рассказчицы) — адаптировано в тот
            // же голос и формат. Путь 3 (принятие) — структурная особенность:
            // по тексту главы Заряна в этом исходе не просыпается вовсе, значит
            // не может "вспоминать" случившееся привычным ретроспективным
            // образом четырёх других фрагментов — сохранён её голос, но как
            // незавершённая, пограничная мысль в момент самого события, не
            // как чистое воспоминание изнутри Яви (согласовано с пользователем
            // как разумное развитие в рамках уже одобренного "адаптируй в её
            // голос, тем же форматом").
            //
            // ClarityGain=0.05, не 0.1 из иллюстративной арифметики §21.1
            // ("9+1=10×0.1=1.0") — та же арифметика не сходится и с уже
            // четырьмя реализованными фрагментами (тоже 0.05 каждый), число
            // не трогаю по той же причине, что и в шаге 1 исходного плана:
            // отдельное решение по балансу, не мой звонок.

            FMemoryFragmentDefinition BuyanGuardian;
            BuyanGuardian.ID = FName(TEXT("BUYAN_GUARDIAN"));
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

            return Out;
        }();
        return Definitions;
    }

    inline const FMemoryFragmentDefinition* FindMemoryFragmentDefinition(FName ID)
    {
        for (const FMemoryFragmentDefinition& Def : GetAllMemoryFragmentDefinitions())
        {
            if (Def.ID == ID) return &Def;
        }
        return nullptr;
    }
}
