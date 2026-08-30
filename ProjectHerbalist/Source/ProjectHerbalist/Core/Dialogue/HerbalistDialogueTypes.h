// Core/Dialogue/HerbalistDialogueTypes.h
// (не DialogueTypes.h — конфликтует с Engine/Source/Runtime/Engine/Classes/
// Sound/DialogueTypes.h, UHT не различает заголовки в разных папках по
// одному имени файла)
//
// Диалоги (DESIGN_Community_And_Homestead.md §1.1, реализация 2026-08-31) —
// "одна и та же система диалога обслуживает и людей, и хозяев места... не
// два канала, а одно дерево-движок с разными наборами веток на разных
// адресатов". Дерево не хранит собственное состояние (§1.1: "не новый
// накопитель") — читает уже существующий Respect (хозяева места,
// LandmarkTypes.h) или Molva (люди, AGridWorldManager::Molva) и статический
// реестр ниже, тем же паттерном, что GetLandmarkDefinitions()/
// GetRitualRecipeDefinitions().
//
// Герой немой (§0, решено 2026-08-30): узел несёт реплику ХОЗЯИНА/NPC, а не
// героя — ветки описывают, что делает/выбирает игрок в третьем лице
// (действие, не прямая речь), тот же регистр письма, что уже принят для
// общины в целом (17_Hero_And_Community.md §17.2, "никто не говорит ему
// спасибо... о нём говорят в третьем лице").
//
// "Разговор с людьми" (сельчанами) — в реестре ниже НЕТ ни одного примера с
// живым человеком: в проекте вообще не существует физических NPC-акторов
// (только AHerbalistEntityActor/ALandmarkEntityActor для хозяев места) —
// честно отложено, не забыто. Дерево ниже полностью готово принять человека
// в реестр, как только появится сам актор, которому дать реплику.
#pragma once

#include "CoreMinimal.h"
#include "HerbalistDialogueTypes.generated.h"

USTRUCT()
struct FDialogueBranch
{
    GENERATED_BODY()

    // Что игрок делает/выбирает, от третьего лица ("кивнуть", "оставить
    // подношение у порога") — не прямая речь Героя, см. комментарий у файла.
    UPROPERTY() FText ActionText;

    // Ветка доступна, только если MinGate <= текущий Respect/Molva <= MaxGate
    // (диапазон включительный). Дефолт [-1, 1] значит "всегда доступна".
    UPROPERTY() float MinGate = -1.0f;
    UPROPERTY() float MaxGate = 1.0f;

    // NAME_None = ветка заканчивает разговор.
    UPROPERTY() FName NextNodeID = NAME_None;
};

USTRUCT()
struct FDialogueNode
{
    GENERATED_BODY()

    UPROPERTY() FName NodeID;

    // Реплика хозяина/NPC на этом узле — то, что видит игрок, попав сюда.
    UPROPERTY() FText SpeakerLine;

    UPROPERTY() TArray<FDialogueBranch> Branches;
};

USTRUCT()
struct FDialogueDefinition
{
    GENERATED_BODY()

    // Обычно совпадает с EntityID адресата (FLandmarkDefinition::EntityID)
    // — тот же ключ, каким Landmark уже идентифицируется, не новое
    // сопоставление.
    UPROPERTY() FName DialogueID;
    UPROPERTY() FName StartNodeID;
    UPROPERTY() TArray<FDialogueNode> Nodes;
};

inline const FDialogueNode* FindDialogueNode(const FDialogueDefinition& Def, FName NodeID)
{
    for (const FDialogueNode& Node : Def.Nodes)
    {
        if (Node.NodeID == NodeID) return &Node;
    }
    return nullptr;
}

// Ветки узла, доступные при текущем Respect/Molva — фильтрация по гейту, не
// хранение: тот же принцип "дерево не копит состояние", что уже
// зафиксирован в §1.1 документа.
inline TArray<const FDialogueBranch*> GetAvailableBranches(const FDialogueNode& Node, float RespectOrMolva)
{
    TArray<const FDialogueBranch*> Available;
    for (const FDialogueBranch& Branch : Node.Branches)
    {
        if (RespectOrMolva >= Branch.MinGate && RespectOrMolva <= Branch.MaxGate)
        {
            Available.Add(&Branch);
        }
    }
    return Available;
}

inline const TArray<FDialogueDefinition>& GetDialogueDefinitions()
{
    static const TArray<FDialogueDefinition> Definitions = []()
    {
        TArray<FDialogueDefinition> Defs;

        // Домовой — первый и пока единственный пример (тот же принцип
        // вертикального среза, что реестр ритуалов/бестиарий: один честный
        // случай, не весь охват сразу). Три ветки узла "Home" ровно
        // отражают три диапазона Respect, которые уже реально что-то меняют
        // в мире (LandmarkTypes.h: bless >= 0.5, curse < -0.3, отягощённое
        // проклятие < -0.6) — дерево читает те же числа, не изобретает свои.
        {
            FDialogueDefinition D;
            D.DialogueID = FName(TEXT("Домовой"));
            D.StartNodeID = FName(TEXT("Home"));

            FDialogueNode Home;
            Home.NodeID = FName(TEXT("Home"));
            Home.SpeakerLine = FText::FromString(TEXT("Домовой молчит, но чувствуется его взгляд из-за печи."));

            FDialogueBranch Offer;
            Offer.ActionText = FText::FromString(TEXT("Оставить у печи блюдце молока"));
            Offer.MinGate = -1.0f; Offer.MaxGate = 1.0f;
            Offer.NextNodeID = NAME_None;
            Home.Branches.Add(Offer);

            FDialogueBranch Good;
            Good.ActionText = FText::FromString(TEXT("Прислушаться — как он расположен к дому?"));
            Good.MinGate = 0.5f; Good.MaxGate = 1.0f;
            Good.NextNodeID = FName(TEXT("GoodStanding"));
            Home.Branches.Add(Good);

            FDialogueBranch Bad;
            Bad.ActionText = FText::FromString(TEXT("Заметить неладное в углу"));
            Bad.MinGate = -1.0f; Bad.MaxGate = -0.3f;
            Bad.NextNodeID = FName(TEXT("BadStanding"));
            Home.Branches.Add(Bad);

            D.Nodes.Add(Home);

            FDialogueNode Good2;
            Good2.NodeID = FName(TEXT("GoodStanding"));
            Good2.SpeakerLine = FText::FromString(TEXT("Дом тих и ладен — Домовой оберегает его от порчи."));
            D.Nodes.Add(Good2);

            FDialogueNode Bad2;
            Bad2.NodeID = FName(TEXT("BadStanding"));
            Bad2.SpeakerLine = FText::FromString(TEXT("Пряжа спутана, миска молока опрокинута — Домовой недоволен домом."));
            D.Nodes.Add(Bad2);

            // Отягощённая ветка (Respect < -0.6) — совпадает с диапазоном
            // BadStanding (-1..-0.3), намеренно: дерево не обязано различать
            // curse/aggravated curse отдельным узлом, сама реплика BadStanding
            // остаётся верной на всём диапазоне "недоволен". Более резкая,
            // отдельная реплика для эскалации в домашнюю Кикимору — контент,
            // который можно дописать позже, не архитектурное ограничение.

            Defs.Add(D);
        }

        return Defs;
    }();
    return Definitions;
}

inline const FDialogueDefinition* FindDialogueDefinition(FName DialogueID)
{
    for (const FDialogueDefinition& D : GetDialogueDefinitions())
    {
        if (D.DialogueID == DialogueID) return &D;
    }
    return nullptr;
}
