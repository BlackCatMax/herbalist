// HerbalistPlayerController.h
#pragma once

#include "CoreMinimal.h"
#include "Math/RandomStream.h"
#include "GameFramework/PlayerController.h"
#include "InputActionValue.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Core/Inventory/HerbalistInventoryComponent.h"
#include "Core/Journal/HerbalistJournalComponent.h"
#include "UI/InventoryWidget.h"
#include "UI/AlchemyTransferWidget.h"
#include "UI/JournalWidget.h"
#include "HerbalistPlayerController.generated.h"

class AStorageContainer;
class UInventoryTransferWidget;
class AAlchemyTableActor;
class AGridWorldManager;
class AHerbalistResourceActor;

UCLASS()
class PROJECTHERBALIST_API AHerbalistPlayerController : public APlayerController
{
    GENERATED_BODY()

public:
    AHerbalistPlayerController();
	
	virtual void SetupInputComponent() override;
    virtual void BeginPlay() override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory")
    UHerbalistInventoryComponent* InventoryComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Journal")
    UHerbalistJournalComponent* JournalComponent;

    UPROPERTY(BlueprintReadOnly, Category = "UI")
    bool bIsAnyWidgetOpen = false;

    UPROPERTY()
    UInventoryTransferWidget* CurrentTransferWidget = nullptr;

    UPROPERTY()
    UAlchemyTransferWidget* CurrentAlchemyWidget = nullptr;

    UPROPERTY()
    AAlchemyTableActor* CurrentAlchemyTable = nullptr;
	
    UPROPERTY(BlueprintReadOnly, Category = "Alchemy")
    float CurrentGlobalDistortion = 0.3f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Herbalist|Harvesting")
    float MaxHarvestDistance = 200.0f;

    // Текущий инструмент сбора (DESIGN_Community_And_Homestead.md §2.3,
    // 2026-08-31) — читается AGridWorldManager::OnResourceCollected в
    // Cmd.Harvest.Tool. v1: переключается Exec-командой (SetGatheringTool),
    // не физическим предметом в инвентаре — экономика источников
    // инструментов (курганы/дары хозяев/торговля) отдельный, ещё не
    // реализованный проход; сам множитель качества работает уже сейчас.
    UPROPERTY(BlueprintReadOnly, Category = "Herbalist|Harvesting")
    EGatheringTool CurrentGatheringTool = EGatheringTool::BareHands;

    // Два предмета-спутника (21_Journey_And_Artifacts.md §21.2) — один и
    // тот же неполный дар Аграфены, переданный второпях (§17.1), не два
    // отдельных подарка. НЕ предметы инвентаря (FInventoryItem заточен под
    // распад/стек, плохо подходит постоянному дару) — два bool, тот же
    // принцип, что уже CurrentGatheringTool не инвентарный предмет. TODO:
    // выдача обоих true — на месте, где по сюжету происходит передача
    // дара (не решаю сам, где именно — не лорная задача этой сессии).
    UPROPERTY(BlueprintReadOnly, Category = "Herbalist|Zaryana")
    bool bHasMirror = false;

    UPROPERTY(BlueprintReadOnly, Category = "Herbalist|Zaryana")
    bool bHasYarnBall = false;

    // "Прогретое" состояние (21_Journey_And_Artifacts.md §21.3-21.4,
    // 2026-09-01) — Зеркальце/Клубочек не заводят новый предмет как
    // артефакты Гамаюна/Мать-Сыры-Земли, а переключают дар в это состояние
    // (см. ArtifactTypes.h::bWarmsCompanionItem, OfferForArtifact ниже).
    // Конкретный усиленный эффект НЕ реализован — §21.4 сама называет его
    // только направлением, не финальным списком.
    UPROPERTY(BlueprintReadOnly, Category = "Herbalist|Zaryana")
    bool bMirrorWarmed = false;

    UPROPERTY(BlueprintReadOnly, Category = "Herbalist|Zaryana")
    bool bYarnBallWarmed = false;

    UFUNCTION(BlueprintCallable, Category = "UI")
    void CloseAnyWidget();

    UFUNCTION(Exec)
    void HarvestTest(int32 X, int32 Y);
    UFUNCTION(Exec)
    void ApplyTest(int32 X, int32 Y);
    UFUNCTION(Exec)
    void ShowInventory();
    UFUNCTION(Exec)
    void MassHarvestTest(int32 X, int32 Y, int32 Count);
	
	UFUNCTION(Exec)
	void TestNewHarvest(int32 X, int32 Y, FName IngredientID);

    UFUNCTION(Exec, BlueprintCallable, Category = "Alchemy")
    void UsePotion();
    
    UFUNCTION(BlueprintCallable, Category = "Herbalist|Harvesting")
    bool CanHarvestActor(AActor* TargetActor) const;
	
    AGridWorldManager* FindWorldManager() const;
    void GetCellFromHit(const FHitResult& Hit, int32& OutX, int32& OutY) const;
    void UpdateDistortionFromCell(int32 X, int32 Y);

    // Свой фиксированный сид, не WorldRNG — тот же приём, что уже
    // AlchemySlotWidget.cpp::PerceptionRng: наблюдение через Зеркальце не
    // должно потреблять/возмущать детерминированный поток симуляции.
    FRandomStream MirrorPerceptionRng = FRandomStream(20260901);
	
    UFUNCTION(Exec)
    void TestNewTransfer(FName IngredientID, int32 Amount);

    UFUNCTION(Exec)
    void TestNewApply(int32 X, int32 Y, FString IngredientList);

    // Сохранения v1 (Core/Save/HerbalistSaveSubsystem.h) — тонкие обёртки над
    // подсистемой, тем же паттерном, что HarvestTest/ApplyTest над пайплайном.
    UFUNCTION(Exec)
    void SaveGame();

    UFUNCTION(Exec)
    void LoadGame();

    // Экран Травника (UI/JournalLogWidget.h, 07_UX §7.2.4) — открывается тем
    // же способом, что и инвентарь (Inventory()). Exec-обёртка отдельно от
    // JournalAction — тестируемо консолью и до того, как в редакторе назначен
    // Input Action asset (тот же паттерн, что InventoryAction). Сам виджет
    // (2026-08-29, "лог, чтобы можно было открыть и почитать") больше не
    // ждёт WBP — JournalLogWidget строит дерево в C++, готов без единого
    // .uasset; JournalWidgetClass/UJournalWidget ниже остаются на будущее,
    // если понадобится более богатый экран через редактор.
    UFUNCTION(Exec)
    void ToggleJournalUI();

    // Переключить текущий инструмент сбора (§ комментарий у CurrentGatheringTool
    // выше). ToolName: "hands"/"iron"/"copper"/"bone" — строка, не enum,
    // тем же паттерном, что SetGatheringTool видится проще с консоли, чем
    // числовой индекс EGatheringTool.
    UFUNCTION(Exec)
    void SetGatheringTool(FString ToolName);

    // Диалоги (DESIGN_Community_And_Homestead.md §1.1, 2026-08-31) — v1
    // консольный, тем же принципом, что SetGatheringTool/SetGardenPlot:
    // TalkTo открывает разговор с хозяином места на клетке (X,Y) — печатает
    // реплику узла и доступные по Respect ветки в лог; ChooseDialogueBranch
    // выбирает ветку по индексу из ПОСЛЕДНЕГО напечатанного списка. Текущий
    // узел разговора — состояние контроллера, не мира (см. комментарий у
    // FTalkCommand, CommandTypes.h): разговор не переживает выход игрока
    // из зоны взаимодействия, не сохраняется.
    UFUNCTION(Exec)
    void TalkTo(int32 X, int32 Y);

    UFUNCTION(Exec)
    void ChooseDialogueBranch(int32 BranchIndex);

    // Подношение общине (DESIGN_Community_And_Homestead.md §1.3, 2026-08-31)
    // — тот же приём, что TestNewApply: ингредиенты по именам через запятую,
    // ищутся в инвентаре, списываются, ΔMolva логируется в
    // AGridWorldManager::OfferToCommunity.
    UFUNCTION(Exec)
    void OfferToCommunity(FString IngredientList);

    // Торговля с общиной (§1.2) — WantedIngredientID должен существовать в
    // реестре (DT_IngredientClass), полученный предмет добавляется в
    // инвентарь тем же способом, что и любой другой (AddItem).
    UFUNCTION(Exec)
    void TradeWithCommunity(FString OfferedIngredientID, FString WantedIngredientID);

    // Зарегистрировать клетку как грядку сада с пристройкой NicheName
    // (DESIGN_Community_And_Homestead.md §2.4, 2026-08-31) — "mycelium"/
    // "cellar"/"pond"/"sunny"/"shade", "none" снимает регистрацию. v1:
    // тот же приём, что SetGatheringTool — консоль вместо физической
    // постройки-актора, сам механизм (AGridWorldManager::GardenPlots →
    // GetRandomResourceForNiche) работает уже сейчас.
    UFUNCTION(Exec)
    void SetGardenPlot(int32 X, int32 Y, FString NicheName);

    // Основать базу (21_Journey_And_Artifacts.md §21.2, 2026-09-01) — v1
    // тот же приём, что SetGardenPlot: консоль вместо физической
    // постройки-актора, сам механизм (AGridWorldManager::Bases →
    // IsValidBrewingLocation) работает уже сейчас. Валидация (не вода, не
    // дубликат) — на стороне AGridWorldManager::RegisterBase.
    UFUNCTION(Exec)
    void FoundBase(int32 X, int32 Y);

    // Отладочная выдача обоих предметов-спутников — заглушка на месте
    // реального сюжетного вручения (см. bHasMirror/bHasYarnBall выше),
    // тот же класс v1-упрощения, что и остальные Exec-команды этого файла.
    UFUNCTION(Exec)
    void GiveZaryanaGifts();

    // Зеркальце — наблюдение Заряны из любой базы, не перемещение
    // (§21.2). Переиспользует AGridWorldManager::GetZaryanaPerceivedState
    // (Слои 1+3 + честный шум PerceiveRealState, шаг 2) — не привилегированное
    // окно истины, тот же шум, что и роса вживую.
    UFUNCTION(Exec, BlueprintCallable, Category = "Zaryana")
    void UseMirror();

    // Клубочек — перемещение между уже основанными базами (индекс в
    // AGridWorldManager::GetBases()), НЕ мгновенное: тратит игровое время,
    // соразмерное дистанции (§21.2), двигая WorldManager->GameClockSeconds
    // вперёд тем же способом, что и обычный ход времени — протаскивает
    // погоду/сутки/регенерацию, не спецэффект.
    UFUNCTION(Exec, BlueprintCallable, Category = "Zaryana")
    void UseYarnBall(int32 BaseIndex);

    // Подношение за артефакт Легендарной сущности (21_Journey_And_Artifacts.md
    // §21.3-21.4) — тот же приём поиска по имени в инвентаре, что уже
    // OfferToCommunity. ArtifactID — точное имя из ArtifactTypes.h ("Рог",
    // "Гребень", "Молодильное яблоко", "Шапка-невидимка", "Камень-оберег",
    // "Фонарь", "Зеркальце", "Клубочек"). Для двух последних — успех переключает
    // bMirrorWarmed/bYarnBallWarmed вместо новой записи в
    // AGridWorldManager::AcquiredArtifacts (см. AGridWorldManager::
    // TryAcquireArtifact).
    UFUNCTION(Exec)
    void OfferForArtifact(FString ArtifactID, FString IngredientList);

    // Рог (Индрик-зверь, §21.3) — "слушает воду": честная диагностика
    // клетки-родника (X,Y), не требует запаса, просто предмет должен быть
    // добыт. Показ — тем же ShowMemoryRevealText, что и Зеркальце.
    UFUNCTION(Exec)
    void UseHorn(int32 X, int32 Y);

    // Гребень (Берегиня, §21.3) — расходуемый побег: снимает проявленную
    // сущность с клетки (X,Y), списывается после одного применения.
    UFUNCTION(Exec)
    void UseComb(int32 X, int32 Y);

    // Молодильное яблоко (Дуб-старец, §21.3) — расходуемое: временное окно
    // сниженного шума росы Заряны.
    UFUNCTION(Exec)
    void UseYouthApple();

    // Шапка-невидимка (Баба-Яга, §21.3) — не расходуется: временно
    // подавляет новые проявления, можно активировать снова после истечения.
    UFUNCTION(Exec)
    void UseInvisibilityCap();

    // Три исхода у Буяна (18_Ending.md §18.1-18.2, 2026-09-01, ревизия
    // "Ending and artifacts") — выбор через действие (три разные команды),
    // не диалоговое меню (§18.1). Каждая требует AGridWorldManager::
    // TryChooseBuyanPath == true (не переигрывается, путь 1 гейтится
    // Clarity+Молвой) — при успехе он сам показывает гарантированный
    // финальный фрагмент памяти (BUYAN_GUARDIAN/BUYAN_TRADE_PLACES/
    // BUYAN_ACCEPT_REALITY, MemoryFragmentDefinitions.h) через
    // CollectMemoryFragment, эти три команды его не дублируют.
    UFUNCTION(Exec)
    void BecomeBuyanGuardian();

    UFUNCTION(Exec)
    void TradePlacesWithZaryana();

    UFUNCTION(Exec)
    void AcceptBuyanReality();

    // Экранный попап текста воспоминания Заряны/объявления Буяна
    // (UI/MemoryRevealWidget.h, "Прогрессия/Заряна" 2026-08-29) — вызывается
    // из AGridWorldManager (CollectMemoryFragment/CheckBuyanCondition), не
    // ждёт открытия Травника: фрагмент должен быть прочитан в момент сбора,
    // Травник ниже — для перечитать позже, не единственный способ увидеть.
    void ShowMemoryRevealText(const FText& Text);

protected:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
    UInputMappingContext* DefaultMappingContext;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
    UInputAction* MoveAction;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
    UInputAction* LookAction;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
    UInputAction* HarvestAction;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
    UInputAction* InfoAction;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
    UInputAction* InventoryAction;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
    UInputAction* ApplyAlchemyAction;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
    UInputAction* InteractAction;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
    UInputAction* JournalAction;

    UPROPERTY(EditDefaultsOnly, Category = "UI")
    TSubclassOf<UInventoryWidget> InventoryWidgetClass;

    UPROPERTY(EditDefaultsOnly, Category = "UI")
    TSubclassOf<UJournalWidget> JournalWidgetClass;

    void Move(const FInputActionValue& Value);
    void Look(const FInputActionValue& Value);
    void Harvest();
    void Info();
    void Inventory();
    void Journal();
    void ApplyAlchemy();
    void Interact();

    bool GetHitResultFromCamera(FHitResult& OutHit, ECollisionChannel Channel = ECC_Visibility);
    void OnLeftClick();
    void OnRightClick();
    void OnApplyAlchemyKey();

    void OnUsePotion();

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
    UInputAction* UsePotionAction;

private:
    UPROPERTY()
    UInventoryWidget* InventoryWidgetInstance = nullptr;

    // UUserWidget, не UJournalWidget -- Journal() сейчас строит UJournalLogWidget
    // (см. комментарий у ToggleJournalUI ниже), JournalWidgetInstance хранит
    // и то, и другое одинаково: обе точки использования (IsInViewport/
    // RemoveFromParent) базовые, конкретный подкласс не нужен.
    UPROPERTY()
    UUserWidget* JournalWidgetInstance = nullptr;

    UPROPERTY()
    class UMemoryRevealWidget* MemoryRevealWidgetInstance = nullptr;

    // Кэш для мира
    UPROPERTY()
    AGridWorldManager* CachedWorldManager = nullptr;

    bool TryHarvestResource(AHerbalistResourceActor* Resource);

    // Текущий разговор (TalkTo/ChooseDialogueBranch, § комментарий у их
    // объявления выше) — состояние UI-сессии, не мира, не UPROPERTY(SaveGame)
    // намеренно.
    FName CurrentDialogueID = NAME_None;
    FName CurrentDialogueNodeID = NAME_None;
    FIntPoint CurrentDialogueCell = FIntPoint(-1, -1);
};