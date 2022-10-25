// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "RosSettings.generated.h"

/**
 *
 */
UCLASS(Config=Ros, defaultconfig, meta = (DisplayName="Ros Settings"))
class UROSBRIDGE_API URosSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "General")
      FString ROSBridgeServerHost;

    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "General")
      int32 ROSBridgeServerPort;

    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "General")
      bool bConnectToROS = true;

};
