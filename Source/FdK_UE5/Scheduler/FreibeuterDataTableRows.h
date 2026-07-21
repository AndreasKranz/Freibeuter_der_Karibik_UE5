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
 * One upgrade trader. Phase 0 only populates a single Cannon row; tier-2 unlock logic is Phase 1
 * (CLAUDE.md §3). Not read by the scheduler demo -- included to prove the data-driven pattern.
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
