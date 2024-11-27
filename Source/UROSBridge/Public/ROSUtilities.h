
#pragma once

#include "ROSUtilities.generated.h"

USTRUCT(BlueprintType)
struct FROSTopic
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ROS")
	FString Topic;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ROS")
	FString Type;

	int32 ID;

	FROSTopic(FString InTopic = "", FString InType = "") : Topic(InTopic), Type(InType){};
};

USTRUCT()
struct FGoalStatusInfo
{
    GENERATED_BODY()
public:
	FGoalStatusInfo(){};

	FGoalStatusInfo(FString InId, uint32 InSecs, uint32 InNSecs, uint8 InStatus = 0, FString InText = ""): Id(InId),
		   Secs(InSecs),
		   NSecs(InNSecs),
		   Status(InStatus),
		   Text(InText){};

	FString Id;
	uint32 Secs;
	uint32 NSecs;
	uint8 Status;
	FString Text;

};
