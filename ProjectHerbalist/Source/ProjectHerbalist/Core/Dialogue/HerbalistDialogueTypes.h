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
//
// 2026-09-02, Unit 5/6 миграции контента проекта на DataTable — тот же
// паттерн, что уже FArtifactDefinition и весь бестиарий:
// GetDialogueDefinitions() ниже лениво грузит /Game/Herbalist/Data/DT_Dialogue.
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
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

    // Символическое подношение (2026-09-06, "оставить у печи блюдце
    // молока" — прямой запрос пользователя: "бесплатный символический акт
    // для поддержания репутации"). В отличие от обычного подношения
    // (Apply зелья/предмета на клетку-обиталище, читает Purity−Corruption
    // предмета) — здесь нет предмета вовсе, только сам выбор ветки диалога.
    // Выбор такой ветки в AHerbalistPlayerController::ChooseDialogueBranch
    // добавляет фиксированный UHerbalistSettings::SymbolicOfferingRespectGain
    // к Respect собеседника напрямую — не второй, конкурирующий канал
    // изменения Respect, а тот же принцип "Respect меняется только через
    // подношение" (решение 2026-08-29), просто подношение без материального
    // компонента. Дефолт false — обычные информационные ветки (например,
    // "прислушаться, как он расположен") не должны молча тоже считаться
    // подношением.
    UPROPERTY() bool bIsSymbolicOffering = false;

    // Калинов мост / Трёхглавый Змей (DESIGN_Brewing_Situations_And_Lore.md
    // §4.4, 2026-09-06, решение пользователя: "диалоговый выбор" вместо
    // боевой системы, которой в проекте нет). Тот же класс спец-флага, что
    // bIsSymbolicOffering выше -- одна ветка диалога несёт эффект за
    // пределами обычного перехода по узлам. Выбор этой ветки в
    // AHerbalistPlayerController::ChooseDialogueBranch бьёт по Purity/
    // Stability клетки самого Змея (UHerbalistSettings::KalinovMostFightCost)
    // -- риск боя, а не победа даром. "Сделка" (другая ветка того же узла)
    // сознательно НЕ резолвит предмет — честный пробел, тот же класс, что
    // уже у ActivateWard/TradeWithCommunity (IngredientRegistrySubsystem
    // недоступен из ChooseDialogueBranch), см. ROADMAP.md.
    UPROPERTY() bool bIsKalinovMostFight = false;

    // Сделка (§4.4, DESIGN_POI_Art_And_LevelDesign.md "открытые вопросы —
    // решения", 2026-09-06) -- "Змей берёт... один из добытых даров, по
    // выбору самого странника" (карточка компендиума). Выбор ЭТОЙ ветки не
    // выбирает предмет сам (дерево диалога статично, не может перечислить
    // артефакты текущего игрока построчно) -- только "вооружает" сделку
    // (AGridWorldManager::ArmKalinovMostDeal), реальную жертву завершает
    // отдельная команда (AHerbalistPlayerController::PayKalinovMostToll),
    // тем же классом разделения, что уже у ActivateWard/TradeWithCommunity
    // (резолв инвентаря — не забота диалогового дерева).
    UPROPERTY() bool bIsKalinovMostDeal = false;
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

USTRUCT(BlueprintType)
struct PROJECTHERBALIST_API FDialogueDefinition : public FTableRowBase
{
    GENERATED_BODY()

    // Обычно совпадает с EntityID адресата (FLandmarkDefinition::EntityID)
    // — тот же ключ, каким Landmark уже идентифицируется, не новое
    // сопоставление.
    UPROPERTY() FName DialogueID;

    // Явный порядок регистрации — тот же приём, что у остальных
    // мигрированных реестров (см. FAmbientEntityDefinition::SortOrder).
    UPROPERTY() int32 SortOrder = 0;

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

// Ленивая загрузка из /Game/Herbalist/Data/DT_Dialogue (2026-09-02) — тот
// же паттерн, что GetArtifactDefinitions() и др., см. подробное
// обоснование в AmbientEntityTypes.h. LogTemp, не HerbalistLogChannels.h
// категория — та же причина (LNK2001): inline-функция компилируется и в
// ProjectHerbalistTests через новый коммандлет.
inline const TArray<FDialogueDefinition>& GetDialogueDefinitions()
{
    static const TArray<FDialogueDefinition> Definitions = []()
    {
        check(IsInGameThread());   // LoadObject не потокобезопасен

        TArray<FDialogueDefinition> Defs;
        UDataTable* Table = LoadObject<UDataTable>(nullptr, TEXT("/Game/Herbalist/Data/DT_Dialogue"));
        if (!Table)
        {
            UE_LOG(LogTemp, Error, TEXT("GetDialogueDefinitions: не удалось загрузить DT_Dialogue -- реестр диалогов будет пуст"));
            return Defs;
        }
        Table->AddToRoot();

        TArray<FDialogueDefinition*> Rows;
        Table->GetAllRows(TEXT("GetDialogueDefinitions"), Rows);
        Defs.Reserve(Rows.Num());
        for (const FDialogueDefinition* Row : Rows)
        {
            if (Row) Defs.Add(*Row);
        }
        Defs.Sort([](const FDialogueDefinition& A, const FDialogueDefinition& B) { return A.SortOrder < B.SortOrder; });
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
