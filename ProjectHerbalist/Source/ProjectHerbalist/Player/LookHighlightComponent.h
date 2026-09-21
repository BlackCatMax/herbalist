// LookHighlightComponent.h
//
// Подсветка того, на что смотришь (решение пользователя 2026-09-21: «прицел
// -- только лёгкая подсветка того, на что смотришь»). Точки в центре экрана
// нет; вместо неё то, с чем можно что-то сделать, чуть проступает.
//
// C++ только отмечает цель: включает на её примитивах Custom Depth со
// своим значением трафарета (HighlightStencilValue). Как именно она
// проступает -- контур, лёгкое свечение -- решает материал пост-процесса,
// это работа в редакторе (как и пост-процесс Морока, `Morok01`).
//
// Подсвечивается только то, с чем можно взаимодействовать: IInteractable и
// собираемые ресурсы. Трава под ногами и стены -- нет.
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "LookHighlightComponent.generated.h"

UCLASS(ClassGroup = (Herbalist), meta = (BlueprintSpawnableComponent))
class PROJECTHERBALIST_API ULookHighlightComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    ULookHighlightComponent();

    // Значение трафарета Custom Depth, по которому материал пост-процесса
    // узнаёт подсвеченное. Одно на всю игру.
    static constexpr int32 HighlightStencilValue = 42;

    // Как далеко видно цель, см. Interact() -- та же дальность.
    UPROPERTY(EditAnywhere, Category = "Herbalist|Highlight")
    float ReachCm = 1000.0f;

    // Что подсвечено сейчас (для тестов и для этапа 3 -- «применить к цели
    // под взглядом»).
    AActor* GetFocusedActor() const { return FocusedActor.Get(); }

    // Сменить цель вручную: сбрасывает подсветку со старой и ставит на новую.
    // TickComponent зовёт его по трассе взгляда; тесты -- напрямую.
    void SetFocusedActor(AActor* NewTarget);

    // Можно ли с этим что-то сделать -- только такое подсвечивается.
    static bool IsHighlightable(const AActor* Actor);

    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
    void Highlight(AActor* Actor);
    void Unhighlight();

    TWeakObjectPtr<AActor> FocusedActor;

    // Что было на примитивах до подсветки (ревью 2026-09-21): снимая
    // подсветку, возвращаем их как было, а не выключаем Custom Depth
    // вслепую -- им может пользоваться что-то ещё.
    struct FSavedDepth
    {
        TWeakObjectPtr<UPrimitiveComponent> Primitive;
        bool bRenderCustomDepth = false;
        int32 StencilValue = 0;
    };
    TArray<FSavedDepth> SavedDepth;
};
