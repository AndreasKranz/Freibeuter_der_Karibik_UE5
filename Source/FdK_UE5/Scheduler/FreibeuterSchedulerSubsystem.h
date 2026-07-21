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

	int32 RegisterShip(FName ShipName, FName StartPortId);
	bool EnsureDemoDataTablesLoaded();

	void HandleRunSchedulerDemoCommand(const TArray<FString>& Args, UWorld* InWorld);

	UPROPERTY(Transient)
	TArray<FFreibeuterScheduledEvent> EventHeap;

	UPROPERTY(Transient)
	TMap<int32, FFreibeuterShipState> Ships;

	/** Demo-only: ships with legs remaining here immediately re-commit a return voyage on arrival, to make short-hop interleaving visible against one long voyage. */
	UPROPERTY(Transient)
	TMap<int32, int32> ShuttleLegsRemaining;

	UPROPERTY(Transient)
	TObjectPtr<UDataTable> PortDataTable;

	UPROPERTY(Transient)
	TObjectPtr<UDataTable> DistanceDataTable;

	UPROPERTY(Transient)
	TObjectPtr<UDataTable> TraderDataTable;

	int32 CurrentDay = 0;
	int32 NextShipId = 1;

	/** Non-owning handle to the registered console command; the console command system owns the object itself. */
	IConsoleObject* RunSchedulerDemoCommandHandle = nullptr;
};
