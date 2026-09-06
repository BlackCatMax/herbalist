// KalinovMostDealPatchCommandlet.h
//
// Точечная правка живого DT_Dialogue (2026-09-06, DESIGN_POI_Art_And_
// LevelDesign.md, "открытые вопросы — решения"): ветка "Откупиться
// подношением, пройти без боя" (ЗмейГорыныч, узел "Bridge", вторая ветка)
// до этой правки не имела игрового эффекта (FDialogueBranch::
// bIsKalinovMostDeal не существовало на момент постройки живой таблицы
// KalinovMostDialogueAppendCommandlet'ом). Тот же приём, что уже
// DomovoiMilkOfferingPatchCommandlet: KalinovMostDialogueAppendCommandlet
// сам идемпотентен только в сторону "ряд уже есть — ничего не делаю", не
// умеет патчить уже существующий ряд.
//
// Запуск: UnrealEditor-Cmd.exe <uproject> -run=KalinovMostDealPatch
#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "KalinovMostDealPatchCommandlet.generated.h"

UCLASS()
class UKalinovMostDealPatchCommandlet : public UCommandlet
{
    GENERATED_BODY()

public:
    virtual int32 Main(const FString& Params) override;
};
