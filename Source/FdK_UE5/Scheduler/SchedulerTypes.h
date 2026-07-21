// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SchedulerTypes.generated.h"

/** Tradeable goods, ascending value & pirate risk (CLAUDE.md §4). */
UENUM(BlueprintType)
enum class EFreibeuterGood : uint8
{
	Wheat,
	Wood,
	Tobacco,
	Rum
};

/** The two event kinds this Phase 0 proof schedules per voyage (CLAUDE.md §2.3, Option A). */
UENUM(BlueprintType)
enum class EFreibeuterEventType : uint8
{
	Arrival,
	Encounter
};

/** Ship upgrade categories, each sold by one trader type (CLAUDE.md §4). Phase 0 only exercises Cannon. */
UENUM(BlueprintType)
enum class EFreibeuterUpgradeType : uint8
{
	Cannon,
	Chest,
	Sail,
	Sailor,
	Residence
};

USTRUCT(BlueprintType)
struct FFreibeuterCargoItem
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Freibeuter")
	EFreibeuterGood Good = EFreibeuterGood::Wheat;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Freibeuter")
	int32 Quantity = 0;
};

/**
 * Live per-ship simulation state. This is runtime state, not tunable design data,
 * so unlike Port/Trader it is not DataTable-backed (CLAUDE.md §2.2 covers tunable data only).
 */
USTRUCT(BlueprintType)
struct FFreibeuterShipState
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Freibeuter")
	int32 ShipId = INDEX_NONE;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Freibeuter")
	FName ShipName;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Freibeuter")
	FName CurrentPortId;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Freibeuter")
	FName DestinationPortId;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Freibeuter")
	int32 DepartureDay = 0;

	/**
	 * Authoritative arrival day. Losing an Encounter mutates this directly (CLAUDE.md §2.3
	 * "modifies the already-scheduled arrival day"); the matching heap entry is then kept in
	 * sync by UFreibeuterSchedulerSubsystem::RescheduleShipArrival.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Freibeuter")
	int32 ArrivalDay = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Freibeuter")
	TArray<FFreibeuterCargoItem> Cargo;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Freibeuter")
	int32 Gold = 0;

	/** Phase 0 only has the Cannons trader, so these are tracked directly rather than via a generic per-upgrade-type map (CLAUDE.md: don't add abstraction beyond what's needed). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Freibeuter")
	int32 CannonTier1Count = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Freibeuter")
	int32 CannonTier2Count = 0;
};

/**
 * Runtime scarcity tracker for one port: the last day each good was delivered there.
 * CLAUDE.md §4 data model: Port.lastDeliveryDay[Good] drives the scarcity/starvation payout bonus.
 * This is session state, not design data, so it lives alongside ships, not in the Port DataTable.
 */
USTRUCT()
struct FFreibeuterPortRuntimeState
{
	GENERATED_BODY()

	UPROPERTY()
	TMap<EFreibeuterGood, int32> LastDeliveryDay;
};

/**
 * Runtime stock tracker for one port's trader, seeded from FFreibeuterTraderRow at NewGame but
 * decremented independently -- mutating the DataTable row itself would corrupt the design asset
 * with per-playthrough state.
 */
USTRUCT()
struct FFreibeuterTraderRuntimeState
{
	GENERATED_BODY()

	UPROPERTY()
	int32 Tier1StockRemaining = 0;

	UPROPERTY()
	int32 Tier2StockRemaining = 0;

	UPROPERTY()
	bool bInitialized = false;
};

/** One entry in the scheduler's min-heap, keyed on TriggerDay (CLAUDE.md §2.3). */
USTRUCT()
struct FFreibeuterScheduledEvent
{
	GENERATED_BODY()

	UPROPERTY()
	EFreibeuterEventType EventType = EFreibeuterEventType::Arrival;

	UPROPERTY()
	int32 TriggerDay = 0;

	UPROPERTY()
	int32 ShipId = INDEX_NONE;

	/** Demo-only override for Encounter events: -1 = roll normally, 0 = force a loss, 1 = force a win. Real gameplay code should never set this to anything but -1. */
	UPROPERTY()
	int32 DebugForcedOutcome = -1;
};
