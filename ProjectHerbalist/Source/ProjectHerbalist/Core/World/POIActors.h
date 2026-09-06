// POIActors.h
//
// Визуал-логика точек интереса (DESIGN_POI_Art_And_LevelDesign.md,
// 2026-09-06, "проработаем POI" — визуал/левел-дизайн/звук поверх уже
// реализованной механики, §4 DESIGN_Brewing_Situations_And_Lore.md).
//
// Тот же паттерн, что уже AMemoryFragmentActor/AShrineActor: код НЕ
// хардкодит меши/материалы/звук ("контент назначается позже, на
// Blueprint-наследнике или размещённом акторе"), только логика видимости/
// интенсивности по данным мира и хук-точки для будущего арта, помеченные
// `// TODO: заменить на финальный арт`. Спавнятся сами
// AGridWorldManager::SeedPointsOfInterest (GridWorldManagerPOI.cpp) в
// координате уже засеянной точки, не размещаются вручную в уровне (в
// отличие от AShrineActor — капища левел-дизайнер расставляет сам,
// Тотем/Светлояр/Горюч-камень — детерминированный сев кода, актор просто
// следует за уже выбранной клеткой).
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "POIActors.generated.h"

class AGridWorldManager;
class UStaticMeshComponent;

// Тотем (§4.2, DESIGN_POI_Art_And_LevelDesign.md §1) — три яруса, три
// компонента-заглушки. Видимость каждого — независимый Tick-опрос своей
// переменной, не событие: тот же принцип, что уже IsSvetloyarVisible/
// GetTotemRevealText (запрос-only, не push-уведомление).
UCLASS(Blueprintable, BlueprintType)
class PROJECTHERBALIST_API APOI_Totem : public AActor
{
    GENERATED_BODY()

public:
    APOI_Totem();

    void Init(AGridWorldManager* InWorldManager, int32 InGridX, int32 InGridY);

protected:
    virtual void Tick(float DeltaTime) override;

    // TODO: заменить на финальный арт -- съёжившаяся придавленная фигура
    // (DESIGN_POI_Art_And_LevelDesign.md §1). Видна всегда, интенсивность
    // (LowerTierIntensity ниже) растёт с Distortion клетки.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UStaticMeshComponent> LowerTierMesh;

    // TODO: заменить на финальный арт -- люди, взявшиеся за руки. Видимость
    // переключается по IsTotemMiddleTierVisible() (Молва).
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UStaticMeshComponent> MiddleTierMesh;

    // TODO: заменить на финальный арт -- фигуры с воздетыми руками. Видимость
    // переключается по высокой Purity клетки (TotemUpperTierPurityThreshold).
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UStaticMeshComponent> UpperTierMesh;

    // Опрошенное Tick'ом значение Distortion клетки -- материал/партикл
    // нижнего яруса сможет читать его напрямую (Blueprint GetLowerTierIntensity),
    // не пересчитывать сам. [0,1], та же ось, что и Cell.State.Meta.Distortion.
    UPROPERTY(BlueprintReadOnly, Category = "Totem")
    float LowerTierIntensity = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Totem")
    bool bMiddleTierVisible = false;

    UPROPERTY(BlueprintReadOnly, Category = "Totem")
    bool bUpperTierVisible = false;

private:
    int32 GridX = -1;
    int32 GridY = -1;

    UPROPERTY()
    TObjectPtr<AGridWorldManager> WorldManager = nullptr;
};

// Светлояр (§4.5, DESIGN_POI_Art_And_LevelDesign.md §2) — три пороговых
// уровня по GlobalPerceptionClarity (дальний звон / звон+пение / вспышка
// купола+хор), не бинарная видимость. Порог самой видимости города уже
// есть (SvetloyarVisibilityClarityThreshold, AGridWorldManager::
// IsSvetloyarVisible) -- здесь только более мелкая, чисто визуальная
// градация ПОВЕРХ него для звука/эффекта.
UCLASS(Blueprintable, BlueprintType)
class PROJECTHERBALIST_API APOI_Svetloyar : public AActor
{
    GENERATED_BODY()

public:
    APOI_Svetloyar();

    void Init(AGridWorldManager* InWorldManager, int32 InGridX, int32 InGridY);

    // 0 = немо (Clarity ниже основного порога), 1 = дальний колокол,
    // 2 = колокол+пение, 3 = вспышка купола+полный хор (DESIGN_POI_Art_And_
    // LevelDesign.md §2, три уровня после самого порога видимости).
    UPROPERTY(BlueprintReadOnly, Category = "Svetloyar")
    int32 SoundTier = 0;

protected:
    virtual void Tick(float DeltaTime) override;

    // TODO: заменить на финальный арт -- призрачные купола под гладью/в
    // мареве, видимость управляется через bCityVisible ниже (см.
    // AGridWorldManager::IsSvetloyarVisible).
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UStaticMeshComponent> LakeMesh;

    UPROPERTY(BlueprintReadOnly, Category = "Svetloyar")
    bool bCityVisible = false;

private:
    int32 GridX = -1;
    int32 GridY = -1;

    UPROPERTY()
    TObjectPtr<AGridWorldManager> WorldManager = nullptr;
};

// Горюч-камень (§4.5, DESIGN_POI_Art_And_LevelDesign.md §3) — сама клетка
// НИКОГДА не меняется (ApplyAlchemyResult/ProcessApplyCommand,
// bTargetIsGoryuchKamen), поэтому актору нечего опрашивать в State/
// TargetState -- только счётчик попыток (GetGoryuchKamenApplyAttemptCount)
// для "глухого стука" при срабатывании.
UCLASS(Blueprintable, BlueprintType)
class PROJECTHERBALIST_API APOI_GoryuchKamen : public AActor
{
    GENERATED_BODY()

public:
    APOI_GoryuchKamen();

    void Init(AGridWorldManager* InWorldManager, int32 InGridX, int32 InGridY);

protected:
    virtual void Tick(float DeltaTime) override;

    // TODO: заменить на финальный арт -- тёмный валун с тёплым рдяным
    // отливом на изломе (DESIGN_POI_Art_And_LevelDesign.md §3). Статичен по
    // определению -- никакой Tick-логики видимости не требуется, только
    // сам меш.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UStaticMeshComponent> StoneMesh;

    // TODO: подключить звук глухого удара -- BlueprintImplementableEvent,
    // чтобы контент мог назначить конкретный звуковой каскад без правки C++.
    UFUNCTION(BlueprintImplementableEvent, Category = "GoryuchKamen")
    void OnThud();

private:
    int32 GridX = -1;
    int32 GridY = -1;
    int32 LastSeenAttemptCount = 0;

    UPROPERTY()
    TObjectPtr<AGridWorldManager> WorldManager = nullptr;
};
