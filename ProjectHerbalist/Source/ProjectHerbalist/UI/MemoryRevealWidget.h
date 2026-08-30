// MemoryRevealWidget.h
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MemoryRevealWidget.generated.h"

class UTextBlock;

// Экранный попап для текста воспоминания Заряны и объявления Буяна
// (2026-08-29, "Прогрессия/Заряна" — закрывает разрыв "игрок собирает
// фрагмент/достигает Буяна, но никогда не видит сам текст, только UE_LOG",
// см. GridWorldManagerZaryana.cpp). Тот же приём, что уже устоялся для
// Травника (UI/JournalLogWidget.h) — дерево строится целиком в NativeConstruct
// через WidgetTree, ни одного BindWidget, готов без единого .uasset.
UCLASS()
class PROJECTHERBALIST_API UMemoryRevealWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    // Показывает текст на экране на DisplaySeconds секунд, затем сам себя
    // убирает с экрана (не уничтожает виджет — переиспользуется следующим
    // Show()). Повторный вызов, пока предыдущий попап ещё виден, просто
    // меняет текст и перезапускает таймер — не копит очередь виджетов друг
    // на друге (фрагменты редки, v1: не больше одного активного, см.
    // TrySpawnStateBasedFragment, так что перекрытие маловероятно, но
    // безопасно и в этом случае).
    void Show(const FText& Text, float DisplaySeconds);

protected:
    virtual void NativeConstruct() override;

private:
    UPROPERTY()
    UTextBlock* BodyText = nullptr;

    void BuildLayout();

    FTimerHandle DismissTimer;
};
