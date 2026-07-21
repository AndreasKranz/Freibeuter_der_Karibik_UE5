// Copyright Epic Games, Inc. All Rights Reserved.

#include "FreibeuterSchedulerSubsystem.h"

#include "FreibeuterDataTableRows.h"
#include "FreibeuterSchedulerLog.h"
#include "HAL/IConsoleManager.h"

namespace FreibeuterTuning
{
	// DataTable asset paths. Real port/trader/goods numbers are still open per FREIBE_1.md §10.
	static const TCHAR* PortDataTablePath = TEXT("/Game/Freibeuter/Data/DT_FreibeuterPorts.DT_FreibeuterPorts");
	static const TCHAR* DistanceDataTablePath = TEXT("/Game/Freibeuter/Data/DT_FreibeuterPortDistances.DT_FreibeuterPortDistances");
	static const TCHAR* TraderDataTablePath = TEXT("/Game/Freibeuter/Data/DT_FreibeuterTraders.DT_FreibeuterTraders");
	static const TCHAR* GoodsDataTablePath = TEXT("/Game/Freibeuter/Data/DT_FreibeuterGoods.DT_FreibeuterGoods");

	// Phase 0's playable slice (FREIBE_1.md §3: "2-3 ports, one trader"). The other 5 real ports and
	// 4 real traders are populated in the DataTables but sit unused until Phase 1 ("all 8 ports, all
	// 5 traders"). San Juan is the dossier's confirmed starting port for every ship.
	static const FName StartingPortId(TEXT("SanJuan"));

	// FREIBE_1.md §4: confirmed starting capital.
	static constexpr int32 StartingGold = 1500;

	// Cargo hold capacity is governed by the Chest upgrade in the real game; Phase 0 has no Chest
	// trader, so this is a flat placeholder baseline rather than a per-ship upgradeable value.
	static constexpr int32 BaseCargoCapacity = 10;

	// Placeholder payout formula (FREIBE_1.md §10 items 2-3 are unrecovered): longer voyages and
	// longer-neglected ports pay more, per CLAUDE.md §4's economy notes.
	static constexpr float DistanceMultiplierPerDay = 0.1f;
	static constexpr float ScarcityMultiplierPerDay = 0.02f;
	static constexpr int32 ScarcityCapDays = 60;

	// Placeholder combat consequence (FREIBE_1.md §5: real formula unrecovered).
	static constexpr int32 EncounterDelayDays = 2;

	static bool DayAscending(const FFreibeuterScheduledEvent& A, const FFreibeuterScheduledEvent& B)
	{
		return A.TriggerDay < B.TriggerDay;
	}
}

void UFreibeuterSchedulerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	IConsoleManager& ConsoleManager = IConsoleManager::Get();

	RegisteredConsoleCommands.Add(ConsoleManager.RegisterConsoleCommand(
		TEXT("Freibeuter.RunSchedulerDemo"),
		TEXT("Runs the Phase 0 day-based scheduler proof-of-concept and logs interleaved event resolution to LogFreibeuterScheduler."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateUObject(this, &UFreibeuterSchedulerSubsystem::HandleRunSchedulerDemoCommand),
		ECVF_Default));

	RegisteredConsoleCommands.Add(ConsoleManager.RegisterConsoleCommand(
		TEXT("Freibeuter.NewGame"),
		TEXT("Starts a fresh Phase 0 playthrough: one ship, full gold, docked at San Juan."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateUObject(this, &UFreibeuterSchedulerSubsystem::HandleNewGameCommand),
		ECVF_Default));

	RegisteredConsoleCommands.Add(ConsoleManager.RegisterConsoleCommand(
		TEXT("Freibeuter.SetSail"),
		TEXT("Usage: Freibeuter.SetSail <DestinationPortId> <Good> <Quantity>. Loads cargo and commits the player ship to a voyage."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateUObject(this, &UFreibeuterSchedulerSubsystem::HandleSetSailCommand),
		ECVF_Default));

	RegisteredConsoleCommands.Add(ConsoleManager.RegisterConsoleCommand(
		TEXT("Freibeuter.Advance"),
		TEXT("Resolves the player ship's next pending event (Encounter or Arrival)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateUObject(this, &UFreibeuterSchedulerSubsystem::HandleAdvanceCommand),
		ECVF_Default));

	RegisteredConsoleCommands.Add(ConsoleManager.RegisterConsoleCommand(
		TEXT("Freibeuter.BuyCannon"),
		TEXT("Buys a Cannon from the trader at the player ship's current port, if one is docked there."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateUObject(this, &UFreibeuterSchedulerSubsystem::HandleBuyCannonCommand),
		ECVF_Default));

	RegisteredConsoleCommands.Add(ConsoleManager.RegisterConsoleCommand(
		TEXT("Freibeuter.Status"),
		TEXT("Logs the player ship's day, location, cargo, gold, and VP."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateUObject(this, &UFreibeuterSchedulerSubsystem::HandleStatusCommand),
		ECVF_Default));
}

void UFreibeuterSchedulerSubsystem::Deinitialize()
{
	IConsoleManager& ConsoleManager = IConsoleManager::Get();
	for (IConsoleObject* Command : RegisteredConsoleCommands)
	{
		ConsoleManager.UnregisterConsoleObject(Command);
	}
	RegisteredConsoleCommands.Reset();

	Super::Deinitialize();
}

void UFreibeuterSchedulerSubsystem::HandleRunSchedulerDemoCommand(const TArray<FString>& Args, UWorld* InWorld)
{
	RunSchedulerDemo();
}

void UFreibeuterSchedulerSubsystem::HandleNewGameCommand(const TArray<FString>& Args, UWorld* InWorld)
{
	NewGame();
}

void UFreibeuterSchedulerSubsystem::HandleSetSailCommand(const TArray<FString>& Args, UWorld* InWorld)
{
	if (Args.Num() != 3)
	{
		UE_LOG(LogFreibeuterScheduler, Warning, TEXT("Usage: Freibeuter.SetSail <DestinationPortId> <Good> <Quantity>"));
		return;
	}

	EFreibeuterGood Good;
	if (!ParseGood(Args[1], Good))
	{
		UE_LOG(LogFreibeuterScheduler, Warning, TEXT("Unknown good '%s'. Expected Wheat, Wood, Tobacco, or Rum."), *Args[1]);
		return;
	}

	SetSail(FName(*Args[0]), Good, FCString::Atoi(*Args[2]));
}

void UFreibeuterSchedulerSubsystem::HandleAdvanceCommand(const TArray<FString>& Args, UWorld* InWorld)
{
	Advance();
}

void UFreibeuterSchedulerSubsystem::HandleBuyCannonCommand(const TArray<FString>& Args, UWorld* InWorld)
{
	BuyCannon();
}

void UFreibeuterSchedulerSubsystem::HandleStatusCommand(const TArray<FString>& Args, UWorld* InWorld)
{
	Status();
}

void UFreibeuterSchedulerSubsystem::PushEvent(const FFreibeuterScheduledEvent& Event)
{
	EventHeap.HeapPush(Event, &FreibeuterTuning::DayAscending);
}

bool UFreibeuterSchedulerSubsystem::PopNextEvent(FFreibeuterScheduledEvent& OutEvent)
{
	if (EventHeap.Num() == 0)
	{
		return false;
	}
	EventHeap.HeapPop(OutEvent, &FreibeuterTuning::DayAscending);
	return true;
}

bool UFreibeuterSchedulerSubsystem::HasPendingArrival(int32 ShipId) const
{
	return EventHeap.ContainsByPredicate([ShipId](const FFreibeuterScheduledEvent& Candidate)
	{
		return Candidate.ShipId == ShipId && Candidate.EventType == EFreibeuterEventType::Arrival;
	});
}

void UFreibeuterSchedulerSubsystem::RescheduleShipArrival(int32 ShipId, int32 NewArrivalDay)
{
	FFreibeuterShipState* Ship = Ships.Find(ShipId);
	if (!Ship)
	{
		return;
	}
	Ship->ArrivalDay = NewArrivalDay;

	const int32 EventIndex = EventHeap.IndexOfByPredicate([ShipId](const FFreibeuterScheduledEvent& Candidate)
	{
		return Candidate.ShipId == ShipId && Candidate.EventType == EFreibeuterEventType::Arrival;
	});

	if (EventIndex == INDEX_NONE)
	{
		UE_LOG(LogFreibeuterScheduler, Warning, TEXT("RescheduleShipArrival: no pending Arrival event found for ship %d"), ShipId);
		return;
	}

	EventHeap[EventIndex].TriggerDay = NewArrivalDay;

	// The heap invariant only held for the old key; mutating a key in place needs a full
	// re-heapify rather than an incremental sift-up/down.
	EventHeap.Heapify(&FreibeuterTuning::DayAscending);
}

int32 UFreibeuterSchedulerSubsystem::GetDistanceDays(FName FromPortId, FName ToPortId) const
{
	if (!DistanceDataTable)
	{
		return 1;
	}

	const FName RowName(*FString::Printf(TEXT("%s_%s"), *FromPortId.ToString(), *ToPortId.ToString()));
	static const FString ContextString(TEXT("GetDistanceDays"));
	if (const FFreibeuterPortDistanceRow* Row = DistanceDataTable->FindRow<FFreibeuterPortDistanceRow>(RowName, ContextString))
	{
		return Row->DistanceDays;
	}

	UE_LOG(LogFreibeuterScheduler, Warning, TEXT("No distance row for %s -> %s, defaulting to 1 day"), *FromPortId.ToString(), *ToPortId.ToString());
	return 1;
}

FName UFreibeuterSchedulerSubsystem::GetGoodRowName(EFreibeuterGood Good)
{
	switch (Good)
	{
	case EFreibeuterGood::Wheat: return FName(TEXT("Wheat"));
	case EFreibeuterGood::Wood: return FName(TEXT("Wood"));
	case EFreibeuterGood::Tobacco: return FName(TEXT("Tobacco"));
	case EFreibeuterGood::Rum: return FName(TEXT("Rum"));
	default: return NAME_None;
	}
}

bool UFreibeuterSchedulerSubsystem::ParseGood(const FString& Text, EFreibeuterGood& OutGood)
{
	if (Text.Equals(TEXT("Wheat"), ESearchCase::IgnoreCase)) { OutGood = EFreibeuterGood::Wheat; return true; }
	if (Text.Equals(TEXT("Wood"), ESearchCase::IgnoreCase)) { OutGood = EFreibeuterGood::Wood; return true; }
	if (Text.Equals(TEXT("Tobacco"), ESearchCase::IgnoreCase)) { OutGood = EFreibeuterGood::Tobacco; return true; }
	if (Text.Equals(TEXT("Rum"), ESearchCase::IgnoreCase)) { OutGood = EFreibeuterGood::Rum; return true; }
	return false;
}

int32 UFreibeuterSchedulerSubsystem::GetBaseValuePerUnit(EFreibeuterGood Good) const
{
	if (!GoodsDataTable)
	{
		return 0;
	}

	static const FString ContextString(TEXT("GetBaseValuePerUnit"));
	if (const FFreibeuterGoodRow* Row = GoodsDataTable->FindRow<FFreibeuterGoodRow>(GetGoodRowName(Good), ContextString))
	{
		return Row->BaseValuePerUnit;
	}
	return 0;
}

bool UFreibeuterSchedulerSubsystem::RollForEncounter(const TArray<FFreibeuterCargoItem>& Cargo) const
{
	// Odds are DataTable-driven (DT_FreibeuterGoods), not hardcoded, per CLAUDE.md §2.2. The real
	// combat/encounter formula is still unrecovered (FREIBE_1.md §10 item 5) -- these are placeholders.
	if (!GoodsDataTable)
	{
		return false;
	}

	static const FString ContextString(TEXT("RollForEncounter"));
	float EncounterChance = 0.f;
	for (const FFreibeuterCargoItem& Item : Cargo)
	{
		if (const FFreibeuterGoodRow* Row = GoodsDataTable->FindRow<FFreibeuterGoodRow>(GetGoodRowName(Item.Good), ContextString))
		{
			EncounterChance = FMath::Max(EncounterChance, Row->EncounterChance);
		}
	}
	return FMath::FRand() < EncounterChance;
}

int32 UFreibeuterSchedulerSubsystem::RegisterShip(FName ShipName, FName StartPortId)
{
	const int32 ShipId = NextShipId++;

	FFreibeuterShipState Ship;
	Ship.ShipId = ShipId;
	Ship.ShipName = ShipName;
	Ship.CurrentPortId = StartPortId;
	Ships.Add(ShipId, Ship);

	return ShipId;
}

void UFreibeuterSchedulerSubsystem::CommitVoyage(int32 ShipId, FName DestinationPortId, const TArray<FFreibeuterCargoItem>& Cargo)
{
	CommitVoyageInternal(ShipId, DestinationPortId, Cargo, RollForEncounter(Cargo), -1);
}

void UFreibeuterSchedulerSubsystem::CommitVoyageInternal(int32 ShipId, FName DestinationPortId, const TArray<FFreibeuterCargoItem>& Cargo, bool bEncounterOccurs, int32 ForcedEncounterOutcome)
{
	FFreibeuterShipState* Ship = Ships.Find(ShipId);
	if (!Ship)
	{
		UE_LOG(LogFreibeuterScheduler, Warning, TEXT("CommitVoyage: unknown ShipId %d"), ShipId);
		return;
	}

	const int32 TravelDays = GetDistanceDays(Ship->CurrentPortId, DestinationPortId);

	// An encounter needs a day strictly between departure and arrival; a 1-day hop has none.
	const bool bCanHostEncounter = TravelDays >= 2;

	Ship->DestinationPortId = DestinationPortId;
	Ship->Cargo = Cargo;
	Ship->DepartureDay = CurrentDay;
	Ship->ArrivalDay = CurrentDay + TravelDays;

	UE_LOG(LogFreibeuterScheduler, Log, TEXT("Day %d: %s commits to voyage %s -> %s (%d days); arrival scheduled for day %d"),
		CurrentDay, *Ship->ShipName.ToString(), *Ship->CurrentPortId.ToString(), *DestinationPortId.ToString(), TravelDays, Ship->ArrivalDay);

	FFreibeuterScheduledEvent ArrivalEvent;
	ArrivalEvent.EventType = EFreibeuterEventType::Arrival;
	ArrivalEvent.TriggerDay = Ship->ArrivalDay;
	ArrivalEvent.ShipId = ShipId;
	PushEvent(ArrivalEvent);

	if (bEncounterOccurs && bCanHostEncounter)
	{
		const int32 EncounterDay = Ship->DepartureDay + FMath::Max(1, TravelDays / 2);

		FFreibeuterScheduledEvent EncounterEvent;
		EncounterEvent.EventType = EFreibeuterEventType::Encounter;
		EncounterEvent.TriggerDay = EncounterDay;
		EncounterEvent.ShipId = ShipId;
		EncounterEvent.DebugForcedOutcome = ForcedEncounterOutcome;
		PushEvent(EncounterEvent);

		UE_LOG(LogFreibeuterScheduler, Log, TEXT("Day %d: %s -- pirate encounter rolled, scheduled for day %d (before arrival)"),
			CurrentDay, *Ship->ShipName.ToString(), EncounterDay);
	}
}

void UFreibeuterSchedulerSubsystem::RunSchedulerToCompletion()
{
	FFreibeuterScheduledEvent Event;
	while (PopNextEvent(Event))
	{
		CurrentDay = Event.TriggerDay;
		ResolveEvent(Event);
	}

	UE_LOG(LogFreibeuterScheduler, Log, TEXT("Scheduler drained; final day reached: %d"), CurrentDay);
}

void UFreibeuterSchedulerSubsystem::ResolveEvent(const FFreibeuterScheduledEvent& Event)
{
	switch (Event.EventType)
	{
	case EFreibeuterEventType::Arrival:
		ResolveArrival(Event);
		break;

	case EFreibeuterEventType::Encounter:
	{
		bool bPlayerWins;
		if (Event.DebugForcedOutcome == 0)
		{
			bPlayerWins = false;
		}
		else if (Event.DebugForcedOutcome == 1)
		{
			bPlayerWins = true;
		}
		else
		{
			// Placeholder 50/50 until the real combat formula (FREIBE_1.md §5, §10 item 5) is
			// reconstructed. Phase 1's UI will surface this probability to the player before they
			// commit (CLAUDE.md §5.1); there's no player-choice step yet, so Phase 0 auto-resolves.
			bPlayerWins = FMath::FRand() < 0.5f;
		}
		ResolveEncounterInternal(Event.ShipId, bPlayerWins);
		break;
	}
	}
}

void UFreibeuterSchedulerSubsystem::ResolveArrival(const FFreibeuterScheduledEvent& Event)
{
	FFreibeuterShipState* Ship = Ships.Find(Event.ShipId);
	if (!Ship)
	{
		return;
	}

	UE_LOG(LogFreibeuterScheduler, Log, TEXT("Day %d: %s ARRIVES at %s"), CurrentDay, *Ship->ShipName.ToString(), *Ship->DestinationPortId.ToString());

	const FName ArrivedFrom = Ship->CurrentPortId;
	Ship->CurrentPortId = Ship->DestinationPortId;
	Ship->DestinationPortId = NAME_None;

	SellCargoOnArrival(*Ship);

	if (int32* LegsRemaining = ShuttleLegsRemaining.Find(Event.ShipId))
	{
		if (*LegsRemaining > 0)
		{
			--(*LegsRemaining);
			const TArray<FFreibeuterCargoItem> Cargo = Ship->Cargo;
			CommitVoyage(Event.ShipId, ArrivedFrom, Cargo);
		}
	}
}

void UFreibeuterSchedulerSubsystem::SellCargoOnArrival(FFreibeuterShipState& Ship)
{
	if (Ship.Cargo.Num() == 0)
	{
		return;
	}

	FFreibeuterPortRuntimeState& PortState = PortRuntimeStates.FindOrAdd(Ship.CurrentPortId);
	const int32 TravelDays = Ship.ArrivalDay - Ship.DepartureDay;
	const float DistanceMultiplier = 1.f + TravelDays * FreibeuterTuning::DistanceMultiplierPerDay;

	int32 TotalProfit = 0;
	for (const FFreibeuterCargoItem& Item : Ship.Cargo)
	{
		const int32 LastDelivery = PortState.LastDeliveryDay.FindRef(Item.Good);
		const int32 DaysSinceDelivery = FMath::Clamp(CurrentDay - LastDelivery, 0, FreibeuterTuning::ScarcityCapDays);
		const float ScarcityMultiplier = 1.f + DaysSinceDelivery * FreibeuterTuning::ScarcityMultiplierPerDay;

		const int32 BaseValue = GetBaseValuePerUnit(Item.Good);
		const int32 ItemProfit = FMath::RoundToInt(BaseValue * Item.Quantity * DistanceMultiplier * ScarcityMultiplier);
		TotalProfit += ItemProfit;

		UE_LOG(LogFreibeuterScheduler, Log, TEXT("  Sold %d x %s: base %d/unit x dist %.2f x scarcity %.2f (%d days) = %d Dukaten"),
			Item.Quantity, *GetGoodRowName(Item.Good).ToString(), BaseValue, DistanceMultiplier, ScarcityMultiplier, DaysSinceDelivery, ItemProfit);

		PortState.LastDeliveryDay.Add(Item.Good, CurrentDay);
	}

	Ship.Gold += TotalProfit;
	Ship.Cargo.Reset();

	UE_LOG(LogFreibeuterScheduler, Log, TEXT("Day %d: %s nets %d Dukaten at %s. Gold: %d"),
		CurrentDay, *Ship.ShipName.ToString(), TotalProfit, *Ship.CurrentPortId.ToString(), Ship.Gold);
}

void UFreibeuterSchedulerSubsystem::ResolveEncounterInternal(int32 ShipId, bool bPlayerWins)
{
	FFreibeuterShipState* Ship = Ships.Find(ShipId);
	if (!Ship)
	{
		return;
	}

	if (bPlayerWins)
	{
		UE_LOG(LogFreibeuterScheduler, Log, TEXT("Day %d: %s WINS the encounter -- arrival unaffected, still day %d"),
			CurrentDay, *Ship->ShipName.ToString(), Ship->ArrivalDay);
		return;
	}

	const int32 NewArrivalDay = Ship->ArrivalDay + FreibeuterTuning::EncounterDelayDays;

	UE_LOG(LogFreibeuterScheduler, Log, TEXT("Day %d: %s LOSES the encounter -- arrival delayed from day %d to day %d"),
		CurrentDay, *Ship->ShipName.ToString(), Ship->ArrivalDay, NewArrivalDay);

	RescheduleShipArrival(ShipId, NewArrivalDay);
}

bool UFreibeuterSchedulerSubsystem::EnsureDataTablesLoaded()
{
	if (!PortDataTable)
	{
		PortDataTable = LoadObject<UDataTable>(nullptr, FreibeuterTuning::PortDataTablePath);
	}
	if (!DistanceDataTable)
	{
		DistanceDataTable = LoadObject<UDataTable>(nullptr, FreibeuterTuning::DistanceDataTablePath);
	}
	if (!TraderDataTable)
	{
		TraderDataTable = LoadObject<UDataTable>(nullptr, FreibeuterTuning::TraderDataTablePath);
	}
	if (!GoodsDataTable)
	{
		GoodsDataTable = LoadObject<UDataTable>(nullptr, FreibeuterTuning::GoodsDataTablePath);
	}

	if (!PortDataTable || !DistanceDataTable || !TraderDataTable || !GoodsDataTable)
	{
		UE_LOG(LogFreibeuterScheduler, Error, TEXT("One or more Freibeuter DataTable assets are missing under /Game/Freibeuter/Data/. Aborting."));
		return false;
	}
	return true;
}

void UFreibeuterSchedulerSubsystem::ResetSimulationState()
{
	EventHeap.Reset();
	Ships.Reset();
	ShuttleLegsRemaining.Reset();
	PortRuntimeStates.Reset();
	TraderRuntimeStates.Reset();
	CurrentDay = 0;
	NextShipId = 1;
	PlayerShipId = INDEX_NONE;
}

FFreibeuterTraderRuntimeState* UFreibeuterSchedulerSubsystem::GetOrInitTraderRuntimeState(FName PortId)
{
	if (FFreibeuterTraderRuntimeState* Existing = TraderRuntimeStates.Find(PortId))
	{
		if (Existing->bInitialized)
		{
			return Existing;
		}
	}

	static const FString ContextString(TEXT("GetOrInitTraderRuntimeState"));
	const FFreibeuterTraderRow* Row = TraderDataTable ? TraderDataTable->FindRow<FFreibeuterTraderRow>(PortId, ContextString) : nullptr;
	if (!Row)
	{
		return nullptr;
	}

	FFreibeuterTraderRuntimeState& State = TraderRuntimeStates.FindOrAdd(PortId);
	State.Tier1StockRemaining = Row->Tier1Stock;
	State.Tier2StockRemaining = Row->Tier2Stock;
	State.bInitialized = true;
	return &State;
}

void UFreibeuterSchedulerSubsystem::NewGame()
{
	if (!EnsureDataTablesLoaded())
	{
		return;
	}

	ResetSimulationState();

	PlayerShipId = RegisterShip(FName(TEXT("Ranger")), FreibeuterTuning::StartingPortId);
	FFreibeuterShipState* Ship = Ships.Find(PlayerShipId);
	check(Ship);
	Ship->Gold = FreibeuterTuning::StartingGold;

	UE_LOG(LogFreibeuterScheduler, Log, TEXT("---- New game: %s starts at %s with %d Dukaten ----"),
		*Ship->ShipName.ToString(), *Ship->CurrentPortId.ToString(), Ship->Gold);

	Status();
}

void UFreibeuterSchedulerSubsystem::SetSail(FName DestinationPortId, EFreibeuterGood Good, int32 Quantity)
{
	if (PlayerShipId == INDEX_NONE)
	{
		UE_LOG(LogFreibeuterScheduler, Warning, TEXT("No active game. Run Freibeuter.NewGame first."));
		return;
	}

	FFreibeuterShipState* Ship = Ships.Find(PlayerShipId);
	check(Ship);

	if (HasPendingArrival(PlayerShipId))
	{
		UE_LOG(LogFreibeuterScheduler, Warning, TEXT("%s is already at sea, bound for %s."), *Ship->ShipName.ToString(), *Ship->DestinationPortId.ToString());
		return;
	}

	static const FString ContextString(TEXT("SetSail"));
	const FFreibeuterPortRow* PortRow = PortDataTable ? PortDataTable->FindRow<FFreibeuterPortRow>(Ship->CurrentPortId, ContextString) : nullptr;
	if (!PortRow)
	{
		UE_LOG(LogFreibeuterScheduler, Error, TEXT("No port row found for current port '%s'."), *Ship->CurrentPortId.ToString());
		return;
	}

	if (Good != PortRow->ProducesGoodA && Good != PortRow->ProducesGoodB)
	{
		UE_LOG(LogFreibeuterScheduler, Warning, TEXT("%s doesn't produce %s here -- it produces %s and %s."),
			*Ship->CurrentPortId.ToString(), *GetGoodRowName(Good).ToString(),
			*GetGoodRowName(PortRow->ProducesGoodA).ToString(), *GetGoodRowName(PortRow->ProducesGoodB).ToString());
		return;
	}

	if (!PortDataTable->FindRow<FFreibeuterPortRow>(DestinationPortId, ContextString))
	{
		UE_LOG(LogFreibeuterScheduler, Warning, TEXT("Unknown destination port '%s'."), *DestinationPortId.ToString());
		return;
	}

	const int32 ClampedQuantity = FMath::Clamp(Quantity, 1, FreibeuterTuning::BaseCargoCapacity);
	if (ClampedQuantity != Quantity)
	{
		UE_LOG(LogFreibeuterScheduler, Log, TEXT("Cargo hold capacity is %d; clamped requested %d to %d."), FreibeuterTuning::BaseCargoCapacity, Quantity, ClampedQuantity);
	}

	FFreibeuterCargoItem Item;
	Item.Good = Good;
	Item.Quantity = ClampedQuantity;

	TArray<FFreibeuterCargoItem> Cargo;
	Cargo.Add(Item);

	CommitVoyage(PlayerShipId, DestinationPortId, Cargo);
}

void UFreibeuterSchedulerSubsystem::Advance()
{
	if (PlayerShipId == INDEX_NONE)
	{
		UE_LOG(LogFreibeuterScheduler, Warning, TEXT("No active game. Run Freibeuter.NewGame first."));
		return;
	}

	if (!HasPendingArrival(PlayerShipId))
	{
		UE_LOG(LogFreibeuterScheduler, Log, TEXT("No pending voyage. Use Freibeuter.SetSail to depart."));
		return;
	}

	// Phase 0 has one ship, so its next event -- Encounter or Arrival -- is always at the heap's
	// root; popping unconditionally is safe here without inspecting ShipId first.
	FFreibeuterScheduledEvent Event;
	if (!PopNextEvent(Event))
	{
		return;
	}

	CurrentDay = Event.TriggerDay;
	ResolveEvent(Event);
	Status();
}

void UFreibeuterSchedulerSubsystem::BuyCannon()
{
	if (PlayerShipId == INDEX_NONE)
	{
		UE_LOG(LogFreibeuterScheduler, Warning, TEXT("No active game. Run Freibeuter.NewGame first."));
		return;
	}

	FFreibeuterShipState* Ship = Ships.Find(PlayerShipId);
	check(Ship);

	static const FString ContextString(TEXT("BuyCannon"));
	const FFreibeuterTraderRow* Row = TraderDataTable ? TraderDataTable->FindRow<FFreibeuterTraderRow>(Ship->CurrentPortId, ContextString) : nullptr;
	if (!Row || Row->TraderType != EFreibeuterUpgradeType::Cannon)
	{
		UE_LOG(LogFreibeuterScheduler, Warning, TEXT("There's no Cannon trader at %s."), *Ship->CurrentPortId.ToString());
		return;
	}

	FFreibeuterTraderRuntimeState* TraderState = GetOrInitTraderRuntimeState(Ship->CurrentPortId);
	check(TraderState);

	// Tier 2 is locked until every tier-1 item has sold (CLAUDE.md §4, FREIBE_1.md §4).
	const bool bBuyingTier1 = TraderState->Tier1StockRemaining > 0;
	const int32 Price = bBuyingTier1 ? Row->Tier1Price : Row->Tier2Price;
	const int32 StockRemaining = bBuyingTier1 ? TraderState->Tier1StockRemaining : TraderState->Tier2StockRemaining;

	if (StockRemaining <= 0)
	{
		UE_LOG(LogFreibeuterScheduler, Log, TEXT("The trader at %s is out of Cannons."), *Ship->CurrentPortId.ToString());
		return;
	}

	if (Ship->Gold < Price)
	{
		UE_LOG(LogFreibeuterScheduler, Log, TEXT("Can't afford the Tier-%d Cannon (%d Dukaten) -- you have %d."), bBuyingTier1 ? 1 : 2, Price, Ship->Gold);
		return;
	}

	Ship->Gold -= Price;
	if (bBuyingTier1)
	{
		--TraderState->Tier1StockRemaining;
		++Ship->CannonTier1Count;
	}
	else
	{
		--TraderState->Tier2StockRemaining;
		++Ship->CannonTier2Count;
	}

	const int32 VP = Ship->CannonTier1Count * 1 + Ship->CannonTier2Count * 2;
	UE_LOG(LogFreibeuterScheduler, Log, TEXT("Bought a Tier-%d Cannon for %d Dukaten. Gold: %d. Cannon VP: %d."),
		bBuyingTier1 ? 1 : 2, Price, Ship->Gold, VP);
}

void UFreibeuterSchedulerSubsystem::Status()
{
	if (PlayerShipId == INDEX_NONE)
	{
		UE_LOG(LogFreibeuterScheduler, Warning, TEXT("No active game. Run Freibeuter.NewGame first."));
		return;
	}

	const FFreibeuterShipState* Ship = Ships.Find(PlayerShipId);
	check(Ship);

	const int32 VP = Ship->CannonTier1Count * 1 + Ship->CannonTier2Count * 2;

	if (HasPendingArrival(PlayerShipId))
	{
		UE_LOG(LogFreibeuterScheduler, Log, TEXT("Day %d: %s at sea, %s -> %s, arriving day %d. Gold: %d. VP: %d."),
			CurrentDay, *Ship->ShipName.ToString(), *Ship->CurrentPortId.ToString(), *Ship->DestinationPortId.ToString(), Ship->ArrivalDay, Ship->Gold, VP);
	}
	else
	{
		UE_LOG(LogFreibeuterScheduler, Log, TEXT("Day %d: %s docked at %s. Gold: %d. Cannons: T1=%d T2=%d. VP: %d."),
			CurrentDay, *Ship->ShipName.ToString(), *Ship->CurrentPortId.ToString(), Ship->Gold, Ship->CannonTier1Count, Ship->CannonTier2Count, VP);
	}
}

void UFreibeuterSchedulerSubsystem::RunSchedulerDemo()
{
	if (!EnsureDataTablesLoaded())
	{
		return;
	}

	ResetSimulationState();

	static const FString ContextString(TEXT("RunSchedulerDemo"));

	for (const FName PortId : { FName("SanJuan"), FName("PuertoPlata"), FName("Curacao") })
	{
		const FFreibeuterPortRow* Row = PortDataTable->FindRow<FFreibeuterPortRow>(PortId, ContextString);
		if (!Row)
		{
			UE_LOG(LogFreibeuterScheduler, Error, TEXT("RunSchedulerDemo: missing port row '%s' in DT_FreibeuterPorts"), *PortId.ToString());
			return;
		}

		UE_LOG(LogFreibeuterScheduler, Log, TEXT("Loaded port %s (\"%s\") from DataTable: produces %s/%s, accepts %s/%s"),
			*PortId.ToString(), *Row->DisplayName.ToString(),
			*UEnum::GetValueAsString(Row->ProducesGoodA), *UEnum::GetValueAsString(Row->ProducesGoodB),
			*UEnum::GetValueAsString(Row->AcceptsGoodA), *UEnum::GetValueAsString(Row->AcceptsGoodB));
	}

	if (const FFreibeuterTraderRow* CannonTrader = TraderDataTable->FindRow<FFreibeuterTraderRow>(FName("PuertoPlata"), ContextString))
	{
		UE_LOG(LogFreibeuterScheduler, Log, TEXT("Loaded Puerto Plata's Cannons trader from DataTable: Tier1 %d @ price %d, Tier2 %d @ price %d"),
			CannonTrader->Tier1Stock, CannonTrader->Tier1Price, CannonTrader->Tier2Stock, CannonTrader->Tier2Price);
	}
	else
	{
		UE_LOG(LogFreibeuterScheduler, Error, TEXT("RunSchedulerDemo: missing trader row 'PuertoPlata' in DT_FreibeuterTraders"));
		return;
	}

	const int32 Sparrow = RegisterShip(FName("Sparrow"), FName("SanJuan"));
	const int32 Anne = RegisterShip(FName("Anne"), FName("PuertoPlata"));
	const int32 Morgan = RegisterShip(FName("Morgan"), FName("SanJuan"));
	const int32 Reyes = RegisterShip(FName("Reyes"), FName("PuertoPlata"));

	// Sparrow and Anne shuttle short 2-day hops back and forth to demonstrate more frequent
	// turns than Morgan's single long voyage.
	ShuttleLegsRemaining.Add(Sparrow, 3);
	ShuttleLegsRemaining.Add(Anne, 3);

	FFreibeuterCargoItem WheatItem;
	WheatItem.Good = EFreibeuterGood::Wheat;
	WheatItem.Quantity = 10;
	TArray<FFreibeuterCargoItem> WheatCargo;
	WheatCargo.Add(WheatItem);

	FFreibeuterCargoItem RumItem;
	RumItem.Good = EFreibeuterGood::Rum;
	RumItem.Quantity = 5;
	TArray<FFreibeuterCargoItem> RumCargo;
	RumCargo.Add(RumItem);

	FFreibeuterCargoItem TobaccoItem;
	TobaccoItem.Good = EFreibeuterGood::Tobacco;
	TobaccoItem.Quantity = 8;
	TArray<FFreibeuterCargoItem> TobaccoCargo;
	TobaccoCargo.Add(TobaccoItem);

	UE_LOG(LogFreibeuterScheduler, Log, TEXT("---- Demo scenario begins ----"));

	// Short-hop shuttles: Wheat cargo makes RollForEncounter deterministically 0%, so the real
	// (non-scripted) CommitVoyage path is used here -- no demo override needed.
	CommitVoyage(Sparrow, FName("PuertoPlata"), WheatCargo);
	CommitVoyage(Anne, FName("SanJuan"), WheatCargo);

	// Long haul, risky cargo: scripted to guarantee both an encounter and a loss, so the
	// arrival-delay path is always observable on every run (per the confirmed demo-determinism
	// choice). The resolution code path itself is identical to the real one.
	CommitVoyageInternal(Morgan, FName("Curacao"), RumCargo, /*bEncounterOccurs=*/true, /*ForcedEncounterOutcome=*/0);

	// Medium voyage, risky cargo, real (non-scripted) roll -- may or may not meet pirates.
	CommitVoyage(Reyes, FName("Curacao"), TobaccoCargo);

	RunSchedulerToCompletion();

	UE_LOG(LogFreibeuterScheduler, Log, TEXT("---- Demo scenario complete ----"));
}
