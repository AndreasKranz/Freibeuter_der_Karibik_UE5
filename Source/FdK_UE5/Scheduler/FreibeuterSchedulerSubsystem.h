// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "SchedulerTypes.h"
#include "FreibeuterSchedulerSubsystem.generated.h"

class UDataTable;
class IConsoleObject;

/**
 * Phase 0 proof of the day-based scheduler (CLAUDE.md §2.3): a min-heap of
 * FFreibeuterScheduledEvent keyed on TriggerDay, covering every ship's Arrival and (Option A)
 * Encounter events in one shared priority queue. Never advanced from Tick -- callers pop and
 * resolve events explicitly; "now" is whatever TriggerDay was last popped.
 *
 * A TObjectPtr<> is a UPROPERTY-trackable pointer the garbage collector can see and null out if
 * the referenced object is destroyed; raw pointers below (IConsoleObject) are used instead where
 * the pointee isn't a UObject and this subsystem doesn't own its lifetime.
 */
UCLASS()
class UFreibeuterSchedulerSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/**
	 * Commits a ship to a voyage: schedules its Arrival, and rolls once for a pirate encounter,
	 * pushing a separate Encounter event partway through if the roll succeeds (CLAUDE.md §2.3,
	 * Option A). This is the real entry point future gameplay code should call -- it uses RNG.
	 */
	UFUNCTION(BlueprintCallable, Category = "Freibeuter|Scheduler")
	void CommitVoyage(int32 ShipId, FName DestinationPortId, const TArray<FFreibeuterCargoItem>& Cargo);

	/** Pops and resolves every pending event in TriggerDay order until the heap is empty. */
	UFUNCTION(BlueprintCallable, Category = "Freibeuter|Scheduler")
	void RunSchedulerToCompletion();

	/** Loads the Phase 0 placeholder DataTables and runs a scripted multi-ship demo scenario end to end. */
	UFUNCTION(BlueprintCallable, Category = "Freibeuter|Scheduler")
	void RunSchedulerDemo();

	/** Starts a fresh Phase 0 playthrough: one player ship, full gold, at San Juan (CLAUDE.md §3/§4). */
	UFUNCTION(BlueprintCallable, Category = "Freibeuter|TradeLoop")
	void NewGame();

	/** Loads cargo (must be one of the current port's two produced goods) and commits the player ship to a voyage. */
	UFUNCTION(BlueprintCallable, Category = "Freibeuter|TradeLoop")
	void SetSail(FName DestinationPortId, EFreibeuterGood Good, int32 Quantity);

	/** Pops and resolves exactly the player ship's next pending event (Encounter or Arrival), so a human can step through a voyage. */
	UFUNCTION(BlueprintCallable, Category = "Freibeuter|TradeLoop")
	void Advance();

	/** Buys the cheapest currently-available Cannon tier (tier 2 only once tier 1 is exhausted) from the port the player ship is docked at. */
	UFUNCTION(BlueprintCallable, Category = "Freibeuter|TradeLoop")
	void BuyCannon();

	/** Logs the player ship's day, location, cargo, gold, and VP. */
	UFUNCTION(BlueprintCallable, Category = "Freibeuter|TradeLoop")
	void Status();

private:
	/** Shared voyage-commit path. ForcedEncounterOutcome lets the demo script a guaranteed win/loss (-1 = roll normally, 0 = force loss, 1 = force win); real gameplay code should never pass anything but -1. */
	void CommitVoyageInternal(int32 ShipId, FName DestinationPortId, const TArray<FFreibeuterCargoItem>& Cargo, bool bEncounterOccurs, int32 ForcedEncounterOutcome = -1);

	void ResolveEvent(const FFreibeuterScheduledEvent& Event);
	void ResolveArrival(const FFreibeuterScheduledEvent& Event);
	void ResolveEncounterInternal(int32 ShipId, bool bPlayerWins);

	void PushEvent(const FFreibeuterScheduledEvent& Event);
	bool PopNextEvent(FFreibeuterScheduledEvent& OutEvent);

	/**
	 * Finds the ship's Arrival event already sitting in the heap and mutates its TriggerDay in
	 * place, then restores the heap invariant. CLAUDE.md §2.3 is explicit that a lost encounter
	 * "modifies the already-scheduled arrival day" rather than queuing a brand new arrival.
	 */
	void RescheduleShipArrival(int32 ShipId, int32 NewArrivalDay);

	int32 GetDistanceDays(FName FromPortId, FName ToPortId) const;
	bool RollForEncounter(const TArray<FFreibeuterCargoItem>& Cargo) const;

	/** Row name a good is stored under in the Goods DataTable -- also its enum entry name ("Wheat", "Wood", "Tobacco", "Rum"). */
	static FName GetGoodRowName(EFreibeuterGood Good);
	int32 GetBaseValuePerUnit(EFreibeuterGood Good) const;

	/** Case-insensitive match against the four good names; false if Args[Index] isn't one of them. */
	static bool ParseGood(const FString& Text, EFreibeuterGood& OutGood);

	int32 RegisterShip(FName ShipName, FName StartPortId);
	bool EnsureDataTablesLoaded();
	void ResetSimulationState();
	bool HasPendingArrival(int32 ShipId) const;

	/** Sells a ship's cargo on arrival: base value x distance x scarcity per item (CLAUDE.md §4), then clears the cargo and records the delivery. */
	void SellCargoOnArrival(FFreibeuterShipState& Ship);

	/** Lazily seeds a trader's runtime stock from its DataTable row the first time it's touched. */
	FFreibeuterTraderRuntimeState* GetOrInitTraderRuntimeState(FName PortId);

	void HandleRunSchedulerDemoCommand(const TArray<FString>& Args, UWorld* InWorld);
	void HandleNewGameCommand(const TArray<FString>& Args, UWorld* InWorld);
	void HandleSetSailCommand(const TArray<FString>& Args, UWorld* InWorld);
	void HandleAdvanceCommand(const TArray<FString>& Args, UWorld* InWorld);
	void HandleBuyCannonCommand(const TArray<FString>& Args, UWorld* InWorld);
	void HandleStatusCommand(const TArray<FString>& Args, UWorld* InWorld);

	UPROPERTY(Transient)
	TArray<FFreibeuterScheduledEvent> EventHeap;

	UPROPERTY(Transient)
	TMap<int32, FFreibeuterShipState> Ships;

	/** Demo-only: ships with legs remaining here immediately re-commit a return voyage on arrival, to make short-hop interleaving visible against one long voyage. */
	UPROPERTY(Transient)
	TMap<int32, int32> ShuttleLegsRemaining;

	UPROPERTY(Transient)
	TMap<FName, FFreibeuterPortRuntimeState> PortRuntimeStates;

	UPROPERTY(Transient)
	TMap<FName, FFreibeuterTraderRuntimeState> TraderRuntimeStates;

	UPROPERTY(Transient)
	TObjectPtr<UDataTable> PortDataTable;

	UPROPERTY(Transient)
	TObjectPtr<UDataTable> DistanceDataTable;

	UPROPERTY(Transient)
	TObjectPtr<UDataTable> TraderDataTable;

	UPROPERTY(Transient)
	TObjectPtr<UDataTable> GoodsDataTable;

	int32 CurrentDay = 0;
	int32 NextShipId = 1;

	/** The single ship the interactive Freibeuter.* commands operate on; INDEX_NONE until NewGame runs. */
	int32 PlayerShipId = INDEX_NONE;

	/** Non-owning handles to the registered console commands; the console command system owns the objects themselves. */
	TArray<IConsoleObject*> RegisteredConsoleCommands;
};
