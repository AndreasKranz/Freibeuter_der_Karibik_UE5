// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "SchedulerTypes.h"
#include "FreibeuterDataTableRows.generated.h"

/**
 * One port. Row name is the port's PortId (also used as the FName ports/ships refer to elsewhere).
 * CLAUDE.md §2.2: this is tunable design data, so it lives in a DataTable, never hardcoded C++.
 */
USTRUCT(BlueprintType)
struct FFreibeuterPortRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Freibeuter")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Freibeuter")
	EFreibeuterGood ProducesGoodA = EFreibeuterGood::Wheat;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Freibeuter")
	EFreibeuterGood ProducesGoodB = EFreibeuterGood::Wood;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Freibeuter")
	EFreibeuterGood AcceptsGoodA = EFreibeuterGood::Tobacco;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Freibeuter")
	EFreibeuterGood AcceptsGoodB = EFreibeuterGood::Rum;
};

/**
 * Distance between an ordered pair of ports, in days. Row name convention: "<PortA>_<PortB>".
 * Both directions are stored as separate rows so lookup never needs to guess symmetry.
 */
USTRUCT(BlueprintType)
struct FFreibeuterPortDistanceRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Freibeuter")
	FName PortA;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Freibeuter")
	FName PortB;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Freibeuter")
	int32 DistanceDays = 1;
};

/**
 * One upgrade trader. The dossier (FREIBE_1.md §3) fixes trader type per port, so the row name is
 * the owning PortId, not the upgrade type. Only Puerto Plata (Cannons) is populated for Phase 0's
 * one-trader scope; the other 4 real traders are Phase 1 (all 5 traders, tier unlocks).
 */
USTRUCT(BlueprintType)
struct FFreibeuterTraderRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Freibeuter")
	EFreibeuterUpgradeType TraderType = EFreibeuterUpgradeType::Cannon;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Freibeuter")
	int32 Tier1Price = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Freibeuter")
	int32 Tier1Stock = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Freibeuter")
	int32 Tier2Price = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Freibeuter")
	int32 Tier2Stock = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Freibeuter")
	int32 DaysUnsold = 0;
};

/**
 * Per-good economy tuning: base sale value and pirate-encounter odds. Row name is the good's enum
 * entry name ("Wheat", "Wood", "Tobacco", "Rum"). Both fields are unrecovered numbers (FREIBE_1.md
 * §10 items 3 and 5) -- moving them here (rather than switch-statements in C++) is what makes them
 * retunable without recompiling, per CLAUDE.md §2.2.
 */
USTRUCT(BlueprintType)
struct FFreibeuterGoodRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Freibeuter")
	int32 BaseValuePerUnit = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Freibeuter")
	float EncounterChance = 0.f;
};
