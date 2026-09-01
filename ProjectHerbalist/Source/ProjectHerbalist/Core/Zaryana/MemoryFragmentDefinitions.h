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
