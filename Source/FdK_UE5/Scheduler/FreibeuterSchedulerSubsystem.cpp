// Copyright Epic Games, Inc. All Rights Reserved.

#include "FreibeuterSchedulerSubsystem.h"

#include "FreibeuterDataTableRows.h"
#include "FreibeuterSchedulerLog.h"
#include "HAL/IConsoleManager.h"

namespace FreibeuterSchedulerDemo
{
	// Phase 0 placeholder DataTable assets. Real numbers are still open per CLAUDE.md §6.
	static const TCHAR* PortDataTablePath = TEXT("/Game/Freibeuter/Data/DT_FreibeuterPorts.DT_FreibeuterPorts");
	static const TCHAR* DistanceDataTablePath = TEXT("/Game/Freibeuter/Data/DT_FreibeuterPortDistances.DT_FreibeuterPortDistances");
	static const TCHAR* TraderDataTablePath = TEXT("/Game/Freibeuter/Data/DT_FreibeuterTraders.DT_FreibeuterTraders");

	static bool DayAscending(const FFreibeuterScheduledEvent& A, const FFreibeuterScheduledEvent& B)
	{
		return A.TriggerDay < B.TriggerDay;
	}
}

void UFreibeuterSchedulerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	RunSchedulerDemoCommandHandle = IConsoleManager::Get().RegisterConsoleCommand(
		TEXT("Freibeuter.RunSchedulerDemo"),
		TEXT("Runs the Phase 0 day-based scheduler proof-of-concept and logs interleaved event resolution to LogFreibeuterScheduler."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateUObject(this, &UFreibeuterSchedulerSubsystem::HandleRunSchedulerDemoCommand),
		ECVF_Default);
}

void UFreibeuterSchedulerSubsystem::Deinitialize()
{
	if (RunSchedulerDemoCommandHandle)
	{
		IConsoleManager::Get().UnregisterConsoleObject(RunSchedulerDemoCommandHandle);
		RunSchedulerDemoCommandHandle = nullptr;
	}

	Super::Deinitialize();
}

void UFreibeuterSchedulerSubsystem::HandleRunSchedulerDemoCommand(const TArray<FString>& Args, UWorld* InWorld)
{
	RunSchedulerDemo();
}

void UFreibeuterSchedulerSubsystem::PushEvent(const FFreibeuterScheduledEvent& Event)
{
	EventHeap.HeapPush(Event, &FreibeuterSchedulerDemo::DayAscending);
}

bool UFreibeuterSchedulerSubsystem::PopNextEvent(FFreibeuterScheduledEvent& OutEvent)
{
	if (EventHeap.Num() == 0)
	{
		return false;
	}
	EventHeap.HeapPop(OutEvent, &FreibeuterSchedulerDemo::DayAscending);
	return true;
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
	EventHeap.Heapify(&FreibeuterSchedulerDemo::DayAscending);
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

bool UFreibeuterSchedulerSubsystem::RollForEncounter(const TArray<FFreibeuterCargoItem>& Cargo) const
{
	// Placeholder occurrence odds -- CLAUDE.md §6.5 flags the real combat/encounter formula as an
	// unknown to reconstruct from the original game. Wheat/Wood are never attacked (CLAUDE.md §4);
	// Rum is riskier than Tobacco.
	float EncounterChance = 0.f;
	for (const FFreibeuterCargoItem& Item : Cargo)
	{
		if (Item.Good == EFreibeuterGood::Rum)
		{
			EncounterChance = FMath::Max(EncounterChance, 0.5f);
		}
		else if (Item.Good == EFreibeuterGood::Tobacco)
		{
			EncounterChance = FMath::Max(EncounterChance, 0.3f);
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
			// Placeholder 50/50 until the real combat formula (CLAUDE.md §6.5) is reconstructed.
			// Phase 1's UI will surface this probability to the player before they commit (§5.1);
			// there's no player-choice step yet, so Phase 0 auto-resolves.
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

	static constexpr int32 EncounterDelayDays = 2;
	const int32 NewArrivalDay = Ship->ArrivalDay + EncounterDelayDays;

	UE_LOG(LogFreibeuterScheduler, Log, TEXT("Day %d: %s LOSES the encounter -- arrival delayed from day %d to day %d"),
		CurrentDay, *Ship->ShipName.ToString(), Ship->ArrivalDay, NewArrivalDay);

	RescheduleShipArrival(ShipId, NewArrivalDay);
}

bool UFreibeuterSchedulerSubsystem::EnsureDemoDataTablesLoaded()
{
	if (!PortDataTable)
	{
		PortDataTable = LoadObject<UDataTable>(nullptr, FreibeuterSchedulerDemo::PortDataTablePath);
	}
	if (!DistanceDataTable)
	{
		DistanceDataTable = LoadObject<UDataTable>(nullptr, FreibeuterSchedulerDemo::DistanceDataTablePath);
	}
	if (!TraderDataTable)
	{
		TraderDataTable = LoadObject<UDataTable>(nullptr, FreibeuterSchedulerDemo::TraderDataTablePath);
	}

	if (!PortDataTable || !DistanceDataTable || !TraderDataTable)
	{
		UE_LOG(LogFreibeuterScheduler, Error, TEXT("RunSchedulerDemo: one or more Freibeuter DataTable assets are missing under /Game/Freibeuter/Data/. Aborting."));
		return false;
	}
	return true;
}

void UFreibeuterSchedulerSubsystem::RunSchedulerDemo()
{
	if (!EnsureDemoDataTablesLoaded())
	{
		return;
	}

	EventHeap.Reset();
	Ships.Reset();
	ShuttleLegsRemaining.Reset();
	CurrentDay = 0;
	NextShipId = 1;

	static const FString ContextString(TEXT("RunSchedulerDemo"));

	for (const FName PortId : { FName("Nassau"), FName("Tortuga"), FName("Havana") })
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

	if (const FFreibeuterTraderRow* CannonTrader = TraderDataTable->FindRow<FFreibeuterTraderRow>(FName("Cannons"), ContextString))
	{
		UE_LOG(LogFreibeuterScheduler, Log, TEXT("Loaded Cannons trader from DataTable: Tier1 %d @ price %d, Tier2 %d @ price %d"),
			CannonTrader->Tier1Stock, CannonTrader->Tier1Price, CannonTrader->Tier2Stock, CannonTrader->Tier2Price);
	}
	else
	{
		UE_LOG(LogFreibeuterScheduler, Error, TEXT("RunSchedulerDemo: missing trader row 'Cannons' in DT_FreibeuterTraders"));
		return;
	}

	const int32 Sparrow = RegisterShip(FName("Sparrow"), FName("Nassau"));
	const int32 Anne = RegisterShip(FName("Anne"), FName("Tortuga"));
	const int32 Morgan = RegisterShip(FName("Morgan"), FName("Nassau"));
	const int32 Reyes = RegisterShip(FName("Reyes"), FName("Tortuga"));

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
	CommitVoyage(Sparrow, FName("Tortuga"), WheatCargo);
	CommitVoyage(Anne, FName("Nassau"), WheatCargo);

	// Long haul, risky cargo: scripted to guarantee both an encounter and a loss, so the
	// arrival-delay path is always observable on every run (per the confirmed demo-determinism
	// choice). The resolution code path itself is identical to the real one.
	CommitVoyageInternal(Morgan, FName("Havana"), RumCargo, /*bEncounterOccurs=*/true, /*ForcedEncounterOutcome=*/0);

	// Medium voyage, risky cargo, real (non-scripted) roll -- may or may not meet pirates.
	CommitVoyage(Reyes, FName("Havana"), TobaccoCargo);

	RunSchedulerToCompletion();

	UE_LOG(LogFreibeuterScheduler, Log, TEXT("---- Demo scenario complete ----"));
}
