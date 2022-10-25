// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "ROSBridgeHandler.h"
#include "ROSSubsystem.generated.h"

UINTERFACE(MinimalAPI)
class URosInterface : public UInterface
{
    GENERATED_BODY()
};

class UROSBRIDGE_API IRosInterface
{
  GENERATED_BODY()

public:

    TSharedPtr<FROSBridgeHandler> ROSHandler;

  virtual void Connect(const FString& InIp, const int32& InPort);
  virtual void Disconnect();

};

/**
   Abstract Subsystem that lives with the GameInstance and is tickable.
 */
UCLASS(Abstract)
class UROSBRIDGE_API UROSGameInstanceSubsystem : public UGameInstanceSubsystem, public FTickableGameObject, public IRosInterface
{
	GENERATED_BODY()

public:
    // Begin USubsystem
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
    // End USubsystem
protected:

    /* FTickableGameObject interface */
    virtual void Tick(float DeltaTime) override;
    virtual bool IsTickable() const override;
    virtual TStatId GetStatId() const override;
};

/**
   Abstract Subsystem that lives with the UWorld and is tickable.
 */
UCLASS(Abstract)
class UROSBRIDGE_API UROSWorldSubsystem : public UTickableWorldSubsystem, public IRosInterface
{
	GENERATED_BODY()

public:
    // Begin USubsystem
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
    // End USubsystem

protected:

    /* FTickableGameObject interface */
    virtual void Tick(float DeltaTime) override;
    virtual bool IsTickable() const override;
    virtual TStatId GetStatId() const override;
};
