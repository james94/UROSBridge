#include "IROSBridgePlugin.h"
#include "Core.h"
#include "Modules/ModuleManager.h"
#include "Networking.h"
#include "Json.h"
#include "ROSBridgeHandler.h"

static void CallbackOnConnection(FROSBridgeHandler* Handler)
{
    UE_LOG(LogTemp, Warning, TEXT("Websocket Connected."));
    Handler->SetClientConnected(true);
}

static void CallbackOnError()
{
    UE_LOG(LogTemp, Warning, TEXT("Error in Websocket."));
}

// Create connection, bind functions to WebSocket Client, and Connect.
bool FROSBridgeHandler::FROSBridgeHandlerRunnable::Init()
{
    UE_LOG(LogTemp, Log, TEXT("FROSBridgeHandlerRunnable Thread Init()"));

    FIPv4Address IPAddress;
    FIPv4Address::Parse(Handler->GetHost(), IPAddress);

    FIPv4Endpoint Endpoint(IPAddress, Handler->GetPort());
    Handler->Client = new FWebSocket(Endpoint.ToInternetAddr().Get());

    // Bind Received callback
    FWebsocketPacketRecievedCallBack ReceivedCallback;
    ReceivedCallback.BindRaw(this->Handler, &FROSBridgeHandler::OnMessage);
    Handler->Client->SetRecieveCallBack(ReceivedCallback);

    // Bind Connected callback
    FWebsocketInfoCallBack ConnectedCallback;
    ConnectedCallback.BindStatic(&CallbackOnConnection, this->Handler);
    Handler->Client->SetConnectedCallBack(ConnectedCallback);

    // Bind Error callback
    FWebsocketInfoCallBack ErrorCallback;
    ErrorCallback.BindStatic(&CallbackOnError);
    Handler->Client->SetErrorCallBack(ErrorCallback);

    Handler->Client->Connect();
    return true;
}

// Process subscribed messages
uint32 FROSBridgeHandler::FROSBridgeHandlerRunnable::Run()
{
    while (StopCounter.GetValue() == 0){
        if (Handler->Client)
            Handler->Client->Tick();
        FPlatformProcess::Sleep(Handler->GetClientInterval());
    }
    UE_LOG(LogTemp, Log, TEXT("Run() function stopped. "));
    return 0;
}

// set the stop counter and disconnect
void FROSBridgeHandler::FROSBridgeHandlerRunnable::Stop()
{
    UE_LOG(LogTemp, Log, TEXT("Stop() function. "));
    StopCounter.Increment();
}


// Callback function when message comes from WebSocket
void FROSBridgeHandler::OnMessage(void* data, int32 length)
{
    char * CharMessage = new char [length + 1];
    memcpy(CharMessage, data, length);
    CharMessage[length] = 0;

    FString JsonMessage = UTF8_TO_TCHAR(CharMessage);
    UE_LOG(LogTemp, Error, TEXT("Json Message: %s"), *JsonMessage);

    // Parse Json Message Here
    TSharedRef< TJsonReader<> > Reader =
            TJsonReaderFactory<>::Create(JsonMessage);
    TSharedPtr< FJsonObject > JsonObject;
    bool DeserializeState = FJsonSerializer::Deserialize(Reader, JsonObject);
    if (!DeserializeState)
    {
        UE_LOG(LogTemp, Error, TEXT("Deserialization Error. "));
        return;
    }

    FString Op = JsonObject->GetStringField(TEXT("op"));

    if (Op == TEXT("publish")) // Message 
    {
        FString Topic = JsonObject->GetStringField(TEXT("topic"));
        UE_LOG(LogTemp, Error, TEXT("Received, Topic: %s. "), *Topic);

        FString Data = JsonObject->GetObjectField(TEXT("msg"))->GetStringField(TEXT("data"));
        UE_LOG(LogTemp, Error, TEXT("Received, Data: %s. "), *Data);

        TSharedPtr< FJsonObject > MsgObject = JsonObject->GetObjectField(TEXT("msg"));

        // Find corresponding subscriber
        bool IsTopicFound = false;
        FROSBridgeSubscriber* Subscriber = NULL;
        for (int i = 0; i < ListSubscribers.Num(); i++)
        {
            if (ListSubscribers[i]->GetMessageTopic() == Topic)
            {
                Subscriber = ListSubscribers[i];
                UE_LOG(LogTemp, Error, TEXT("Subscriber Found. Id = %d. "), i);
                IsTopicFound = true; break;
            }
        }

        if (!IsTopicFound)
        {
            UE_LOG(LogTemp, Error, TEXT("Error: Topic %s not Found. "), *Topic);
        }
        else
        {
            FROSBridgeMsg* ROSBridgeMsg;
            ROSBridgeMsg = Subscriber->ParseMessage(MsgObject);
            UE_LOG(LogTemp, Error, TEXT("Parse Finished. "));

            FRenderTask* RenderTask = new FRenderTask(Subscriber, Topic, ROSBridgeMsg);
            UE_LOG(LogTemp, Error, TEXT("New FRenderTask. "));

            QueueTask.Enqueue(RenderTask);
        }
    }
    else if (Op == TEXT("service_response"))
    {
        FString ID = JsonObject->GetStringField(TEXT("id"));
        FString ServiceName = JsonObject->GetStringField(TEXT("service"));
        TSharedPtr< FJsonObject > ValuesObj; 
        if (JsonObject->HasField("values")) // has values
            ValuesObj = JsonObject->GetObjectField(TEXT("values"));
        else
            ValuesObj = MakeShareable(new FJsonObject);

        bool bFoundService = false; 
        int FoundServiceIndex; 
        LockArrayService.Lock(); // lock mutex, when access ArrayService
        for (int i = 0; i < ArrayService.Num(); i++)
        {
            if (ArrayService[i]->Name == ServiceName &&
                ArrayService[i]->ID == ID)
            {
                ArrayService[i]->bIsResponsed = true; 
                check(ArrayService[i]->Response.IsValid());
                ArrayService[i]->Response->FromJson(ValuesObj);
                bFoundService = true; 
                FoundServiceIndex = i;
            }
        }
        LockArrayService.Unlock(); // unlock mutex

        if (!bFoundService)
        {
            UE_LOG(LogTemp, Error, TEXT("Error: Service Name [%s] Id [%s] not found. "), *ServiceName, *ID); 
        } 
    }
    else if (Op == "call_service")
    {
        FString ID = JsonObject->GetStringField(TEXT("id")); 
        // there is always an Id for rosbridge_server generated service call
        FString ServiceName = JsonObject->GetStringField(TEXT("service"));
        TSharedPtr< FJsonObject > ArgsObj;
        if (JsonObject->HasField("args")) // has values
            ArgsObj = JsonObject->GetObjectField(TEXT("args"));
        else
            ArgsObj = MakeShareable(new FJsonObject);

        // call service in block mode
        bool bFoundService = false;
        int FoundServiceIndex;
        for (int i = 0; i < ListServiceServer.Num(); i++)
            if (ListServiceServer[i]->GetName() == ServiceName)
            {
                bFoundService = true;
                FoundServiceIndex = i; 
                break; 
            }

        if (!bFoundService)
        {
            UE_LOG(LogTemp, Error, TEXT("Error: Service Name [%s] Id [%s] not found. "), *ServiceName, *ID);
        }
        else
        {
            UE_LOG(LogTemp, Log, TEXT("Info: Service Name [%s] Id [%s] found, calling callback function."), *ServiceName, *ID);
            TSharedPtr<FROSBridgeSrv::SrvRequest> Request = ListServiceServer[FoundServiceIndex]->FromJson(ArgsObj); 
            TSharedPtr<FROSBridgeSrv::SrvResponse > Response = ListServiceServer[FoundServiceIndex]->CallBack(Request); // block 
            PublishServiceResponse(ServiceName, ID, Response); 
        }
    }

    delete [] CharMessage;
    UE_LOG(LogTemp, Error, TEXT("OnMessage End. "));
}

// Create runnable instance and run the thread;
void FROSBridgeHandler::Connect()
{
    Runnable = //  MakeShareable(
        new FROSBridgeHandlerRunnable(this);
    // );
    Thread = FRunnableThread::Create(Runnable, TEXT("ROSBridgeHandlerRunnable"),
                                     0, TPri_BelowNormal);
    while (!IsClientConnected())
        FPlatformProcess::Sleep(0.01);

    // Subscribe all topics
    UE_LOG(LogTemp, Log, TEXT("Subscribe all topics. "));
    for (int i = 0; i < ListSubscribers.Num(); i++)
    {
        UE_LOG(LogTemp, Warning, TEXT("Subscribing Topic %s"), *ListSubscribers[i]->GetMessageTopic());
        FString WebSocketMessage = FROSBridgeMsg::Subscribe(ListSubscribers[i]->GetMessageTopic());
        Client->Send(WebSocketMessage);
    }

    // Advertise all topics
    UE_LOG(LogTemp, Log, TEXT("Advertise all topics. "));
    for (int i = 0; i < ListPublishers.Num(); i++)
    {
        UE_LOG(LogTemp, Warning, TEXT("Advertising Topic %s"), *ListPublishers[i]->GetMessageTopic());
        FString WebSocketMessage = FROSBridgeMsg::Advertise(ListPublishers[i]->GetMessageTopic(),
                                                            ListPublishers[i]->GetMessageType());
        Client->Send(WebSocketMessage);
    }

    // Advertise all service servers
    for (int i = 0; i < ListServiceServer.Num(); i++)
    {
        UE_LOG(LogTemp, Warning, TEXT("Advertising Service [%s]"), *ListServiceServer[i]->GetName()); 
        FString WebSocketMessage = FROSBridgeSrv::AdvertiseService(ListServiceServer[i]->GetName(), 
                                                                   ListServiceServer[i]->GetType());
        Client->Send(WebSocketMessage); 
    }
}

// Unsubscribe / Unadvertise all topics
// Stop the thread
void FROSBridgeHandler::Disconnect()
{
    // Unsubscribe all topics
    UE_LOG(LogTemp, Log, TEXT("Unsubscribe all topics. "));
    for (int i = 0; i < ListSubscribers.Num(); i++)
    {
        UE_LOG(LogTemp, Warning, TEXT("Unsubscribing Topic %s"), *ListSubscribers[i]->GetMessageTopic());
        FString WebSocketMessage = FROSBridgeMsg::UnSubscribe(ListSubscribers[i]->GetMessageTopic());
        Client->Send(WebSocketMessage);
    }

    // Unadvertise all topics
    UE_LOG(LogTemp, Log, TEXT("Unadvertise all topics. "));
    for (int i = 0; i < ListPublishers.Num(); i++)
    {
        UE_LOG(LogTemp, Warning, TEXT("Unadvertising Topic %s"), *ListPublishers[i]->GetMessageTopic());
        FString WebSocketMessage = FROSBridgeMsg::UnAdvertise(ListPublishers[i]->GetMessageTopic());
        Client->Send(WebSocketMessage);
    }

    // Unadvertise all service servers
    for (int i = 0; i < ListServiceServer.Num(); i++)
    {
        UE_LOG(LogTemp, Warning, TEXT("Unadvertising Service [%s]"), *ListServiceServer[i]->GetName());
        FString WebSocketMessage = FROSBridgeSrv::UnadvertiseService(ListServiceServer[i]->GetName());
        Client->Send(WebSocketMessage);
    }

	Client->Flush();

    // Kill the thread and the Runnable
    UE_LOG(LogTemp, Log, TEXT("Kill the thread. "));
    Thread->Kill();
    Thread->WaitForCompletion();

    UE_LOG(LogTemp, Log, TEXT("Delete the thread. "));
    delete Thread;
    UE_LOG(LogTemp, Log, TEXT("Set Thread to NULL. "));
    Thread = NULL;

    UE_LOG(LogTemp, Log, TEXT("Delete the Client. "));
    delete Client;
    Client = NULL;

    UE_LOG(LogTemp, Log, TEXT("Delete the Runnable. "));
    delete Runnable;
    Runnable = NULL;
}

// Run
void FROSBridgeHandler::Run()
{
    // Do not use this function.
    check(false);
}

// Update for each frame / substep
void FROSBridgeHandler::Render()
{
    while (!QueueTask.IsEmpty())
    {
        UE_LOG(LogTemp, Log, TEXT("Queue not empty. "));
        FRenderTask* RenderTask;
        QueueTask.Dequeue(RenderTask);
        UE_LOG(LogTemp, Log, TEXT("Dequeue Done. "));

        FROSBridgeMsg* Msg = RenderTask->Message;
        RenderTask->Subscriber->CallBack(Msg);
        UE_LOG(LogTemp, Log, TEXT("Callback Done. "));

        delete RenderTask;
        // delete Msg;
    }

    LockArrayService.Lock(); // lock mutex, when access ArrayService
    for (int i = 0; i < ArrayService.Num(); i++)
    {
        if (ArrayService[i]->bIsResponsed)
        {
            ArrayService[i]->Client->CallBack(
                ArrayService[i]->Request,
                ArrayService[i]->Response
            ); 
            ArrayService[i]->bIsProcessed = true; 
        }
    }
    for (int i = 0; i < ArrayService.Num(); i++)
    {
        if (ArrayService[i]->bIsProcessed)
        {
            ArrayService.RemoveAt(i);
            i--;
        }
    }
    LockArrayService.Unlock(); // unlock mutex of ArrayService
}

void FROSBridgeHandler::PublishServiceResponse(FString Service, FString ID,
    TSharedPtr<FROSBridgeSrv::SrvResponse> Response)
{
    FString MsgToSend = FROSBridgeSrv::ServiceResponse(Service, ID, Response); 
    Client->Send(MsgToSend); 
}

void FROSBridgeHandler::PublishMsg(FString Topic, FROSBridgeMsg* Msg)
{
    FString MsgToSend = FROSBridgeMsg::Publish(Topic, Msg);
    Client->Send(MsgToSend);
}

void FROSBridgeHandler::CallService(FROSBridgeSrvClient* SrvClient, 
                                    TSharedPtr<FROSBridgeSrv::SrvRequest> Request,
                                    TSharedPtr<FROSBridgeSrv::SrvResponse> Response)
{
    FString Name = SrvClient->GetName(); 
    FString ID = FString::FromInt(FMath::RandRange(0, 10000000)); 
    LockArrayService.Lock(); // lock mutex, when access ArrayService
    ArrayService.Add(new FServiceTask(SrvClient, Name, ID, Request, Response));
    LockArrayService.Unlock(); 
    CallServiceImpl(Name, Request, ID); 
}

void FROSBridgeHandler::CallServiceImpl(FString Name, TSharedPtr<FROSBridgeSrv::SrvRequest> Request, FString ID)
{
    FString MsgToSend = FROSBridgeSrv::CallService(Name, Request.Get(), ID);
    UE_LOG(LogTemp, Log, TEXT("Call Service Impl: Message to Send: %s"), *MsgToSend);
    Client->Send(MsgToSend); 
}

