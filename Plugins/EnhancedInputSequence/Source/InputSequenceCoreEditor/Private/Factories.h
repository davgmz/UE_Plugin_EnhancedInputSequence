// Copyright 2023 Pentangle Studio under EULA https://www.unrealengine.com/en-US/eula/unreal

#pragma once

#include "Factories/Factory.h"
#include "Factories.generated.h"

//------------------------------------------------------
// UFactory_InputSequence
//------------------------------------------------------

UCLASS()
class UFactory_InputSequence : public UFactory
{
	GENERATED_UCLASS_BODY()

public:

	virtual UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn, FName CallingContext) override;
	virtual FText GetDisplayName() const override;
	virtual uint32 GetMenuCategories() const override;
};

//------------------------------------------------------
// UFactory_RequestKey
//------------------------------------------------------

UCLASS()
class UFactory_RequestKey : public UFactory
{
	GENERATED_UCLASS_BODY()

public:

	virtual UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn, FName CallingContext) override;
	virtual FText GetDisplayName() const override;
	virtual uint32 GetMenuCategories() const override;
};