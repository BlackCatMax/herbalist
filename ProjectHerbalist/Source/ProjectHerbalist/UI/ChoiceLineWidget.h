// ChoiceLineWidget.h
//
// Субтитр и строка выбора (DESIGN_Diegetic_Interface.md, этап 5; решения
// пользователя 2026-09-21): реплика хозяина места -- строкой внизу экрана,
// ответы -- под ней, выделенный отмечен; колесо мыши двигает выделение,
// взаимодействие выбирает. Одна из немногих вещей, что остаются экраном.
// Ввода не забирает и паузы не ставит -- время идёт.
//
// Строит себя в C++, как строка ощущения, никакого WBP.
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ChoiceLineWidget.generated.h"

class UTextBlock;
class UVerticalBox;

UCLASS()
class PROJECTHERBALIST_API UChoiceLineWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    void SetContent(const FString& Prompt, const TArray<FString>& Options, int32 Selected);

protected:
    virtual void NativeConstruct() override;

private:
    void Refresh();

    UPROPERTY()
    TObjectPtr<UTextBlock> PromptText = nullptr;

    UPROPERTY()
    TObjectPtr<UVerticalBox> OptionBox = nullptr;

    FString PendingPrompt;
    TArray<FString> PendingOptions;
    int32 PendingSelected = 0;
};
