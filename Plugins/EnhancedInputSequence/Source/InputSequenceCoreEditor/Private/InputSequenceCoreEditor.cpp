// Copyright 2023 Pentangle Studio under EULA https://www.unrealengine.com/en-US/eula/unreal

#include "InputSequenceCoreEditor.h"

#include "Kismet/KismetTextLibrary.h"

#include "Factory_InputSequence.h"
#include "AssetTypeActions_InputSequence.h"
#include "AssetTypeCategories.h"
#include "InputSequence.h"

#include "InputAction.h"

#include "ISGraph.h"
#include "ISGraphSchema.h"
#include "ISGraphNodes.h"
#include "EdGraphNode_Comment.h"

#include "ConnectionDrawingPolicy.h"
#include "SLevelOfDetailBranchNode.h"
#include "SPinTypeSelector.h"
#include "SGraphActionMenu.h"
#include "KismetPins/SGraphPinExec.h"
#include "Widgets/Layout/SGridPanel.h"
#include "Widgets/Input/SNumericEntryBox.h"

#include "EdGraphUtilities.h"
#include "GraphEditorActions.h"
#include "Settings/EditorStyleSettings.h"
#include "HAL/PlatformApplicationMisc.h"
#include "Framework/Commands/GenericCommands.h"

#include "IAssetTools.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Brushes/SlateImageBrush.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Styling/SlateTypes.h"
#include <Styling/StyleColors.h>

#include "UObject/ObjectSaveContext.h"

#define LOCTEXT_NAMESPACE "FInputSequenceCoreEditor"

const float InputFocusRadius = 2.f;
const float InputFocusThickness = 0.5f;
const FVector2D Icon8x8(8.0f, 8.0f);
const auto extention = TEXT(".png");

const FSpinBoxStyle spinBoxStyle = FSpinBoxStyle()
.SetBackgroundBrush(FSlateRoundedBoxBrush(FStyleColors::Input, InputFocusRadius, FStyleColors::InputOutline, InputFocusThickness))
.SetHoveredBackgroundBrush(FSlateRoundedBoxBrush(FStyleColors::Input, InputFocusRadius, FStyleColors::Hover, InputFocusThickness))
.SetActiveBackgroundBrush(FSlateRoundedBoxBrush(FStyleColors::Input, InputFocusRadius, FStyleColors::Primary, InputFocusThickness))
.SetActiveFillBrush(FSlateRoundedBoxBrush(FStyleColors::Primary, InputFocusRadius, FLinearColor::Transparent, InputFocusThickness))
.SetHoveredFillBrush(FSlateRoundedBoxBrush(FStyleColors::Hover, InputFocusRadius, FLinearColor::Transparent, InputFocusThickness))
.SetInactiveFillBrush(FSlateRoundedBoxBrush(FStyleColors::Dropdown, InputFocusRadius, FLinearColor::Transparent, InputFocusThickness))
.SetArrowsImage(FSlateNoResource())
.SetForegroundColor(FStyleColors::ForegroundHover)
.SetTextPadding(FMargin(3.f, 1.f, 3.f, 1.f))
.SetInsetPadding(FMargin(0.5));

const FCheckBoxStyle checkBoxStyle = FCheckBoxStyle()
.SetCheckBoxType(ESlateCheckBoxType::CheckBox)
.SetUncheckedImage(FSlateImageBrush(FPaths::EngineContentDir() / TEXT("Slate") / "Common/CheckBox" + extention, Icon8x8))
.SetUncheckedHoveredImage(FSlateImageBrush(FPaths::EngineContentDir() / TEXT("Slate") / "Common/CheckBox" + extention, Icon8x8))
.SetUncheckedPressedImage(FSlateImageBrush(FPaths::EngineContentDir() / TEXT("Slate") / "Common/CheckBox_Hovered" + extention, Icon8x8, FLinearColor(0.5f, 0.5f, 0.5f)))
.SetCheckedImage(FSlateImageBrush(FPaths::EngineContentDir() / TEXT("Slate") / "Common/CheckBox_Checked_Hovered" + extention, Icon8x8))
.SetCheckedHoveredImage(FSlateImageBrush(FPaths::EngineContentDir() / TEXT("Slate") / "Common/CheckBox_Checked_Hovered" + extention, Icon8x8, FLinearColor(0.5f, 0.5f, 0.5f)))
.SetCheckedPressedImage(FSlateImageBrush(FPaths::EngineContentDir() / TEXT("Slate") / "Common/CheckBox_Checked" + extention, Icon8x8))
.SetUndeterminedImage(FSlateImageBrush(FPaths::EngineContentDir() / TEXT("Slate") / "Common/CheckBox_Undetermined" + extention, Icon8x8))
.SetUndeterminedHoveredImage(FSlateImageBrush(FPaths::EngineContentDir() / TEXT("Slate") / "Common/CheckBox_Undetermined_Hovered" + extention, Icon8x8))
.SetUndeterminedPressedImage(FSlateImageBrush(FPaths::EngineContentDir() / TEXT("Slate") / "Common/CheckBox_Undetermined_Hovered" + extention, Icon8x8, FLinearColor(0.5f, 0.5f, 0.5f)));

const FSlateFontInfo pinFontInfo(FCoreStyle::GetDefaultFont(), 6, "Regular");
const FSlateFontInfo inputEventFontInfo(FCoreStyle::GetDefaultFont(), 8, "Regular");
const FSlateFontInfo inputEventFontInfo_Selected(FCoreStyle::GetDefaultFont(), 8, "Bold");
const float padding = 2;

const FName NAME_NoBorder("NoBorder");
const FString trueFlag("1");

template<class T>
void GetAssetsFromAssetRegistry(TArray<FAssetData>& outAssetDatas)
{
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");

	FARFilter filter;
	filter.ClassPaths.Add(T::StaticClass()->GetClassPathName());
	filter.bRecursiveClasses = true;
	filter.bRecursivePaths = true;

	AssetRegistryModule.Get().GetAssets(filter, outAssetDatas);
}

void AddPinToDynamicNode(UEdGraphNode* node, FName category, FName pinName, const UEdGraphNode::FCreatePinParams& params, TObjectPtr<UInputAction> inputAction)
{
	UEdGraphPin* graphPin = node->CreatePin(EGPD_Output, category, pinName, params);
	graphPin->DefaultObject = inputAction;

	node->Modify();

	if (UISGraphNode_Dynamic* dynamicGraphNode = Cast<UISGraphNode_Dynamic>(node))
	{
		dynamicGraphNode->OnUpdateGraphNode.ExecuteIfBound();
	}
}

//------------------------------------------------------
// SISGraphNode_Dynamic
//------------------------------------------------------

class SToolTip_Dummy : public SLeafWidget, public IToolTip
{
public:

	SLATE_BEGIN_ARGS(SToolTip_Dummy) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs) {}

	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override { return LayerId; }
	virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D::ZeroVector; }

	virtual TSharedRef<class SWidget> AsWidget() { return SNullWidget::NullWidget; }
	virtual TSharedRef<SWidget> GetContentWidget() { return SNullWidget::NullWidget; }
	virtual void SetContentWidget(const TSharedRef<SWidget>& InContentWidget) override {}
	virtual bool IsEmpty() const override { return false; }
	virtual bool IsInteractive() const { return false; }
	virtual void OnOpening() override {}
	virtual void OnClosed() override {}
};

//------------------------------------------------------
// SISGraphNode_Dynamic
//------------------------------------------------------

class SISGraphNode_Dynamic : public SGraphNode
{
public:
	SLATE_BEGIN_ARGS(SISGraphNode_Dynamic) {}
	SLATE_END_ARGS();

	void Construct(const FArguments& InArgs, UEdGraphNode* InNode);

	virtual ~SISGraphNode_Dynamic();

protected:

	virtual void CreatePinWidgets() override;
};

void SISGraphNode_Dynamic::Construct(const FArguments& InArgs, UEdGraphNode* InNode)
{
	SetCursor(EMouseCursor::CardinalCross);

	GraphNode = InNode;

	if (UISGraphNode_Dynamic* dynamicGraphNode = Cast<UISGraphNode_Dynamic>(InNode))
	{
		dynamicGraphNode->OnUpdateGraphNode.BindLambda([&]() { UpdateGraphNode(); });
	}

	UpdateGraphNode();
}

SISGraphNode_Dynamic::~SISGraphNode_Dynamic()
{
	if (UISGraphNode_Dynamic* dynamicGraphNode = Cast<UISGraphNode_Dynamic>(GraphNode))
	{
		dynamicGraphNode->OnUpdateGraphNode.Unbind();
	}
}

void SISGraphNode_Dynamic::CreatePinWidgets()
{
	// Create Pin widgets for each of the pins.
	for (int32 PinIndex = 0; PinIndex < GraphNode->Pins.Num(); ++PinIndex)
	{
		UEdGraphPin* CurPin = GraphNode->Pins[PinIndex];

		if (CurPin->PinType.PinCategory == UISGraphSchema::PC_Input) continue;

		if (!ensureMsgf(CurPin->GetOuter() == GraphNode
			, TEXT("Graph node ('%s' - %s) has an invalid %s pin: '%s'; (with a bad %s outer: '%s'); skiping creation of a widget for this pin.")
			, *GraphNode->GetNodeTitle(ENodeTitleType::ListView).ToString()
			, *GraphNode->GetPathName()
			, (CurPin->Direction == EEdGraphPinDirection::EGPD_Input) ? TEXT("input") : TEXT("output")
			, CurPin->PinFriendlyName.IsEmpty() ? *CurPin->PinName.ToString() : *CurPin->PinFriendlyName.ToString()
			, CurPin->GetOuter() ? *CurPin->GetOuter()->GetClass()->GetName() : TEXT("UNKNOWN")
			, CurPin->GetOuter() ? *CurPin->GetOuter()->GetPathName() : TEXT("NULL")))
		{
			continue;
		}

		CreateStandardPinWidget(CurPin);
	}
}

//------------------------------------------------------
// SISParameterMenu
//------------------------------------------------------

class SISParameterMenu : public SCompoundWidget
{
public:
	DECLARE_DELEGATE_RetVal_OneParam(FText, FGetSectionTitle, int32);

	SLATE_BEGIN_ARGS(SISParameterMenu) : _AutoExpandMenu(false) {}

		SLATE_ARGUMENT(bool, AutoExpandMenu)
		SLATE_EVENT(FGetSectionTitle, OnGetSectionTitle)
	SLATE_END_ARGS();

	void Construct(const FArguments& InArgs)
	{
		this->bAutoExpandMenu = InArgs._AutoExpandMenu;

		ChildSlot
			[
				SNew(SBorder).BorderImage(FAppStyle::GetBrush("Menu.Background")).Padding(5)
					[
						SNew(SBox)
							.MinDesiredWidth(300)
							.MaxDesiredHeight(700) // Set max desired height to prevent flickering bug for menu larger than screen
							[
								SAssignNew(GraphMenu, SGraphActionMenu)
									.OnCollectStaticSections(this, &SISParameterMenu::OnCollectStaticSections)
									.OnGetSectionTitle(this, &SISParameterMenu::OnGetSectionTitle)
									.OnCollectAllActions(this, &SISParameterMenu::CollectAllActions)
									.OnActionSelected(this, &SISParameterMenu::OnActionSelected)
									.SortItemsRecursively(false)
									.AlphaSortItems(false)
									.AutoExpandActionMenu(bAutoExpandMenu)
									.ShowFilterTextBox(true)
							]
					]
			];
	}

	TSharedPtr<SEditableTextBox> GetSearchBox() { return GraphMenu->GetFilterTextBox(); }

protected:

	virtual void OnCollectStaticSections(TArray<int32>& StaticSectionIDs) = 0;

	virtual FText OnGetSectionTitle(int32 InSectionID) = 0;

	virtual void CollectAllActions(FGraphActionListBuilderBase& OutAllActions) = 0;

	virtual void OnActionSelected(const TArray<TSharedPtr<FEdGraphSchemaAction>>& SelectedActions, ESelectInfo::Type InSelectionType) = 0;

private:

	bool bAutoExpandMenu;

	TSharedPtr<SGraphActionMenu> GraphMenu;
};

//------------------------------------------------------
// SISParameterMenu_Pin
//------------------------------------------------------

class SISParameterMenu_Pin : public SISParameterMenu
{
public:
	SLATE_BEGIN_ARGS(SISParameterMenu_Pin)
		: _AutoExpandMenu(false)
		{}
		//~ Begin Required Args
		SLATE_ARGUMENT(UEdGraphNode*, Node)
		//~ End Required Args
		SLATE_ARGUMENT(bool, AutoExpandMenu)
	SLATE_END_ARGS();

	void Construct(const FArguments& InArgs)
	{
		this->Node = InArgs._Node;

		SISParameterMenu::FArguments SuperArgs;
		SuperArgs._AutoExpandMenu = InArgs._AutoExpandMenu;
		SISParameterMenu::Construct(SuperArgs);
	}

protected:

	virtual void OnCollectStaticSections(TArray<int32>& StaticSectionIDs) override { StaticSectionIDs.Add(1); }

	virtual FText OnGetSectionTitle(int32 InSectionID) override { return LOCTEXT("SISParameterMenu_Pin_SectionTitle", "Input Actions"); }

	virtual void CollectAllActions(FGraphActionListBuilderBase& OutAllActions) override
	{
		TArray<FAssetData> assetDatas;
		GetAssetsFromAssetRegistry<UInputAction>(assetDatas);

		TSet<UInputAction*> alreadyAdded;

		for (const UEdGraphPin* pin : Node->Pins)
		{
			if (UInputAction* inputAction = Cast<UInputAction>(pin->DefaultObject))
			{
				alreadyAdded.FindOrAdd(inputAction);
			}
		}

		TArray<TSharedPtr<FEdGraphSchemaAction>> schemaActions;

		for (const FAssetData& assetData : assetDatas)
		{
			if (UInputAction* inputAction = Cast<UInputAction>(assetData.GetAsset()))
			{
				const FName inputActionName = inputAction->GetFName();

				if (!alreadyAdded.Contains(inputAction))
				{
					const FText tooltip = FText::Format(LOCTEXT("SISParameterMenu_Pin_Tooltip", "Add {0} for {1}"), FText::FromString("Action pin"), FText::FromName(inputActionName));

					TSharedPtr<FISGraphSchemaAction_AddPin> schemaAction(
						new FISGraphSchemaAction_AddPin(
							FText::GetEmpty()
							, FText::FromName(inputActionName)
							, tooltip
							, 0
							, 1
						)
					);

					schemaAction->InputAction = inputAction;
					schemaActions.Add(schemaAction);
				}
			}
		}

		for (TSharedPtr<FEdGraphSchemaAction> schemaAction : schemaActions)
		{
			TSharedPtr<FISGraphSchemaAction_AddPin> addPinAction = StaticCastSharedPtr<FISGraphSchemaAction_AddPin>(schemaAction);
			OutAllActions.AddAction(schemaAction);
		}
	}

	virtual void OnActionSelected(const TArray<TSharedPtr<FEdGraphSchemaAction>>& SelectedActions, ESelectInfo::Type InSelectionType) override
	{
		if (InSelectionType == ESelectInfo::OnMouseClick || InSelectionType == ESelectInfo::OnKeyPress)
		{
			for (int32 ActionIndex = 0; ActionIndex < SelectedActions.Num(); ActionIndex++)
			{
				FSlateApplication::Get().DismissAllMenus();
				SelectedActions[ActionIndex]->PerformAction(Node->GetGraph(), Node->FindPin(NAME_None, EGPD_Input), FVector2D::ZeroVector);
			}
		}
	}

private:

	UEdGraphNode* Node;
};

//------------------------------------------------------
// SGraphPin_Input
//------------------------------------------------------

class SGraphPin_Input : public SGridPanel
{
public:
	SLATE_BEGIN_ARGS(SGraphPin_Input) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, UEdGraphPin* InPin);

protected:

	FReply OnClicked_RemovePin() const;

	FReply OnClicked_Started() const;

	FReply OnClicked_Triggered() const;

	FReply OnClicked_Completed() const;

	void SetTriggerEvent(const ETriggerEvent triggerEvent) const;

	FSlateColor GetTriggerEventForegroundColor_Started() const { return GetTriggerEventForegroundColor(ButtonStartedPtr, FString::FromInt(static_cast<int32>(ETriggerEvent::Started))); }

	FSlateColor GetTriggerEventForegroundColor_Triggered() const { return GetTriggerEventForegroundColor(ButtonTriggeredPtr, FString::FromInt(static_cast<int32>(ETriggerEvent::Triggered))); }

	FSlateColor GetTriggerEventForegroundColor_Completed() const { return GetTriggerEventForegroundColor(ButtonCompletedPtr, FString::FromInt(static_cast<int32>(ETriggerEvent::Completed))); }

	FSlateColor GetTriggerEventForegroundColor(const TSharedPtr<SButton>& buttonPtr, const FString& triggerEventString) const;

	FSlateFontInfo GetTriggerEventFont_Started() const { return GetTriggerEventFont(ButtonStartedPtr, FString::FromInt(static_cast<int32>(ETriggerEvent::Started))); }

	FSlateFontInfo GetTriggerEventFont_Triggered() const { return GetTriggerEventFont(ButtonTriggeredPtr, FString::FromInt(static_cast<int32>(ETriggerEvent::Triggered))); }

	FSlateFontInfo GetTriggerEventFont_Completed() const { return GetTriggerEventFont(ButtonCompletedPtr, FString::FromInt(static_cast<int32>(ETriggerEvent::Completed))); }

	FSlateFontInfo GetTriggerEventFont(const TSharedPtr<SButton>& buttonPtr, const FString& triggerEventString) const;

	void RequirePreciseMatchStateChanged(ECheckBoxState checkBoxState) const;

	ECheckBoxState RequirePreciseMatch() const;

	void SetWaitTimeValue(const float NewValue) const;

	void SetWaitTimeValueCommited(const float NewValue, ETextCommit::Type CommitType) const { SetWaitTimeValue(NewValue); }

	TOptional<float> GetWaitTimeValue() const;

	FSlateColor GetPinTextColor() const;

	FText GetPinFriendlyName() const;

	TSharedPtr<SButton> ButtonStartedPtr;
	TSharedPtr<SButton> ButtonTriggeredPtr;
	TSharedPtr<SButton> ButtonCompletedPtr;

	UEdGraphPin* PinObject;
};

void SGraphPin_Input::Construct(const FArguments& Args, UEdGraphPin* InPin)
{
	SGridPanel::Construct(SGridPanel::FArguments());

	PinObject = InPin;

	SetRowFill(0, 1);
	SetRowFill(1, 1);
	SetColumnFill(0, 1);
	SetColumnFill(1, 1);
	SetColumnFill(2, 0);

	AddSlot(0, 0).RowSpan(2).VAlign(VAlign_Center).HAlign(HAlign_Center).Padding(2 * padding, padding)
		[
			SNew(STextBlock).Text_Raw(this, &SGraphPin_Input::GetPinFriendlyName).Font(pinFontInfo).ColorAndOpacity(this, &SGraphPin_Input::GetPinTextColor).AutoWrapText(true)
		];

	AddSlot(1, 0).VAlign(VAlign_Center)
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot().FillWidth(1).Padding(padding, padding, 0, 0)
			[
				SAssignNew(ButtonStartedPtr, SButton).ButtonStyle(FAppStyle::Get(), NAME_NoBorder)
					.Cursor(EMouseCursor::Hand)
					.ToolTipText(LOCTEXT("SGraphPin_Input_TooltipText_Started", "Started"))
					.OnClicked_Raw(this, &SGraphPin_Input::OnClicked_Started)
					[
						SNew(STextBlock).Text(FText::FromString("S"))
							.Font_Raw(this, &SGraphPin_Input::GetTriggerEventFont_Started)
							.ColorAndOpacity_Raw(this, &SGraphPin_Input::GetTriggerEventForegroundColor_Started)
					]
			]

			+ SHorizontalBox::Slot().FillWidth(1).Padding(padding, padding, 0, 0)
			[
				SAssignNew(ButtonTriggeredPtr, SButton).ButtonStyle(FAppStyle::Get(), NAME_NoBorder)
					.Cursor(EMouseCursor::Hand)
					.ToolTipText(LOCTEXT("SGraphPin_Input_TooltipText_Triggered", "Triggered"))
					.OnClicked_Raw(this, &SGraphPin_Input::OnClicked_Triggered)
					[
						SNew(STextBlock).Text(FText::FromString("T"))
							.Font_Raw(this, &SGraphPin_Input::GetTriggerEventFont_Triggered)
							.ColorAndOpacity_Raw(this, &SGraphPin_Input::GetTriggerEventForegroundColor_Triggered)
					]
			]

			+ SHorizontalBox::Slot().FillWidth(1).Padding(padding, padding, 0, 0)
			[
				SAssignNew(ButtonCompletedPtr, SButton).ButtonStyle(FAppStyle::Get(), NAME_NoBorder)
					.Cursor(EMouseCursor::Hand)
					.ToolTipText(LOCTEXT("SGraphPin_Input_TooltipText_Completed", "Completed"))
					.OnClicked_Raw(this, &SGraphPin_Input::OnClicked_Completed)
					[
						SNew(STextBlock).Text(FText::FromString("C"))
							.Font_Raw(this, &SGraphPin_Input::GetTriggerEventFont_Completed)
							.ColorAndOpacity_Raw(this, &SGraphPin_Input::GetTriggerEventForegroundColor_Completed)
					]
			]
		];

	AddSlot(1, 1).VAlign(VAlign_Center)
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot().AutoWidth().Padding(2 * padding, 0, 0, padding)
			[
				SNew(SCheckBox).Style(&checkBoxStyle)
					.Cursor(EMouseCursor::Hand)
					.ToolTipText(LOCTEXT("SGraphPin_Input_TooltipText_RequirePreciseMatch", "Require precise match"))
					.OnCheckStateChanged(this, &SGraphPin_Input::RequirePreciseMatchStateChanged)
					.IsChecked_Raw(this, &SGraphPin_Input::RequirePreciseMatch)
			]

			+ SHorizontalBox::Slot().FillWidth(1).Padding(padding, 0, 0, padding)
			[
				SNew(SNumericEntryBox<float>).SpinBoxStyle(&spinBoxStyle)
					.Font(pinFontInfo)
					.ToolTipText(LOCTEXT("SGraphPin_Input_TooltipText_WaitTime", "Wait for (sec)"))
					.AllowSpin(true)
					.MinValue(0)
					.MaxValue(10)
					.MinSliderValue(0)
					.MaxSliderValue(10)
					.Delta(0.01f)
					.Value(this, &SGraphPin_Input::GetWaitTimeValue)
					.OnValueChanged(this, &SGraphPin_Input::SetWaitTimeValue)
					.OnValueCommitted(this, &SGraphPin_Input::SetWaitTimeValueCommited)
			]
		];

	AddSlot(2, 0).RowSpan(2).VAlign(VAlign_Center).Padding(padding)
		[
			SNew(SButton)
				.ButtonStyle(FAppStyle::Get(), NAME_NoBorder)
				.ForegroundColor(FLinearColor::White)
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				.Cursor(EMouseCursor::Hand)
				.ToolTipText(LOCTEXT("SGraphPin_Input_TooltipText_RemovePin", "Click to remove pin"))
				.OnClicked_Raw(this, &SGraphPin_Input::OnClicked_RemovePin)
				[
					SNew(SImage).Image(FAppStyle::GetBrush("Cross"))
				]
		];

	SetToolTip(SNew(SToolTip_Dummy));

	if (PinObject)
	{
		if (UInputSequence* inputSequence = PinObject->GetOwningNode()->GetGraph()->GetTypedOuter<UInputSequence>())
		{
			PinObject->PinToolTip = inputSequence->GetRequirePreciseMatchDefaultValue() ? trueFlag : "";
		}
	}
}

FReply SGraphPin_Input::OnClicked_RemovePin() const
{
	if (UEdGraphPin* FromPin = PinObject)
	{
		UEdGraphNode* FromNode = FromPin->GetOwningNode();

		UEdGraph* ParentGraph = FromNode->GetGraph();

		if (FromPin->HasAnyConnections())
		{
			const FScopedTransaction Transaction(LOCTEXT("K2_DeleteNode", "Delete Node"));

			ParentGraph->Modify();

			UEdGraphNode* linkedGraphNode = FromPin->LinkedTo[0]->GetOwningNode();

			linkedGraphNode->Modify();
			linkedGraphNode->DestroyNode();
		}

		{
			const FScopedTransaction Transaction(LOCTEXT("K2_DeletePin", "Delete Pin"));

			FromNode->RemovePin(FromPin);

			FromNode->Modify();

			if (UISGraphNode_Dynamic* dynamicGraphNode = Cast<UISGraphNode_Dynamic>(FromNode))
			{
				dynamicGraphNode->OnUpdateGraphNode.ExecuteIfBound();
			}
		}
	}

	return FReply::Handled();
}

FReply SGraphPin_Input::OnClicked_Started() const
{
	SetTriggerEvent(ETriggerEvent::Started);
	return FReply::Handled();
}

FReply SGraphPin_Input::OnClicked_Triggered() const
{
	SetTriggerEvent(ETriggerEvent::Triggered);
	return FReply::Handled();
}

FReply SGraphPin_Input::OnClicked_Completed() const
{
	SetTriggerEvent(ETriggerEvent::Completed);
	return FReply::Handled();
}

void SGraphPin_Input::SetTriggerEvent(const ETriggerEvent triggerEvent) const
{
	if (PinObject)
	{
		if (UISGraphNode_Input* inputGraphNode = Cast<UISGraphNode_Input>(PinObject->GetOwningNode()))
		{
			const FScopedTransaction Transaction(LOCTEXT("SGraphPin_Input_SetTriggerEvent", "Set Trigger Event"));

			PinObject->Modify();

			const ETriggerEvent currentTriggerEvent = PinObject->DefaultValue.IsEmpty() ? ETriggerEvent::None : static_cast<ETriggerEvent>(FCString::Atoi(*PinObject->DefaultValue));

			PinObject->DefaultValue = FString::FromInt(static_cast<int32>(currentTriggerEvent != triggerEvent ? triggerEvent : ETriggerEvent::None));
		}
	}
}

FSlateColor SGraphPin_Input::GetTriggerEventForegroundColor(const TSharedPtr<SButton>& buttonPtr, const FString& triggerEventString) const
{
	const bool isHovered = buttonPtr->IsHovered();

	FLinearColor color = isHovered ? FLinearColor::White : FLinearColor(1, 1, 1, 0.2f);

	if (PinObject && PinObject->DefaultValue == triggerEventString)
	{
		color = FLinearColor::Green;
	}

	return color;
}

FSlateFontInfo SGraphPin_Input::GetTriggerEventFont(const TSharedPtr<SButton>& buttonPtr, const FString& triggerEventString) const
{
	if (PinObject)
	{
		return PinObject->DefaultValue == triggerEventString ? inputEventFontInfo_Selected : inputEventFontInfo;
	}

	return inputEventFontInfo;
}

void SGraphPin_Input::RequirePreciseMatchStateChanged(ECheckBoxState checkBoxState) const
{
	if (PinObject)
	{
		PinObject->PinToolTip = (checkBoxState == ECheckBoxState::Checked ? trueFlag : "");
	}
}

ECheckBoxState SGraphPin_Input::RequirePreciseMatch() const
{
	if (PinObject)
	{
		if (PinObject->PinToolTip == trueFlag)
		{
			return ECheckBoxState::Checked;
		}
	}

	return ECheckBoxState::Unchecked;
}

void SGraphPin_Input::SetWaitTimeValue(const float NewValue) const
{
	if (PinObject)
	{
		const float PrevValue = PinObject->DefaultTextValue.IsEmpty() ? 0.f : FCString::Atof(*PinObject->DefaultTextValue.ToString());
		
		if (NewValue != PrevValue)
		{
			PinObject->DefaultTextValue = FText::FromString(FString::SanitizeFloat(NewValue, 2));
		}
	}
}

TOptional<float> SGraphPin_Input::GetWaitTimeValue() const
{
	if (PinObject)
	{
		return FMath::RoundHalfToEven(PinObject->DefaultTextValue.IsEmpty() ? 0.f : FCString::Atof(*PinObject->DefaultTextValue.ToString()));
	}

	return 0.f;
}

FSlateColor SGraphPin_Input::GetPinTextColor() const { return PinObject && PinObject->DefaultObject.IsResolved() && PinObject->DefaultObject.operator bool() ? FLinearColor::White : FLinearColor::Red; }

FText SGraphPin_Input::GetPinFriendlyName() const { return PinObject && PinObject->DefaultObject ? FText::FromString(PinObject->DefaultObject->GetName()) : LOCTEXT("SGraphPin_Input_PinFriendlyName", "???"); }

//------------------------------------------------------
// SISGraphNode_Input
//------------------------------------------------------

class SISGraphNode_Input : public SISGraphNode_Dynamic
{
protected:

	virtual void CreateBelowPinControls(TSharedPtr<SVerticalBox> MainBox) override;

	TSharedRef<SWidget> OnGetAddButtonMenuContent();

	void OverrideResetTimeStateChanged(ECheckBoxState checkBoxState) const;

	bool HasOverrideResetTime() const;

	ECheckBoxState OverrideResetTime() const;

	void SetResetTimeValue(const float NewValue) const;

	void SetResetTimeValueCommited(const float NewValue, ETextCommit::Type CommitType) const { SetResetTimeValue(NewValue); }

	TOptional<float> GetResetTimeValue() const;

protected:

	TSharedPtr<SComboButton> AddButton;
};

void SISGraphNode_Input::CreateBelowPinControls(TSharedPtr<SVerticalBox> MainBox)
{
	TSharedPtr<SVerticalBox> innerVerticalBox = SNew(SVerticalBox);

	MainBox->AddSlot().AutoHeight()[innerVerticalBox.ToSharedRef()];

	// Create Pin widgets for each of the pins.
	for (int32 PinIndex = 0; PinIndex < GraphNode->Pins.Num(); ++PinIndex)
	{
		UEdGraphPin* CurPin = GraphNode->Pins[PinIndex];

		if (CurPin->PinType.PinCategory == UISGraphSchema::PC_Input)
		{
			innerVerticalBox->AddSlot().AutoHeight()[SNew(SGraphPin_Input, CurPin)];
		}
	}

	MainBox->AddSlot().AutoHeight()
		[
			SNew(SGridPanel).FillColumn(0, 0).FillColumn(1, 1).FillColumn(2, 0)

				+ SGridPanel::Slot(0, 0).Padding(2 * padding, padding)
				[
					SNew(SCheckBox).Style(&checkBoxStyle)
						.Cursor(EMouseCursor::Hand)
						.ToolTipText(LOCTEXT("SISGraphNode_Input_TooltipText_OverrideResetTime", "Override Reset Time"))
						.OnCheckStateChanged(this, &SISGraphNode_Input::OverrideResetTimeStateChanged)
						.IsChecked_Raw(this, &SISGraphNode_Input::OverrideResetTime)
				]

				+ SGridPanel::Slot(1, 0).Padding(padding)
				[
					SNew(SBox).MinDesiredWidth(150)
						[
							SNew(SNumericEntryBox<float>).SpinBoxStyle(&spinBoxStyle)
								.IsEnabled_Raw(this, &SISGraphNode_Input::HasOverrideResetTime)
								.Font(pinFontInfo)
								.ToolTipText(LOCTEXT("SISGraphNode_Input_TooltipText_ResetTime", "Reset Time (sec)"))
								.AllowSpin(true)
								.MinValue(0)
								.MaxValue(10)
								.MinSliderValue(0)
								.MaxSliderValue(10)
								.Delta(0.0f)
								.Value(this, &SISGraphNode_Input::GetResetTimeValue)
								.OnValueChanged(this, &SISGraphNode_Input::SetResetTimeValue)
								.OnValueCommitted(this, &SISGraphNode_Input::SetResetTimeValueCommited)
						]
				]

				+ SGridPanel::Slot(2, 0).Padding(padding)
				[
					SAssignNew(AddButton, SComboButton)
						.HasDownArrow(false)
						.ButtonStyle(FAppStyle::Get(), NAME_NoBorder)
						.ForegroundColor(FLinearColor::White)
						.HAlign(HAlign_Center)
						.VAlign(VAlign_Center)
						.Cursor(EMouseCursor::Hand)
						.ToolTipText(LOCTEXT("SISGraphNode_Input_Add_ToolTipText", "Click to add new pin"))
						.OnGetMenuContent(this, &SISGraphNode_Input::OnGetAddButtonMenuContent)
						.ButtonContent()
						[
							SNew(SImage).Image(FAppStyle::Get().GetBrush("Icons.PlusCircle"))
						]
				]
		];
}

TSharedRef<SWidget> SISGraphNode_Input::OnGetAddButtonMenuContent()
{
	TSharedRef<SISParameterMenu_Pin> MenuWidget = SNew(SISParameterMenu_Pin).Node(GraphNode);

	AddButton->SetMenuContentWidgetToFocus(MenuWidget->GetSearchBox());

	return MenuWidget;
}

void SISGraphNode_Input::OverrideResetTimeStateChanged(ECheckBoxState checkBoxState) const
{
	if (UISGraphNode_Input* dynamicGraphNode = Cast<UISGraphNode_Input>(GraphNode))
	{
		dynamicGraphNode->bOverrideResetTime = checkBoxState == ECheckBoxState::Checked;
	}
}

bool SISGraphNode_Input::HasOverrideResetTime() const
{
	if (UISGraphNode_Input* dynamicGraphNode = Cast<UISGraphNode_Input>(GraphNode))
	{
		return dynamicGraphNode->bOverrideResetTime;
	}

	return false;
}

ECheckBoxState SISGraphNode_Input::OverrideResetTime() const
{
	if (UISGraphNode_Input* dynamicGraphNode = Cast<UISGraphNode_Input>(GraphNode))
	{
		if (dynamicGraphNode->bOverrideResetTime)
		{
			return ECheckBoxState::Checked;
		}
	}

	return ECheckBoxState::Unchecked;
}

void SISGraphNode_Input::SetResetTimeValue(const float NewValue) const
{
	if (UISGraphNode_Input* dynamicGraphNode = Cast<UISGraphNode_Input>(GraphNode))
	{
		if (dynamicGraphNode->ResetTime != NewValue)
		{
			dynamicGraphNode->ResetTime = NewValue;
		}
	}
}

TOptional<float> SISGraphNode_Input::GetResetTimeValue() const
{
	if (UISGraphNode_Input* dynamicGraphNode = Cast<UISGraphNode_Input>(GraphNode))
	{
		return dynamicGraphNode->ResetTime;
	}

	return 0;
}

//------------------------------------------------------
// SISGraphNode_Hub
//------------------------------------------------------

class SISGraphNode_Hub : public SISGraphNode_Dynamic
{
protected:

	virtual void CreateBelowPinControls(TSharedPtr<SVerticalBox> MainBox) override;

	FReply OnClickedAddButton();
};

void SISGraphNode_Hub::CreateBelowPinControls(TSharedPtr<SVerticalBox> MainBox)
{
	MainBox->AddSlot().HAlign(HAlign_Right)
		[
			SNew(SButton)
				.ButtonStyle(FAppStyle::Get(), NAME_NoBorder)
				.ForegroundColor(FLinearColor::White)
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				.Cursor(EMouseCursor::Hand)
				.ToolTipText(LOCTEXT("SISGraphNode_Hub_ToolTipText", "Click to add new pin"))
				.OnClicked_Raw(this, &SISGraphNode_Hub::OnClickedAddButton)
				[
					SNew(SImage).Image(FAppStyle::Get().GetBrush("Icons.PlusCircle"))
				]
		];
}

FReply SISGraphNode_Hub::OnClickedAddButton()
{
	const FScopedTransaction Transaction(LOCTEXT("SISGraphNode_Hub_AddPin", "Add Pin"));

	int32 outputPinsCount = 1;
	for (UEdGraphPin* pin : GraphNode->Pins)
	{
		if (pin->Direction == EGPD_Output) outputPinsCount++;
	}

	UEdGraphNode::FCreatePinParams params;
	params.Index = outputPinsCount;

	const FString numberedName = FString::FromInt(outputPinsCount);
	AddPinToDynamicNode(GraphNode, UISGraphSchema::PC_Exec, FName(numberedName), params, nullptr);

	return FReply::Handled();
}

//------------------------------------------------------
// SGraphPin_HubExec
//------------------------------------------------------

class SGraphPin_HubExec : public SGraphPinExec
{
public:
	SLATE_BEGIN_ARGS(SGraphPin_HubExec) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, UEdGraphPin* InPin);

protected:
	
	FReply OnClicked_RemovePin() const;
};

void SGraphPin_HubExec::Construct(const FArguments& Args, UEdGraphPin* InPin)
{
	SGraphPin::FArguments InArgs = SGraphPin::FArguments();

	bUsePinColorForText = InArgs._UsePinColorForText;
	this->SetCursor(EMouseCursor::Default);

	SetVisibility(MakeAttributeSP(this, &SGraphPin_HubExec::GetPinVisiblity));

	GraphPinObj = InPin;
	check(GraphPinObj != NULL);

	const UEdGraphSchema* Schema = GraphPinObj->GetSchema();
	checkf(
		Schema, 
		TEXT("Missing schema for pin: %s with outer: %s of type %s"), 
		*(GraphPinObj->GetName()),
		GraphPinObj->GetOuter() ? *(GraphPinObj->GetOuter()->GetName()) : TEXT("NULL OUTER"), 
		GraphPinObj->GetOuter() ? *(GraphPinObj->GetOuter()->GetClass()->GetName()) : TEXT("NULL OUTER")
	);
	
	// Create the pin icon widget
	TSharedRef<SWidget> PinWidgetRef = SPinTypeSelector::ConstructPinTypeImage(
		MakeAttributeSP(this, &SGraphPin_HubExec::GetPinIcon),
		MakeAttributeSP(this, &SGraphPin_HubExec::GetPinColor),
		MakeAttributeSP(this, &SGraphPin_HubExec::GetSecondaryPinIcon),
		MakeAttributeSP(this, &SGraphPin_HubExec::GetSecondaryPinColor));
	PinImage = PinWidgetRef;

	PinWidgetRef->SetCursor(
		TAttribute<TOptional<EMouseCursor::Type> >::Create(
			TAttribute<TOptional<EMouseCursor::Type> >::FGetter::CreateRaw(this, &SGraphPin_HubExec::GetPinCursor)
		)
	);

	// Create the pin indicator widget (used for watched values)
	TSharedRef<SWidget> PinStatusIndicator =
		SNew(SButton)
		.ButtonStyle(FAppStyle::Get(), NAME_NoBorder)
		.Visibility(this, &SGraphPin_HubExec::GetPinStatusIconVisibility)
		.ContentPadding(0)
		.OnClicked(this, &SGraphPin_HubExec::ClickedOnPinStatusIcon)
		[
			SNew(SImage).Image(this, &SGraphPin_HubExec::GetPinStatusIcon)
		];

	TSharedRef<SWidget> LabelWidget = GetLabelWidget(InArgs._PinLabelStyle);

	// Create the widget used for the pin body (status indicator, label, and value)
	LabelAndValue =
		SNew(SWrapBox)
		.PreferredSize(150.f);

	LabelAndValue->AddSlot()
		.VAlign(VAlign_Center)
		[
			PinStatusIndicator
		];

	LabelAndValue->AddSlot()
		.VAlign(VAlign_Center)
		[
			LabelWidget
		];

	TSharedPtr<SHorizontalBox> PinContent;
	// Output pin
	FullPinHorizontalRowWidget = PinContent = SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		[
			LabelAndValue.ToSharedRef()
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(InArgs._SideToSideMargin, 0, 0, 0)
		[
			PinWidgetRef
		];

	// Set up a hover for pins that is tinted the color of the pin.
	
	SBorder::Construct(SBorder::FArguments().BorderImage(FAppStyle::GetBrush(NAME_NoBorder))
		[
			SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(SLevelOfDetailBranchNode)
						.UseLowDetailSlot(this, &SGraphPin_HubExec::UseLowDetailPinNames)
						.LowDetail()
						[
							SNullWidget::NullWidget
						]
						.HighDetail()
						[
							SNew(SButton)
								.ButtonStyle(FAppStyle::Get(), NAME_NoBorder)
								.ForegroundColor(GetPinColor())
								.HAlign(HAlign_Center)
								.VAlign(VAlign_Center)
								.Cursor(EMouseCursor::Hand)
								.ToolTipText(LOCTEXT("SGraphPin_HubExec_TooltipText_RemovePin", "Click to remove pin"))
								.OnClicked_Raw(this, &SGraphPin_HubExec::OnClicked_RemovePin)
								[
									SNew(SImage).Image(FAppStyle::GetBrush("Cross"))
								]
						]
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(SBorder)
						.BorderImage(this, &SGraphPin_HubExec::GetPinBorder)
						.BorderBackgroundColor(this, &SGraphPin_HubExec::GetHighlightColor)
						.OnMouseButtonDown(this, &SGraphPin_HubExec::OnPinNameMouseDown)
						[
							SNew(SBorder)
								.BorderImage(CachedImg_Pin_DiffOutline)
								.BorderBackgroundColor(this, &SGraphPin_HubExec::GetPinDiffColor)
								[
									SNew(SLevelOfDetailBranchNode)
										.UseLowDetailSlot(this, &SGraphPin_HubExec::UseLowDetailPinNames)
										.LowDetail()
										[
											//@TODO: Try creating a pin-colored line replacement that doesn't measure text / call delegates but still renders
											PinWidgetRef
										]
										.HighDetail()
										[
											PinContent.ToSharedRef()
										]
								]
						]
				]
		]
	);

	SetToolTip(SNew(SToolTip_Dummy));

	CachePinIcons();
}

FReply SGraphPin_HubExec::OnClicked_RemovePin() const
{
	if (UEdGraphPin* FromPin = GetPinObj())
	{
		if (UEdGraphNode* FromNode = FromPin->GetOwningNode())
		{
			const FScopedTransaction Transaction(LOCTEXT("K2_DeletePin", "Delete Pin"));

			int nextAfterRemovedIndex = FromNode->Pins.IndexOfByKey(FromPin) + 1;

			if (FromNode->Pins.IsValidIndex(nextAfterRemovedIndex))
			{
				for (size_t i = nextAfterRemovedIndex; i < FromNode->Pins.Num(); i++)
				{
					UEdGraphPin* pin = FromNode->Pins[i];

					if (pin->Direction == EGPD_Output && pin->PinType.PinCategory == UISGraphSchema::PC_Exec)
					{
						pin->PinName = FName(FString::FromInt(i - 1));
					}
				}
			}

			FromNode->RemovePin(FromPin);

			FromNode->Modify();

			if (UISGraphNode_Dynamic* dynamicGraphNode = Cast<UISGraphNode_Dynamic>(FromNode)) dynamicGraphNode->OnUpdateGraphNode.ExecuteIfBound();
		}
	}

	return FReply::Handled();
}

//------------------------------------------------------
// FISGraphNodeFactory
//------------------------------------------------------

TSharedPtr<SGraphNode> FISGraphNodeFactory::CreateNode(UEdGraphNode* InNode) const
{
	if (UISGraphNode_Input* inputGraphNode = Cast<UISGraphNode_Input>(InNode))
	{
		return SNew(SISGraphNode_Input, inputGraphNode);
	}
	else if (UISGraphNode_Hub* hubGraphNode = Cast<UISGraphNode_Hub>(InNode))
	{
		return SNew(SISGraphNode_Hub, hubGraphNode);
	}

	return nullptr;
}

//------------------------------------------------------
// FISGraphPinFactory
//------------------------------------------------------

TSharedPtr<SGraphPin> FISGraphPinFactory::CreatePin(UEdGraphPin* InPin) const
{
	if (InPin->GetSchema()->IsA<UISGraphSchema>())
	{
		if (InPin->PinType.PinCategory == UISGraphSchema::PC_Exec)
		{
			if (InPin->Direction == EEdGraphPinDirection::EGPD_Output && InPin->GetOwningNode() && InPin->GetOwningNode()->IsA<UISGraphNode_Hub>())
			{
				return SNew(SGraphPin_HubExec, InPin);
			}
			else
			{
				TSharedPtr<SGraphPin> sGraphPin = SNew(SGraphPinExec, InPin);
				sGraphPin->SetToolTip(SNew(SToolTip_Dummy));
				return sGraphPin;
			}
		}

		return FGraphPanelPinFactory::CreatePin(InPin);
	}

	return nullptr;
}

//------------------------------------------------------
// FISGraphPinConnectionFactory
//------------------------------------------------------

class FISConnectionDrawingPolicy : public FConnectionDrawingPolicy
{
public:
	FISConnectionDrawingPolicy(int32 InBackLayerID, int32 InFrontLayerID, float ZoomFactor, const FSlateRect& InClippingRect, FSlateWindowElementList& InDrawElements, UEdGraph* InGraphObj)
		: FConnectionDrawingPolicy(InBackLayerID, InFrontLayerID, ZoomFactor, InClippingRect, InDrawElements)
		, GraphObj(InGraphObj)
	{}

	virtual void DetermineWiringStyle(UEdGraphPin* OutputPin, UEdGraphPin* InputPin, /*inout*/ FConnectionParams& Params) override
	{
		FConnectionDrawingPolicy::DetermineWiringStyle(OutputPin, InputPin, Params);

		if (OutputPin->PinType.PinCategory == UISGraphSchema::PC_Exec)
		{
			Params.WireThickness = 4;
		}
		else
		{
			Params.bUserFlag1 = true;
		}

		const bool bDeemphasizeUnhoveredPins = HoveredPins.Num() > 0;
		if (bDeemphasizeUnhoveredPins)
		{
			ApplyHoverDeemphasis(OutputPin, InputPin, /*inout*/ Params.WireThickness, /*inout*/ Params.WireColor);
		}
	}

	virtual void DrawSplineWithArrow(const FVector2D& StartPoint, const FVector2D& EndPoint, const FConnectionParams& Params) override
	{
		DrawConnection(
			WireLayerID,
			StartPoint,
			EndPoint,
			Params);

		// Draw the arrow
		if (ArrowImage != nullptr && Params.bUserFlag1)
		{
			FVector2D ArrowPoint = EndPoint - ArrowRadius;

			FSlateDrawElement::MakeBox(
				DrawElementsList,
				ArrowLayerID,
				FPaintGeometry(ArrowPoint, ArrowImage->ImageSize * ZoomFactor, ZoomFactor),
				ArrowImage,
				ESlateDrawEffect::None,
				Params.WireColor
			);
		}
	}

protected:
	UEdGraph* GraphObj;
	TMap<UEdGraphNode*, int32> NodeWidgetMap;
};

FConnectionDrawingPolicy* FISGraphPinConnectionFactory::CreateConnectionPolicy(const UEdGraphSchema* Schema, int32 InBackLayerID, int32 InFrontLayerID, float ZoomFactor, const class FSlateRect& InClippingRect, class FSlateWindowElementList& InDrawElements, UEdGraph* InGraphObj) const
{
	if (Schema->IsA<UISGraphSchema>())
	{
		return new FISConnectionDrawingPolicy(InBackLayerID, InFrontLayerID, ZoomFactor, InClippingRect, InDrawElements, InGraphObj);;
	}

	return nullptr;
}

//------------------------------------------------------
// FISGraphSchemaAction_NewComment
//------------------------------------------------------

UISGraph::UISGraph(const FObjectInitializer& ObjectInitializer) :Super(ObjectInitializer)
{
	Schema = UISGraphSchema::StaticClass();
}

void UISGraph::PreSave(FObjectPreSaveContext SaveContext)
{
	Super::PreSave(SaveContext);

	if (UInputSequence* inputSequence = GetTypedOuter<UInputSequence>())
	{
		inputSequence->GetEntryStates().Empty();
		inputSequence->GetStates().Empty();

		TMap<FGuid, TObjectPtr<UEdGraphNode>> nodesMapping;

		for (TObjectPtr<UEdGraphNode>& node : Nodes)
		{
			nodesMapping.Add(node->NodeGuid, node);

			if (UISGraphNode_Entry* entryGraphNode = Cast<UISGraphNode_Entry>(node))
			{
				inputSequence->GetEntryStates().Add(entryGraphNode->NodeGuid);

				FSetElementId stateId = inputSequence->GetStates().Emplace(GetTypeHash(entryGraphNode->NodeGuid));
				FISState& state = inputSequence->GetStates()[stateId];
				state.StateGuid = entryGraphNode->NodeGuid;
				state.StateFlags = EFlags_State::IS_ENTRY_STATE;
			}
			else if (UISGraphNode_Reset* resetGraphNode = Cast<UISGraphNode_Reset>(node))
			{
				FSetElementId stateId = inputSequence->GetStates().Emplace(GetTypeHash(resetGraphNode->NodeGuid));
				FISState& state = inputSequence->GetStates()[stateId];
				state.StateGuid = resetGraphNode->NodeGuid;
				state.StateFlags = EFlags_State::IS_RESET_STATE;
			}
			else if (UISGraphNode_Input* inputGraphNode = Cast<UISGraphNode_Input>(node))
			{
				FSetElementId stateId = inputSequence->GetStates().Emplace(GetTypeHash(inputGraphNode->NodeGuid));
				FISState& state = inputSequence->GetStates()[stateId];
				state.StateGuid = inputGraphNode->NodeGuid;

				for (UEdGraphPin* pin : inputGraphNode->Pins)
				{
					if (pin->PinType.PinCategory == UISGraphSchema::PC_Input)
					{
						if (UInputAction* inputAction = Cast<UInputAction>(pin->DefaultObject))
						{
							FISInputActionInfo& inputCheck = state.InputActionInfos.Emplace(FSoftObjectPath(inputAction));
							inputCheck.TriggerEvent = static_cast<ETriggerEvent>(FCString::Atoi(*pin->DefaultValue));
							
							if (pin->PinToolTip == trueFlag)
							{
								inputCheck.InputActionInfoFlags |= EFlags_InputActionInfo::REQUIRE_PRECISE_MATCH;
							}
							
							inputCheck.WaitTime = FMath::RoundHalfToEven(pin->DefaultTextValue.IsEmpty() ? 0.f : FCString::Atof(*pin->DefaultTextValue.ToString()));
						}
					}
				}

				state.EnterEvents.SetNum(inputGraphNode->EnterEvents.Num());
				for (const TObjectPtr<UISEvent>& Event : inputGraphNode->EnterEvents)
				{
					state.EnterEvents.Add(DuplicateObject(Event, InputSequence));
				}

				state.PassEvents.SetNum(inputGraphNode->PassEvents.Num());
				for (const TObjectPtr<UISEvent>& Event : inputGraphNode->PassEvents)
				{
					state.PassEvents.Add(DuplicateObject(Event, InputSequence));
				}

				state.ResetEvents.SetNum(inputGraphNode->ResetEvents.Num());
				for (const TObjectPtr<UISEvent>& Event : inputGraphNode->ResetEvents)
				{
					state.ResetEvents.Add(DuplicateObject(Event, InputSequence));
				}

				state.StateObject = inputGraphNode->StateObject;
				state.StateContext = inputGraphNode->StateContext;

				if (inputGraphNode->bOverrideResetTime)
				{
					state.StateFlags |= EFlags_State::OVERRIDE_HAS_RESET_TIME;
					
					if (state.ResetTime > 0)
					{
						state.StateFlags |= EFlags_State::HAS_RESET_TIME;
					}
				}

				state.ResetTime = inputGraphNode->ResetTime;
			}
			else if (UISGraphNode_Hub* hubGraphNode = Cast<UISGraphNode_Hub>(node))
			{
				// Hub Nodes are skipped
			}
		}

		TQueue<TObjectPtr<UEdGraphNode>> queuedNodes;

		for(const FGuid& entryState : inputSequence->GetEntryStates())
		{
			queuedNodes.Enqueue(nodesMapping[entryState]);
		}

		TSet<TObjectPtr<UEdGraphNode>> processedNodes;

		TObjectPtr<UEdGraphNode> dequeuedNode;

		while (queuedNodes.Dequeue(dequeuedNode))
		{
			processedNodes.FindOrAdd(dequeuedNode);

			dequeuedNode->ForEachNodeDirectlyConnected([&queuedNodes, &processedNodes](UEdGraphNode* node)
				{
					if (!processedNodes.Contains(node))
					{
						queuedNodes.Enqueue(node);
					}
				});
		}
	}
}

//------------------------------------------------------
// FISGraphSchemaAction_NewComment
//------------------------------------------------------

UEdGraphNode* FISGraphSchemaAction_NewComment::PerformAction(class UEdGraph* ParentGraph, UEdGraphPin* FromPin, const FVector2D Location, bool bSelectNewNode/* = true*/)
{
	// Add menu item for creating comment boxes
	UEdGraphNode_Comment* CommentTemplate = NewObject<UEdGraphNode_Comment>();

	FVector2D SpawnLocation = Location;

	CommentTemplate->SetBounds(SelectedNodesBounds);
	SpawnLocation.X = CommentTemplate->NodePosX;
	SpawnLocation.Y = CommentTemplate->NodePosY;

	return FEdGraphSchemaAction_NewNode::SpawnNodeFromTemplate<UEdGraphNode_Comment>(ParentGraph, CommentTemplate, SpawnLocation);
}

//------------------------------------------------------
// FISGraphSchemaAction_NewNode
//------------------------------------------------------

UEdGraphNode* FISGraphSchemaAction_NewNode::PerformAction(class UEdGraph* ParentGraph, UEdGraphPin* FromPin, const FVector2D Location, bool bSelectNewNode)
{
	UEdGraphNode* ResultNode = NULL;

	// If there is a template, we actually use it
	if (NodeTemplate != NULL)
	{
		const FScopedTransaction Transaction(LOCTEXT("K2_AddNode", "Add Node"));
		ParentGraph->Modify();
		if (FromPin)
		{
			FromPin->Modify();
		}

		// set outer to be the graph so it doesn't go away
		NodeTemplate->Rename(NULL, ParentGraph);
		ParentGraph->AddNode(NodeTemplate, true, bSelectNewNode);

		NodeTemplate->CreateNewGuid();
		NodeTemplate->PostPlacedNewNode();
		NodeTemplate->AllocateDefaultPins();
		NodeTemplate->AutowireNewNode(FromPin);

		NodeTemplate->NodePosX = Location.X;
		NodeTemplate->NodePosY = Location.Y;
		NodeTemplate->SnapToGrid(GetDefault<UEditorStyleSettings>()->GridSnapSize);

		ResultNode = NodeTemplate;

		ResultNode->SetFlags(RF_Transactional);
	}

	return ResultNode;
}

void FISGraphSchemaAction_NewNode::AddReferencedObjects(FReferenceCollector& Collector)
{
	FEdGraphSchemaAction::AddReferencedObjects(Collector);

	Collector.AddReferencedObject(NodeTemplate);
}

//------------------------------------------------------
// FISGraphSchemaAction_AddPin
//------------------------------------------------------

UEdGraphNode* FISGraphSchemaAction_AddPin::PerformAction(class UEdGraph* ParentGraph, UEdGraphPin* FromPin, const FVector2D Location, bool bSelectNewNode)
{
	UEdGraphNode* ResultNode = NULL;

	if (InputAction)
	{
		const FScopedTransaction Transaction(LOCTEXT("K2_AddPin", "Add Pin"));

		UEdGraphNode::FCreatePinParams params;
		////// TODO params.Index = CorrectedInputIndex + execPinCount;

		AddPinToDynamicNode(FromPin->GetOwningNode(), UISGraphSchema::PC_Input, FName(FGuid::NewGuid().ToString()), params, InputAction);
	}

	return ResultNode;
}

//------------------------------------------------------
// UISGraphSchema
//------------------------------------------------------

template<class T>
TSharedPtr<T> AddNewActionAs(FGraphContextMenuBuilder& ContextMenuBuilder, const FText& Category, const FText& MenuDesc, const FText& Tooltip, const int32 Grouping = 0)
{
	TSharedPtr<T> Action(new T(Category, MenuDesc, Tooltip, Grouping));
	ContextMenuBuilder.AddAction(Action);
	return Action;
}

const FName UISGraphSchema::PC_Exec("UISGraphSchema_PC_Exec");

const FName UISGraphSchema::PC_Input("UISGraphSchema_PC_Input");

void UISGraphSchema::GetGraphContextActions(FGraphContextMenuBuilder& ContextMenuBuilder) const
{
	{
		// Add Input node
		TSharedPtr<FISGraphSchemaAction_NewNode> Action = AddNewActionAs<FISGraphSchemaAction_NewNode>(ContextMenuBuilder, FText::GetEmpty(), LOCTEXT("AddNode_Input", "Add Input node..."), LOCTEXT("AddNode_Input_Tooltip", "A new Input node"));
		Action->NodeTemplate = NewObject<UISGraphNode_Input>(ContextMenuBuilder.OwnerOfTemporaries);
	}

	{
		// Add Hub node
		TSharedPtr<FISGraphSchemaAction_NewNode> Action = AddNewActionAs<FISGraphSchemaAction_NewNode>(ContextMenuBuilder, FText::GetEmpty(), LOCTEXT("AddNode_Hub", "Add Hub node..."), LOCTEXT("AddNode_Hub_Tooltip", "A new Hub node"));
		Action->NodeTemplate = NewObject<UISGraphNode_Hub>(ContextMenuBuilder.OwnerOfTemporaries);
	}

	{
		// Add Reset node
		TSharedPtr<FISGraphSchemaAction_NewNode> Action = AddNewActionAs<FISGraphSchemaAction_NewNode>(ContextMenuBuilder, FText::GetEmpty(), LOCTEXT("AddNode_Reset", "Add Reset node..."), LOCTEXT("AddNode_Reset_Tooltip", "A new Reset node"));
		Action->NodeTemplate = NewObject<UISGraphNode_Reset>(ContextMenuBuilder.OwnerOfTemporaries);
	}
}

const FPinConnectionResponse UISGraphSchema::CanCreateConnection(const UEdGraphPin* pinA, const UEdGraphPin* pinB) const
{
	if (pinA == nullptr || pinB == nullptr) return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, LOCTEXT("Pin(s)Null", "One or both of pins was null"));

	if (pinA->GetOwningNode() == pinB->GetOwningNode()) return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, LOCTEXT("PinsOfSameNode", "Both pins are on same node"));

	if (pinA->Direction == pinB->Direction) return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, LOCTEXT("PinsOfSameDirection", "Both pins have same direction (both input or both output)"));

	if (pinA->PinType.PinCategory != pinB->PinType.PinCategory) return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, LOCTEXT("PinsCategoryMismatched", "Pins Categories are mismatched (Flow pin should be connected to Flow pin, Input pin - to Input pin)"));

	return FPinConnectionResponse(CONNECT_RESPONSE_BREAK_OTHERS_AB, TEXT(""));
}

void UISGraphSchema::CreateDefaultNodesForGraph(UEdGraph& Graph) const
{
	FGraphNodeCreator<UISGraphNode_Entry> entryGraphNodeCreator(Graph);
	UISGraphNode_Entry* entryGraphNode = entryGraphNodeCreator.CreateNode();
	entryGraphNode->NodePosX = -300;
	entryGraphNodeCreator.Finalize();
	SetNodeMetaData(entryGraphNode, FNodeMetadata::DefaultGraphNode);
}

TSharedPtr<FEdGraphSchemaAction> UISGraphSchema::GetCreateCommentAction() const
{
	return TSharedPtr<FEdGraphSchemaAction>(static_cast<FEdGraphSchemaAction*>(new FISGraphSchemaAction_NewComment));
}

//------------------------------------------------------
// UISGraphNode_Entry
//------------------------------------------------------

void UISGraphNode_Entry::AllocateDefaultPins()
{
	CreatePin(EGPD_Output, UISGraphSchema::PC_Exec, NAME_None);
}

FText UISGraphNode_Entry::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return LOCTEXT("UISGraphNode_Entry_NodeTitle", "Entry");
}

FLinearColor UISGraphNode_Entry::GetNodeTitleColor() const { return FLinearColor::Green; }

FText UISGraphNode_Entry::GetTooltipText() const
{
	return LOCTEXT("UISGraphNode_Entry_TooltipText", "This is Entry node of Input sequence...");
}

//------------------------------------------------------
// UISGraphNode_Input
//------------------------------------------------------

UISGraphNode_Input::UISGraphNode_Input(const FObjectInitializer& ObjectInitializer) :Super(ObjectInitializer)
{
	ResetTime = 0;
	bOverrideResetTime = 0;
}

void UISGraphNode_Input::AllocateDefaultPins()
{
	CreatePin(EGPD_Input, UISGraphSchema::PC_Exec, NAME_None);
	CreatePin(EGPD_Output, UISGraphSchema::PC_Exec, NAME_None);
}

FText UISGraphNode_Input::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return LOCTEXT("UISGraphNode_Input_NodeTitle", "Input");
}

FLinearColor UISGraphNode_Input::GetNodeTitleColor() const { return FLinearColor::Blue; }

FText UISGraphNode_Input::GetTooltipText() const
{
	return LOCTEXT("UISGraphNode_Input_TooltipText", "This is Input node of Input sequence...");
}

//------------------------------------------------------
// UISGraphNode_Hub
//------------------------------------------------------

void UISGraphNode_Hub::AllocateDefaultPins()
{
	CreatePin(EGPD_Input, UISGraphSchema::PC_Exec, NAME_None);
	CreatePin(EGPD_Output, UISGraphSchema::PC_Exec, "1");
}

FText UISGraphNode_Hub::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return LOCTEXT("UISGraphNode_Hub_NodeTitle", "Hub");
}

FLinearColor UISGraphNode_Hub::GetNodeTitleColor() const { return FLinearColor::Green; }

FText UISGraphNode_Hub::GetTooltipText() const
{
	return LOCTEXT("UISGraphNode_Hub_TooltipText", "This is Hub node of Input sequence...");
}

//------------------------------------------------------
// UISGraphNode_Reset
//------------------------------------------------------

void UISGraphNode_Reset::AllocateDefaultPins()
{
	CreatePin(EGPD_Input, UISGraphSchema::PC_Exec, NAME_None);
}

FText UISGraphNode_Reset::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return LOCTEXT("UISGraphNode_Reset_NodeTitle", "Reset");
}

FLinearColor UISGraphNode_Reset::GetNodeTitleColor() const { return FLinearColor::Green; }

FText UISGraphNode_Reset::GetTooltipText() const
{
	return LOCTEXT("UISGraphNode_Reset_TooltipText", "This is Reset node of Input sequence...");
}

//------------------------------------------------------
// FInputSequenceEditor
//------------------------------------------------------

class FISEditor : public FEditorUndoClient, public FAssetEditorToolkit
{
public:

	static const FName AppIdentifier;
	static const FName DetailsTabId;
	static const FName GraphTabId;

	void InitISEditor(const EToolkitMode::Type Mode, const TSharedPtr< class IToolkitHost >& InitToolkitHost, UInputSequence* inputSequence);

	virtual void RegisterTabSpawners(const TSharedRef<FTabManager>& TabManager) override;
	virtual void UnregisterTabSpawners(const TSharedRef<FTabManager>& TabManager) override;

	virtual FLinearColor GetWorldCentricTabColorScale() const override { return FLinearColor::White; }

	virtual FName GetToolkitFName() const override { return FName("ISEditor"); }
	virtual FText GetBaseToolkitName() const override { return NSLOCTEXT("FISEditor", "BaseToolkitName", "Input Sequence Editor"); }
	virtual FString GetWorldCentricTabPrefix() const override { return "ISEditor"; }

protected:

	TSharedRef<SDockTab> SpawnTab_DetailsTab(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnTab_GraphTab(const FSpawnTabArgs& Args);

	void CreateCommandList();

	void OnSelectionChanged(const TSet<UObject*>& selectedNodes);

	void OnNodeTitleCommitted(const FText& NewText, ETextCommit::Type CommitInfo, UEdGraphNode* NodeBeingChanged);

	//~ Begin FEditorUndoClient Interface
	virtual void PostUndo(bool bSuccess) override;
	virtual void PostRedo(bool bSuccess) override;
	// End of FEditorUndoClient

	FGraphPanelSelectionSet GetSelectedNodes() const;

	void SelectAllNodes();

	bool CanSelectAllNodes() const { return true; }

	void DeleteSelectedNodes();

	bool CanDeleteNodes() const;

	void CopySelectedNodes();

	bool CanCopyNodes() const;

	void DeleteSelectedDuplicatableNodes();

	void CutSelectedNodes() { CopySelectedNodes(); DeleteSelectedDuplicatableNodes(); }

	bool CanCutNodes() const { return CanCopyNodes() && CanDeleteNodes(); }

	void PasteNodes();

	bool CanPasteNodes() const;

	void DuplicateNodes() { CopySelectedNodes(); PasteNodes(); }

	bool CanDuplicateNodes() const { return CanCopyNodes(); }

	void OnCreateComment();

	bool CanCreateComment() const;

protected:

	UInputSequence* InputSequence;

	TSharedPtr<FUICommandList> GraphEditorCommands;

	TWeakPtr<SGraphEditor> GraphEditorPtr;

	TSharedPtr<IDetailsView> DetailsView;
};

const FName FISEditor::AppIdentifier(TEXT("FISEditor_AppIdentifier"));
const FName FISEditor::DetailsTabId(TEXT("FISEditor_DetailsTab_Id"));
const FName FISEditor::GraphTabId(TEXT("FISEditor_GraphTab_Id"));

void FISEditor::InitISEditor(const EToolkitMode::Type Mode, const TSharedPtr< class IToolkitHost >& InitToolkitHost, UInputSequence* inputSequence)
{
	check(inputSequence != NULL);

	InputSequence = inputSequence;

	InputSequence->SetFlags(RF_Transactional);

	TSharedRef<FTabManager::FLayout> StandaloneDefaultLayout = FTabManager::NewLayout("FISEditor_StandaloneDefaultLayout")
		->AddArea
		(
			FTabManager::NewPrimaryArea()->SetOrientation(Orient_Vertical)
			->Split
			(
				FTabManager::NewStack()
				->SetSizeCoefficient(0.1f)
				->SetHideTabWell(true)
			)
			->Split
			(
				FTabManager::NewSplitter()->SetOrientation(Orient_Horizontal)
				->Split
				(
					FTabManager::NewStack()
					->SetSizeCoefficient(0.3f)
					->AddTab(DetailsTabId, ETabState::OpenedTab)
					->SetHideTabWell(true)
				)
				->Split
				(
					FTabManager::NewStack()
					->SetSizeCoefficient(0.7f)
					->AddTab(GraphTabId, ETabState::OpenedTab)
					->SetHideTabWell(true)
				)
			)
		);

	FAssetEditorToolkit::InitAssetEditor(Mode, InitToolkitHost, AppIdentifier, StandaloneDefaultLayout, true, true, InputSequence);
}

void FISEditor::RegisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager)
{
	WorkspaceMenuCategory = InTabManager->AddLocalWorkspaceMenuCategory(LOCTEXT("WorkspaceMenuCategory", "Input Sequence Editor"));
	TSharedRef<FWorkspaceItem> WorkspaceMenuCategoryRef = WorkspaceMenuCategory.ToSharedRef();

	FAssetEditorToolkit::RegisterTabSpawners(InTabManager);

	InTabManager->RegisterTabSpawner(DetailsTabId, FOnSpawnTab::CreateSP(this, &FISEditor::SpawnTab_DetailsTab))
		.SetDisplayName(LOCTEXT("DetailsTab_DisplayName", "Details"))
		.SetGroup(WorkspaceMenuCategoryRef)
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Details"));

	InTabManager->RegisterTabSpawner(GraphTabId, FOnSpawnTab::CreateSP(this, &FISEditor::SpawnTab_GraphTab))
		.SetDisplayName(LOCTEXT("GraphTab_DisplayName", "Graph"))
		.SetGroup(WorkspaceMenuCategoryRef)
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "GraphEditor.EventGraph_16x"));
}

void FISEditor::UnregisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager)
{
	FAssetEditorToolkit::UnregisterTabSpawners(InTabManager);

	InTabManager->UnregisterTabSpawner(GraphTabId);
	InTabManager->UnregisterTabSpawner(DetailsTabId);
}

TSharedRef<SDockTab> FISEditor::SpawnTab_DetailsTab(const FSpawnTabArgs& Args)
{
	check(Args.GetTabId() == DetailsTabId);

	FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");

	FDetailsViewArgs DetailsViewArgs = FDetailsViewArgs();
	DetailsViewArgs.bUpdatesFromSelection = false;
	DetailsViewArgs.bLockable = false;
	DetailsViewArgs.bAllowSearch = false;
	DetailsViewArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
	DetailsViewArgs.bHideSelectionTip = true;

	DetailsView = PropertyEditorModule.CreateDetailView(DetailsViewArgs);
	DetailsView->SetObject(InputSequence);

	return SNew(SDockTab).Label(LOCTEXT("DetailsTab_Label", "Details"))[DetailsView.ToSharedRef()];
}

TSharedRef<SDockTab> FISEditor::SpawnTab_GraphTab(const FSpawnTabArgs& Args)
{
	check(Args.GetTabId().TabType == GraphTabId);

	check(InputSequence != NULL);

	if (InputSequence->EdGraph == NULL)
	{
		UISGraph* isGraph = NewObject<UISGraph>(InputSequence, NAME_None, RF_Transactional);
		isGraph->SetInputSequence(InputSequence);

		InputSequence->EdGraph = isGraph;
		InputSequence->EdGraph->GetSchema()->CreateDefaultNodesForGraph(*InputSequence->EdGraph);
	}

	check(InputSequence->EdGraph != NULL);

	FGraphAppearanceInfo AppearanceInfo;
	AppearanceInfo.CornerText = LOCTEXT("GraphTab_AppearanceInfo_CornerText", "Input Sequence");

	SGraphEditor::FGraphEditorEvents InEvents;
	InEvents.OnSelectionChanged = SGraphEditor::FOnSelectionChanged::CreateSP(this, &FISEditor::OnSelectionChanged);
	InEvents.OnTextCommitted = FOnNodeTextCommitted::CreateSP(this, &FISEditor::OnNodeTitleCommitted);

	CreateCommandList();

	return SNew(SDockTab)
		.Label(LOCTEXT("GraphTab_Label", "Graph"))
		.TabColorScale(GetTabColorScale())
		[
			SAssignNew(GraphEditorPtr, SGraphEditor)
				.AdditionalCommands(GraphEditorCommands)
				.Appearance(AppearanceInfo)
				.GraphEvents(InEvents)
				.TitleBar(SNew(STextBlock).Text(LOCTEXT("GraphTab_Title", "Input Sequence")).TextStyle(FAppStyle::Get(), TEXT("GraphBreadcrumbButtonText")))
				.GraphToEdit(InputSequence->EdGraph)
		];
}

void FISEditor::CreateCommandList()
{
	if (GraphEditorCommands.IsValid()) return;

	GraphEditorCommands = MakeShareable(new FUICommandList);

	GraphEditorCommands->MapAction(FGenericCommands::Get().SelectAll,
		FExecuteAction::CreateRaw(this, &FISEditor::SelectAllNodes),
		FCanExecuteAction::CreateRaw(this, &FISEditor::CanSelectAllNodes)
	);

	GraphEditorCommands->MapAction(FGenericCommands::Get().Delete,
		FExecuteAction::CreateRaw(this, &FISEditor::DeleteSelectedNodes),
		FCanExecuteAction::CreateRaw(this, &FISEditor::CanDeleteNodes)
	);

	GraphEditorCommands->MapAction(FGenericCommands::Get().Copy,
		FExecuteAction::CreateRaw(this, &FISEditor::CopySelectedNodes),
		FCanExecuteAction::CreateRaw(this, &FISEditor::CanCopyNodes)
	);

	GraphEditorCommands->MapAction(FGenericCommands::Get().Cut,
		FExecuteAction::CreateRaw(this, &FISEditor::CutSelectedNodes),
		FCanExecuteAction::CreateRaw(this, &FISEditor::CanCutNodes)
	);

	GraphEditorCommands->MapAction(FGenericCommands::Get().Paste,
		FExecuteAction::CreateRaw(this, &FISEditor::PasteNodes),
		FCanExecuteAction::CreateRaw(this, &FISEditor::CanPasteNodes)
	);

	GraphEditorCommands->MapAction(FGenericCommands::Get().Duplicate,
		FExecuteAction::CreateRaw(this, &FISEditor::DuplicateNodes),
		FCanExecuteAction::CreateRaw(this, &FISEditor::CanDuplicateNodes)
	);

	GraphEditorCommands->MapAction(
		FGraphEditorCommands::Get().CreateComment,
		FExecuteAction::CreateRaw(this, &FISEditor::OnCreateComment),
		FCanExecuteAction::CreateRaw(this, &FISEditor::CanCreateComment)
	);
}

void FISEditor::OnSelectionChanged(const TSet<UObject*>& selectedNodes)
{
	if (selectedNodes.Num() == 1)
	{
		if (UISGraphNode_Input* inputNode = Cast<UISGraphNode_Input>(*selectedNodes.begin()))
		{
			return DetailsView->SetObject(inputNode);
		}

		if (UEdGraphNode_Comment* commentNode = Cast<UEdGraphNode_Comment>(*selectedNodes.begin()))
		{
			return DetailsView->SetObject(commentNode);
		}
	}

	return DetailsView->SetObject(InputSequence);;
}

void FISEditor::OnNodeTitleCommitted(const FText& NewText, ETextCommit::Type CommitInfo, UEdGraphNode* NodeBeingChanged)
{
	if (NodeBeingChanged)
	{
		const FScopedTransaction Transaction(LOCTEXT("K2_RenameNode", "Rename Node"));
		NodeBeingChanged->Modify();
		NodeBeingChanged->OnRenameNode(NewText.ToString());
	}
}

void FISEditor::PostUndo(bool bSuccess)
{
	if (bSuccess)
	{
		// Clear selection, to avoid holding refs to nodes that go away
		if (TSharedPtr<SGraphEditor> graphEditor = GraphEditorPtr.Pin())
		{
			if (graphEditor.IsValid())
			{
				graphEditor->ClearSelectionSet();
				graphEditor->NotifyGraphChanged();
			}
		}
		FSlateApplication::Get().DismissAllMenus();
	}
}

void FISEditor::PostRedo(bool bSuccess)
{
	if (bSuccess)
	{
		// Clear selection, to avoid holding refs to nodes that go away
		if (TSharedPtr<SGraphEditor> graphEditor = GraphEditorPtr.Pin())
		{
			if (graphEditor.IsValid())
			{
				graphEditor->ClearSelectionSet();
				graphEditor->NotifyGraphChanged();
			}
		}
		FSlateApplication::Get().DismissAllMenus();
	}
}

FGraphPanelSelectionSet FISEditor::GetSelectedNodes() const
{
	FGraphPanelSelectionSet CurrentSelection;

	if (TSharedPtr<SGraphEditor> graphEditor = GraphEditorPtr.Pin())
	{
		if (graphEditor.IsValid())
		{
			CurrentSelection = graphEditor->GetSelectedNodes();
		}
	}

	return CurrentSelection;
}

void FISEditor::SelectAllNodes()
{
	if (TSharedPtr<SGraphEditor> graphEditor = GraphEditorPtr.Pin())
	{
		if (graphEditor.IsValid())
		{
			graphEditor->SelectAllNodes();
		}
	}
}

void FISEditor::DeleteSelectedNodes()
{
	if (TSharedPtr<SGraphEditor> graphEditor = GraphEditorPtr.Pin())
	{
		if (graphEditor.IsValid())
		{
			const FScopedTransaction Transaction(FGenericCommands::Get().Delete->GetDescription());

			graphEditor->GetCurrentGraph()->Modify();

			const FGraphPanelSelectionSet SelectedNodes = graphEditor->GetSelectedNodes();
			graphEditor->ClearSelectionSet();

			for (FGraphPanelSelectionSet::TConstIterator NodeIt(SelectedNodes); NodeIt; ++NodeIt)
			{
				if (UEdGraphNode* Node = Cast<UEdGraphNode>(*NodeIt))
				{
					if (Node->CanUserDeleteNode())
					{
						Node->Modify();
						Node->DestroyNode();
					}
				}
			}
		}
	}
}

bool FISEditor::CanDeleteNodes() const
{
	const FGraphPanelSelectionSet SelectedNodes = GetSelectedNodes();
	for (FGraphPanelSelectionSet::TConstIterator SelectedIter(SelectedNodes); SelectedIter; ++SelectedIter)
	{
		UEdGraphNode* Node = Cast<UEdGraphNode>(*SelectedIter);
		if (Node && Node->CanUserDeleteNode()) return true;
	}

	return false;
}

void FISEditor::CopySelectedNodes()
{
	FGraphPanelSelectionSet SelectedNodes = GetSelectedNodes();

	for (FGraphPanelSelectionSet::TConstIterator SelectedIter(SelectedNodes); SelectedIter; ++SelectedIter)
	{
		if (UEdGraphNode* Node = Cast<UEdGraphNode>(*SelectedIter))
		{
			Node->PrepareForCopying();
		}
	}

	FString ExportedText;
	FEdGraphUtilities::ExportNodesToText(SelectedNodes, ExportedText);
	FPlatformApplicationMisc::ClipboardCopy(*ExportedText);
}

bool FISEditor::CanCopyNodes() const
{
	const FGraphPanelSelectionSet SelectedNodes = GetSelectedNodes();
	for (FGraphPanelSelectionSet::TConstIterator SelectedIter(SelectedNodes); SelectedIter; ++SelectedIter)
	{
		UEdGraphNode* Node = Cast<UEdGraphNode>(*SelectedIter);
		if (Node && Node->CanDuplicateNode()) return true;
	}

	return false;
}

void FISEditor::DeleteSelectedDuplicatableNodes()
{
	if (TSharedPtr<SGraphEditor> graphEditor = GraphEditorPtr.Pin())
	{
		if (graphEditor.IsValid())
		{
			const FGraphPanelSelectionSet OldSelectedNodes = graphEditor->GetSelectedNodes();
			graphEditor->ClearSelectionSet();

			for (FGraphPanelSelectionSet::TConstIterator SelectedIter(OldSelectedNodes); SelectedIter; ++SelectedIter)
			{
				UEdGraphNode* Node = Cast<UEdGraphNode>(*SelectedIter);
				if (Node && Node->CanDuplicateNode())
				{
					graphEditor->SetNodeSelection(Node, true);
				}
			}

			DeleteSelectedNodes();

			graphEditor->ClearSelectionSet();

			for (FGraphPanelSelectionSet::TConstIterator SelectedIter(OldSelectedNodes); SelectedIter; ++SelectedIter)
			{
				if (UEdGraphNode* Node = Cast<UEdGraphNode>(*SelectedIter))
				{
					graphEditor->SetNodeSelection(Node, true);
				}
			}
		}
	}
}

void FISEditor::PasteNodes()
{
	if (TSharedPtr<SGraphEditor> graphEditor = GraphEditorPtr.Pin())
	{
		if (graphEditor.IsValid())
		{
			FVector2D Location = graphEditor->GetPasteLocation();

			UEdGraph* EdGraph = graphEditor->GetCurrentGraph();

			// Undo/Redo support
			const FScopedTransaction Transaction(FGenericCommands::Get().Paste->GetDescription());

			EdGraph->Modify();

			// Clear the selection set (newly pasted stuff will be selected)
			graphEditor->ClearSelectionSet();

			// Grab the text to paste from the clipboard.
			FString TextToImport;
			FPlatformApplicationMisc::ClipboardPaste(TextToImport);

			// Import the nodes
			TSet<UEdGraphNode*> PastedNodes;
			FEdGraphUtilities::ImportNodesFromText(EdGraph, TextToImport, /*out*/ PastedNodes);

			//Average position of nodes so we can move them while still maintaining relative distances to each other
			FVector2D AvgNodePosition(0.0f, 0.0f);

			for (TSet<UEdGraphNode*>::TIterator It(PastedNodes); It; ++It)
			{
				UEdGraphNode* Node = *It;
				AvgNodePosition.X += Node->NodePosX;
				AvgNodePosition.Y += Node->NodePosY;
			}

			if (PastedNodes.Num() > 0)
			{
				float InvNumNodes = 1.0f / float(PastedNodes.Num());
				AvgNodePosition.X *= InvNumNodes;
				AvgNodePosition.Y *= InvNumNodes;
			}

			for (TSet<UEdGraphNode*>::TIterator It(PastedNodes); It; ++It)
			{
				UEdGraphNode* Node = *It;

				// Select the newly pasted stuff
				graphEditor->SetNodeSelection(Node, true);

				Node->NodePosX = (Node->NodePosX - AvgNodePosition.X) + Location.X;
				Node->NodePosY = (Node->NodePosY - AvgNodePosition.Y) + Location.Y;

				Node->SnapToGrid(GetDefault<UEditorStyleSettings>()->GridSnapSize);

				// Give new node a different Guid from the old one
				Node->CreateNewGuid();
			}

			EdGraph->NotifyGraphChanged();

			InputSequence->PostEditChange();
			InputSequence->MarkPackageDirty();
		}
	}
}

bool FISEditor::CanPasteNodes() const
{
	if (TSharedPtr<SGraphEditor> graphEditor = GraphEditorPtr.Pin())
	{
		if (graphEditor.IsValid())
		{
			FString ClipboardContent;
			FPlatformApplicationMisc::ClipboardPaste(ClipboardContent);

			return FEdGraphUtilities::CanImportNodesFromText(graphEditor->GetCurrentGraph(), ClipboardContent);
		}
	}

	return false;
}

void FISEditor::OnCreateComment()
{
	if (TSharedPtr<SGraphEditor> graphEditor = GraphEditorPtr.Pin())
	{
		if (graphEditor.IsValid())
		{
			TSharedPtr<FEdGraphSchemaAction> Action = graphEditor->GetCurrentGraph()->GetSchema()->GetCreateCommentAction();
			TSharedPtr<FISGraphSchemaAction_NewComment> newCommentAction = StaticCastSharedPtr<FISGraphSchemaAction_NewComment>(Action);

			if (newCommentAction.IsValid())
			{
				graphEditor->GetBoundsForSelectedNodes(newCommentAction->SelectedNodesBounds, 50);
				newCommentAction->PerformAction(graphEditor->GetCurrentGraph(), nullptr, FVector2D());
			}
		}
	}
}

bool FISEditor::CanCreateComment() const
{
	const FGraphPanelSelectionSet SelectedNodes = GetSelectedNodes();
	return SelectedNodes.Num() > 0;
}

//------------------------------------------------------
// FAssetTypeActions_InputSequence
//------------------------------------------------------

FText FAssetTypeActions_InputSequence::GetName() const { return LOCTEXT("FAssetTypeActions_InputSequence_Name", "Input Sequence"); }

UClass* FAssetTypeActions_InputSequence::GetSupportedClass() const { return UInputSequence::StaticClass(); }

void FAssetTypeActions_InputSequence::OpenAssetEditor(const TArray<UObject*>& InObjects, TSharedPtr<IToolkitHost> EditWithinLevelEditor)
{
	EToolkitMode::Type Mode = EditWithinLevelEditor.IsValid() ? EToolkitMode::WorldCentric : EToolkitMode::Standalone;

	for (TArray<UObject*>::TConstIterator ObjIt = InObjects.CreateConstIterator(); ObjIt; ++ObjIt)
	{
		if (UInputSequence* inputSequence = Cast<UInputSequence>(*ObjIt))
		{
			TSharedRef<FISEditor> NewEditor(new FISEditor());
			NewEditor->InitISEditor(Mode, EditWithinLevelEditor, inputSequence);
		}
	}
}

uint32 FAssetTypeActions_InputSequence::GetCategories() { return EAssetTypeCategories::Misc; }

//------------------------------------------------------
// UFactory_InputSequence
//------------------------------------------------------

UFactory_InputSequence::UFactory_InputSequence(const FObjectInitializer& ObjectInitializer) :Super(ObjectInitializer)
{
	bCreateNew = true;
	bEditAfterNew = true;
	SupportedClass = UInputSequence::StaticClass();
}

UObject* UFactory_InputSequence::FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn, FName CallingContext)
{
	return NewObject<UInputSequence>(InParent, InClass, InName, Flags);
}

FText UFactory_InputSequence::GetDisplayName() const { return LOCTEXT("UFactory_InputSequence_DisplayName", "Input Sequence"); }

uint32 UFactory_InputSequence::GetMenuCategories() const { return EAssetTypeCategories::Misc; }

//------------------------------------------------------
// FInputSequenceCoreEditor
//------------------------------------------------------

const FName AssetToolsModuleName("AssetTools");

void FInputSequenceCoreEditor::StartupModule()
{
	RegisteredAssetTypeActions.Add(MakeShared<FAssetTypeActions_InputSequence>());

	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>(AssetToolsModuleName).Get();
	for (TSharedPtr<FAssetTypeActions_Base>& registeredAssetTypeAction : RegisteredAssetTypeActions)
	{
		if (registeredAssetTypeAction.IsValid()) AssetTools.RegisterAssetTypeActions(registeredAssetTypeAction.ToSharedRef());
	}

	ISGraphNodeFactory = MakeShareable(new FISGraphNodeFactory());
	FEdGraphUtilities::RegisterVisualNodeFactory(ISGraphNodeFactory);

	ISGraphPinFactory = MakeShareable(new FISGraphPinFactory());
	FEdGraphUtilities::RegisterVisualPinFactory(ISGraphPinFactory);

	ISGraphPinConnectionFactory = MakeShareable(new FISGraphPinConnectionFactory());
	FEdGraphUtilities::RegisterVisualPinConnectionFactory(ISGraphPinConnectionFactory);
}

void FInputSequenceCoreEditor::ShutdownModule()
{
	FEdGraphUtilities::UnregisterVisualPinConnectionFactory(ISGraphPinConnectionFactory);
	ISGraphPinConnectionFactory.Reset();

	FEdGraphUtilities::UnregisterVisualPinFactory(ISGraphPinFactory);
	ISGraphPinFactory.Reset();

	FEdGraphUtilities::UnregisterVisualNodeFactory(ISGraphNodeFactory);
	ISGraphNodeFactory.Reset();

	if (FModuleManager::Get().IsModuleLoaded(AssetToolsModuleName))
	{
		IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>(AssetToolsModuleName).Get();
		for (TSharedPtr<FAssetTypeActions_Base>& registeredAssetTypeAction : RegisteredAssetTypeActions)
		{
			if (registeredAssetTypeAction.IsValid()) AssetTools.UnregisterAssetTypeActions(registeredAssetTypeAction.ToSharedRef());
		}
	}

	RegisteredAssetTypeActions.Empty();
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FInputSequenceCoreEditor, InputSequenceCoreEditor)