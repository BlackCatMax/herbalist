// SensationLineWidget.h
//
// Одна строка внизу экрана -- ощущение от предмета в руке при осмотре
// (DESIGN_Diegetic_Interface.md). Одна из немногих вещей, что остаются
// экраном: строка ощущения, субтитры речи и Травник. Не блокирует ввод и не
// ставит паузу -- время идёт (решение 2026-09-21).
//
// Строит себя в C++, как Травник, никакого WBP.
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SensationLineWidget.generated.h"

class UTextBlock;

UCLASS()
class PROJECTHERBALIST_API USensationLineWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    void SetLine(const FString& Line);

protected:
    virtual void NativeConstruct() override;

private:
    UPROPERTY()
    TObjectPtr<UTextBlock> LineText = nullptr;

    FString PendingLine;
};
