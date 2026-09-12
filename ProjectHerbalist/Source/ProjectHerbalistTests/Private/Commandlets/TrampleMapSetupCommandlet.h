// TrampleMapSetupCommandlet.h
//
// Заводит ассеты показа троп (2026-09-12), всё идемпотентно:
//   1. /Game/Materials/RT_TrampleMap -- 1024x1024 (FTrampleWindow::Size),
//      RGBA8, линейная гамма, билинейный, адресация Wrap.
//   2. В MPC_WorldStateFields -- параметры TrampleMapFrame и
//      TramplePlayerPosition, если их нет.
// Карты не трогает: пути к ассетам лежат в Herbalist Settings
// (Config/DefaultGame.ini), подсистема троп берёт их оттуда на любой карте.
//
// Отличие от RT_WorldStateMap -- ТОЛЬКО адресация: там Clamp (за краем сетки
// данных нет), здесь Wrap (окно адресуется по кругу: тексель = мировой
// тексель mod 1024, материал читает frac(WorldPos / 256 м)). С Clamp тропы
// легли бы одной полосой по краю текстуры.
//
// Запуск: UnrealEditor-Cmd.exe <uproject> -run=TrampleMapSetup
#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "TrampleMapSetupCommandlet.generated.h"

UCLASS()
class UTrampleMapSetupCommandlet : public UCommandlet
{
    GENERATED_BODY()

public:
    virtual int32 Main(const FString& Params) override;
};
